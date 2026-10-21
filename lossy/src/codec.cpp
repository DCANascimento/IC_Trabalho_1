#include "codec.hpp"

#include "bitstream.hpp"
#include "bytes.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>

// Compressed file format, version 2 of the codec:
//
//   "LSY5"          magic: identifies the format and its version
//   u16 channels
//   u32 sampleRate
//   u16 bitsPerSample
//   u32 numFrames   samples per channel
//   u8  transform   0 = DCT, 1 = MDCT
//   u16 blockSize   N
//   f32 step        quantization step
//   then a bit stream with, for each block:
//     ms            1 bit, only for stereo: 1 = the two channels are Mid and Side
//     for each channel:
//       last        number of coded coefficients (log2(N) + 1 bits); the
//                   coefficients from 'last' to N-1 are all zero
//       last Rice codes of zigzag(quantized coefficient)
//
// Each Rice parameter k is not stored: encoder and decoder both compute it from
// the magnitudes of neighbouring coefficients that are already decoded (see
// contextRiceParameter), so it adapts to every coefficient at no cost.
//
// Mid/Side is an orthonormal rotation of the two channels' coefficients:
//   M = (L + R) / sqrt(2),  S = (L - R) / sqrt(2)
// Because it is orthonormal, the quantization error (and so the SNR) is the
// same in both representations; the encoder just picks the one that needs fewer bits.
//
// Samples are scaled so that a 16-bit sample is the unit (step = 16 means the
// same thing for 8, 16 or 24-bit input).
//
// Block b starts at sample b*N - overlap, where overlap = inputSize - N
// (0 for the DCT, N for the MDCT). Samples before the start or after the end of
// the audio are taken as zero. With overlap, one extra block is needed so that
// the last samples get both of their halves.

