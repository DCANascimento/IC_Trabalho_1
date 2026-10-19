#include "codec.hpp"

#include "bitstream.hpp"
#include "bytes.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>

// Compressed file format, version 1 of the codec:
//
//   "LSY3"          magic: identifies the format and its version
//   u16 channels
//   u32 sampleRate
//   u16 bitsPerSample
//   u32 numFrames   samples per channel
//   u8  transform   0 = DCT, 1 = MDCT
//   u16 blockSize   N
//   f32 step        quantization step
//   then a bit stream with, for each block, for each channel:
//     last          number of coded coefficients (log2(N) + 1 bits); the
//                   coefficients from 'last' to N-1 are all zero
//     k             Rice parameter (5 bits), only if last > 0
//     last Rice codes of zigzag(quantized coefficient)
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

const char MAGIC[] = "LSY3";
const int RICE_PARAMETER_BITS = 5;

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

// Exact size in bits of the Rice codes of values[0..count) with parameter k.
uint64_t riceCost(const uint32_t* values, size_t count, int k)
{
    uint64_t bits = 0;
    for (size_t i = 0; i < count; i++)
    {
        const uint32_t q = values[i] >> k;
        bits += q < RICE_ESCAPE ? q + 1 + k : RICE_ESCAPE + 32;
    }
    return bits;
}

// For geometric-like data the best Rice parameter is close to log2(mean):
// start there and also try the neighbours.
int bestRiceParameter(const uint32_t* values, size_t count)
{
    uint64_t sum = 0;
    for (size_t i = 0; i < count; i++)
        sum += values[i];

    int guess = 0;   // largest k with 2^k <= mean
    while (guess < MAX_RICE_PARAMETER && (uint64_t(count) << (guess + 1)) <= sum)
        guess++;

    int best = guess;
    uint64_t bestCost = riceCost(values, count, guess);

    for (int k : {guess - 1, guess + 1})
    {
        if (k < 0 || k > MAX_RICE_PARAMETER)
            continue;
        const uint64_t cost = riceCost(values, count, k);
        if (cost < bestCost)
        {
            bestCost = cost;
            best = k;
        }
    }
    return best;
}

// Zeroth-order entropy of the quantized values, in bits per value: the lower
// bound for coding them one by one with a fixed code.
double entropy(const std::unordered_map<int32_t, size_t>& histogram, size_t total)
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
    std::vector<double> input(transform->inputSize());
    std::vector<double> coeffs(n);
    std::vector<int32_t> quantized(n);
    std::vector<uint32_t> values(n);
    std::unordered_map<int32_t, size_t> histogram;   // only filled with -v
    BitWriter bits;

    for (size_t b = 0; b < numBlocks; b++)
    {
        const ptrdiff_t start = ptrdiff_t(b * n) - ptrdiff_t(overlap);

        for (size_t c = 0; c < channels; c++)
        {
            gatherSamples(audio.samples[c], start, input.size(), unit, input.data());
            transform->forward(input.data(), coeffs.data());

            // Uniform quantization: keep only round(X / step).
            for (size_t k = 0; k < n; k++)
                quantized[k] = int32_t(std::lround(coeffs[k] / step));

            if (params.verbose)
                for (size_t k = 0; k < n; k++)
                    histogram[quantized[k]]++;

            // The trailing zeros (usually the highest frequencies) are not coded.
            size_t last = n;
            while (last > 0 && quantized[last - 1] == 0)
                last--;

            bits.write(uint32_t(last), lastBits);
            if (last == 0)
                continue;

            for (size_t k = 0; k < last; k++)
                values[k] = zigzagEncode(quantized[k]);

            const int riceK = bestRiceParameter(values.data(), last);
            bits.write(uint32_t(riceK), RICE_PARAMETER_BITS);
            for (size_t k = 0; k < last; k++)
                bits.writeRice(values[k], riceK);
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
                  << ", N = " << n << ", step = " << params.step << '\n'
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
    std::vector<double> coeffs(n);
    std::vector<double> output(transform->inputSize());
    // Second part of the previous block's output, still waiting for the next block.
    std::vector<std::vector<double>> pending(channels, std::vector<double>(overlap, 0.0));

    for (size_t b = 0; b < numBlocks; b++)
    {
        const ptrdiff_t start = ptrdiff_t(b * n) - ptrdiff_t(overlap);

        for (size_t c = 0; c < channels; c++)
        {
            const size_t last = bits.read(lastBits);
            if (last > n || !bits.ok())
            {
                std::cerr << filename << ": corrupted or truncated file\n";
                return false;
            }

            const int riceK = last > 0 ? int(bits.read(RICE_PARAMETER_BITS)) : 0;

            // Dequantization: coefficient = index * step.
            for (size_t k = 0; k < last; k++)
                coeffs[k] = zigzagDecode(bits.readRice(riceK)) * step;
            std::fill(coeffs.begin() + last, coeffs.end(), 0.0);

            transform->inverse(coeffs.data(), output.data());

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