namespace
{

const char MAGIC[] = "LSY5";

double sampleUnit(int bitsPerSample)
{
    return std::ldexp(1.0, bitsPerSample - 16);   // 2^(bits - 16)
}

size_t countBlocks(size_t numFrames, size_t n, size_t overlap)
{
    return (numFrames + n - 1) / n + (overlap > 0 ? 1 : 0);
}

// Bits needed to write any value from 0 to n.
int bitsFor(size_t n)
{
    int bits = 0;
    while ((size_t(1) << bits) <= n)
        bits++;
    return bits;
}

// Copies 'count' samples starting at 'start', scaled to 16-bit units.
// Positions outside the audio are filled with zeros.
void gatherSamples(const std::vector<int32_t>& channel, ptrdiff_t start, size_t count,
                   double unit, double* out)
{
    for (size_t i = 0; i < count; i++)
    {
        const ptrdiff_t t = start + ptrdiff_t(i);
        out[i] = (t >= 0 && size_t(t) < channel.size()) ? channel[t] / unit : 0.0;
    }
}

// |x| from zigzag(x).
uint32_t magnitude(uint32_t zigzag)
{
    return (zigzag >> 1) + (zigzag & 1);
}

// Rice parameter for coefficient i, predicted from magnitudes the decoder already
// knows: the 4 previous coefficients of this block and the 3 nearest ones in the
// previous block of the same channel (the one at the same frequency counts twice).
// s / 8 is a weighted mean of |x|; the mean zigzag value is about 2|x| = s / 4,
// and the best Rice parameter for geometric-like data is about log2(mean):
// the largest k with 2^k <= s / 4.
int contextRiceParameter(const uint32_t* mag, const uint32_t* prevMag, size_t i, size_t n)
{
    uint64_t s = 2 * uint64_t(prevMag[i]);
    if (i > 0)
        s += prevMag[i - 1];
    if (i + 1 < n)
        s += prevMag[i + 1];
    for (size_t j = 1; j <= 4 && j <= i; j++)
        s += mag[i - j];

    int k = 0;
    while (k < MAX_RICE_PARAMETER && (uint64_t(1) << (k + 3)) <= s)
        k++;
    return k;
}

// Rotates two coefficient vectors in place: (a, b) -> ((a + b) / sqrt(2), (a - b) / sqrt(2)).
// The rotation is its own inverse: it turns L/R into M/S and M/S back into L/R.
void midSide(std::vector<double>& a, std::vector<double>& b)
{
    const double r = std::sqrt(0.5);
    for (size_t k = 0; k < a.size(); k++)
    {
        const double x = a[k];
        a[k] = r * (x + b[k]);
        b[k] = r * (x - b[k]);
    }
}

// One channel of one block, quantized and ready to be written.
struct ChannelCode
{
    std::vector<uint32_t> values;   // zigzag of the quantized coefficients
    std::vector<uint32_t> mag;      // their magnitudes (context for the next coefficients)
    std::vector<uint8_t> riceK;     // Rice parameter of each coefficient
    size_t last = 0;                // values[last..N) are all zero
    uint64_t bits = 0;              // size of the coded channel, in bits
};

// Uniform quantization (keep only round(X / step)), then the Rice parameter of
// every coefficient and the exact size of the coded channel.
void quantizeChannel(const std::vector<double>& coeffs, double step, int lastBits,
                     const std::vector<uint32_t>& prevMag, ChannelCode& code)
{
    const size_t n = coeffs.size();
    code.values.resize(n);
    code.mag.resize(n);
    code.riceK.resize(n);
    code.last = 0;
    for (size_t i = 0; i < n; i++)
    {
        code.values[i] = zigzagEncode(int32_t(std::lround(coeffs[i] / step)));
        code.mag[i] = magnitude(code.values[i]);
        if (code.values[i] != 0)
            code.last = i + 1;
    }

    // The trailing zeros (usually the highest frequencies) are not coded.
    code.bits = lastBits;
    for (size_t i = 0; i < code.last; i++)
    {
        code.riceK[i] = uint8_t(contextRiceParameter(code.mag.data(), prevMag.data(), i, n));
        code.bits += riceLength(code.values[i], code.riceK[i]);
    }
}

void writeChannel(const ChannelCode& code, int lastBits, BitWriter& bits)
{
    bits.write(uint32_t(code.last), lastBits);
    for (size_t i = 0; i < code.last; i++)
        bits.writeRice(code.values[i], code.riceK[i]);
}

// Zeroth-order entropy of the quantized values, in bits per value: the lower
// bound for coding them one by one with a single fixed code. Context modelling
// (a different code for each coefficient) can go below it.
double entropy(const std::unordered_map<uint32_t, size_t>& histogram, size_t total)
{
    double bits = 0;
    for (const auto& entry : histogram)
    {
        const double p = double(entry.second) / total;
        bits -= p * std::log2(p);
    }
    return bits;
}

} // namespace

bool encodeFile(const WavAudio& audio, const std::string& filename,
                const EncoderParams& params)
{
    const size_t channels = audio.numChannels();
    const size_t numFrames = audio.numFrames();
    const size_t n = params.blockSize;
    const double unit = sampleUnit(audio.bitsPerSample);
    const int lastBits = bitsFor(n);

    auto transform = makeTransform(params.transform, n);
    const size_t overlap = transform->inputSize() - n;
    const size_t numBlocks = countBlocks(numFrames, n, overlap);

    ByteWriter header;
    header.putTag(MAGIC);
    header.put16(uint16_t(channels));
    header.put32(audio.sampleRate);
    header.put16(audio.bitsPerSample);
    header.put32(uint32_t(numFrames));
    header.put8(uint8_t(params.transform));
    header.put16(uint16_t(n));
    header.putFloat(float(params.step));

    const double step = float(params.step);   // use exactly the value the decoder will read
    const bool stereo = channels == 2;
    std::vector<double> input(transform->inputSize());
    std::vector<std::vector<double>> coeffs(channels, std::vector<double>(n));
    std::vector<ChannelCode> codes(channels);   // L/R (or, for mono/multichannel, the channels)
    std::vector<ChannelCode> msCodes(2);        // M/S alternative, stereo only
    // Magnitudes of the previous block, as coded, for the Rice parameter contexts.
    std::vector<std::vector<uint32_t>> prevMag(channels, std::vector<uint32_t>(n, 0));
    std::unordered_map<uint32_t, size_t> histogram;   // only filled with -v
    size_t msBlocks = 0;
    BitWriter bits;

    for (size_t b = 0; b < numBlocks; b++)
    {
        const ptrdiff_t start = ptrdiff_t(b * n) - ptrdiff_t(overlap);

        for (size_t c = 0; c < channels; c++)
        {
            gatherSamples(audio.samples[c], start, input.size(), unit, input.data());
            transform->forward(input.data(), coeffs[c].data());
            quantizeChannel(coeffs[c], step, lastBits, prevMag[c], codes[c]);
        }

        // Stereo: also try Mid/Side and keep whichever needs fewer bits. The
        // distortion is the same, so this is a pure rate decision.
        const std::vector<ChannelCode>* chosen = &codes;
        if (stereo)
        {
            bool useMs = false;
            if (params.midSide)
            {
                midSide(coeffs[0], coeffs[1]);
                quantizeChannel(coeffs[0], step, lastBits, prevMag[0], msCodes[0]);
                quantizeChannel(coeffs[1], step, lastBits, prevMag[1], msCodes[1]);
                useMs = msCodes[0].bits + msCodes[1].bits < codes[0].bits + codes[1].bits;
            }
            bits.write(useMs ? 1 : 0, 1);
            if (useMs)
            {
                chosen = &msCodes;
                msBlocks++;
            }
        }

        for (size_t c = 0; c < channels; c++)
        {
            const ChannelCode& code = (*chosen)[c];
            writeChannel(code, lastBits, bits);
            prevMag[c] = code.mag;
            if (params.verbose)
                for (uint32_t v : code.values)
                    histogram[v]++;
        }
    }

    bits.flush();

    std::vector<uint8_t>& out = header.bytes;
    out.insert(out.end(), bits.bytes.begin(), bits.bytes.end());

    if (!writeFile(filename, out))
    {
        std::cerr << filename << ": cannot write file\n";
        return false;
    }

    if (params.verbose)
    {
        const size_t total = numBlocks * n * channels;
        const double seconds = double(numFrames) / audio.sampleRate;
        const double pcmBytes = double(numFrames) * channels * audio.bitsPerSample / 8;
        const double bitsPerCoeff = entropy(histogram, total);

        std::cerr << "Input:       " << channels << " ch, " << audio.sampleRate << " Hz, "
                  << audio.bitsPerSample << " bits, " << seconds << " s\n"
                  << "Transform:   " << (params.transform == TransformType::Mdct ? "MDCT" : "DCT")
                  << ", N = " << n << ", step = " << params.step << '\n';
        if (stereo)
            std::cerr << "Mid/Side:    " << 100.0 * msBlocks / numBlocks << " % of the blocks\n";
        std::cerr
                  << "Zero coeffs: " << 100.0 * histogram[0] / total << " %\n"
                  << "Entropy:     " << bitsPerCoeff << " bits/coeff (ideal "
                  << bitsPerCoeff * total / seconds / 1000 << " kbps)\n"
                  << "Coded:       " << 8.0 * bits.bytes.size() / total << " bits/coeff\n"
                  << "Output:      " << out.size() << " bytes, ratio "
                  << pcmBytes / out.size() << ", "
                  << out.size() * 8 / seconds / 1000 << " kbps\n";
    }

    return true;
}

bool decodeFile(const std::string& filename, WavAudio& audio)
{
    std::vector<uint8_t> bytes;
    if (!readFile(filename, bytes))
    {
        std::cerr << filename << ": cannot read file\n";
        return false;
    }

    ByteReader in(bytes.data(), bytes.size());

    if (!in.tagIs(MAGIC))
    {
        std::cerr << filename << ": not a file produced by this version of wav_lossy_enc\n";
        return false;
    }

    const size_t channels = in.get16();
    audio.sampleRate = in.get32();
    audio.bitsPerSample = in.get16();
    const size_t numFrames = in.get32();
    const uint8_t type = in.get8();
    const size_t n = in.get16();
    const double step = in.getFloat();

    const int sampleBits = audio.bitsPerSample;
    if (!in.ok() || channels == 0 || type > uint8_t(TransformType::Mdct) ||
        !isValidBlockSize(n) ||
        (sampleBits != 8 && sampleBits != 16 && sampleBits != 24 && sampleBits != 32))
    {
        std::cerr << filename << ": invalid header\n";
        return false;
    }

    auto transform = makeTransform(TransformType(type), n);
    const size_t overlap = transform->inputSize() - n;
    const size_t numBlocks = countBlocks(numFrames, n, overlap);
    const int lastBits = bitsFor(n);
    const double unit = sampleUnit(sampleBits);
    const double minValue = -std::ldexp(1.0, sampleBits - 1);
    const double maxValue = std::ldexp(1.0, sampleBits - 1) - 1;

    // Every block of every channel takes at least lastBits bits: reject headers
    // announcing more audio than the file can hold (before allocating memory).
    if (uint64_t(numBlocks) * channels * lastBits > uint64_t(in.remaining()) * 8)
    {
        std::cerr << filename << ": corrupted or truncated file\n";
        return false;
    }

    audio.samples.assign(channels, std::vector<int32_t>(numFrames));

    BitReader bits(in.position(), in.remaining());
    const bool stereo = channels == 2;
    std::vector<std::vector<double>> coeffs(channels, std::vector<double>(n));
    std::vector<uint32_t> mag(n);   // magnitudes of the current block (Rice contexts)
    std::vector<std::vector<uint32_t>> prevMag(channels, std::vector<uint32_t>(n, 0));
    std::vector<double> output(transform->inputSize());
    // Second part of the previous block's output, still waiting for the next block.
    std::vector<std::vector<double>> pending(channels, std::vector<double>(overlap, 0.0));

    for (size_t b = 0; b < numBlocks; b++)
    {
        const ptrdiff_t start = ptrdiff_t(b * n) - ptrdiff_t(overlap);
        const bool useMs = stereo && bits.read(1) == 1;

        for (size_t c = 0; c < channels; c++)
        {
            const size_t last = bits.read(lastBits);
            if (last > n || !bits.ok())
            {
                std::cerr << filename << ": corrupted or truncated file\n";
                return false;
            }

            // Same Rice parameters as the encoder: they only depend on values
            // decoded before. Dequantization: coefficient = index * step.
            for (size_t i = 0; i < last; i++)
            {
                const int k = contextRiceParameter(mag.data(), prevMag[c].data(), i, n);
                const uint32_t v = bits.readRice(k);
                mag[i] = magnitude(v);
                coeffs[c][i] = zigzagDecode(v) * step;
            }
            std::fill(mag.begin() + last, mag.end(), 0);
            std::fill(coeffs[c].begin() + last, coeffs[c].end(), 0.0);
            std::swap(mag, prevMag[c]);
        }

        if (useMs)
            midSide(coeffs[0], coeffs[1]);   // M/S back to L/R

        for (size_t c = 0; c < channels; c++)
        {
            transform->inverse(coeffs[c].data(), output.data());

            // Overlap-add: after this, the first N output samples are final.
            for (size_t i = 0; i < overlap; i++)
                output[i] += pending[c][i];
            std::copy(output.begin() + n, output.end(), pending[c].begin());

            // Back to integers, clipping values outside the sample range.
            for (size_t i = 0; i < n; i++)
            {
                const ptrdiff_t t = start + ptrdiff_t(i);
                if (t >= 0 && size_t(t) < numFrames)
                    audio.samples[c][t] =
                        int32_t(std::clamp(std::round(output[i] * unit), minValue, maxValue));
            }
        }
    }

    if (!bits.ok())
    {
        std::cerr << filename << ": file is truncated\n";
        return false;
    }

    return true;
}
