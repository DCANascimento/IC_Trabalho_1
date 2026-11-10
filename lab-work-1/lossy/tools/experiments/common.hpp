#pragma once

// Shared code of the lossy codec experiments.
//
// forEachBlock() gives the quantized MDCT coefficients of every block of a WAV
// file, computed as the codec does it (samples in 16-bit units, sine-window MDCT,
// uniform quantization round(X / step)). Channels are kept as L/R (no Mid/Side),
// so that the entropy coding schemes are compared on the same data.

#include "bitstream.hpp"
#include "mdct.hpp"
#include "wav.hpp"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

// f(channel, block, cur, prev, last): cur and prev are the quantized coefficients
// of this block and of the previous block of the same channel; cur[last..N) are zero.
using BlockFunction = std::function<void(size_t channel, size_t block,
                                         const std::vector<int32_t>& cur,
                                         const std::vector<int32_t>& prev, size_t last)>;

inline bool forEachBlock(const std::string& filename, double step, size_t n,
                         const BlockFunction& f)
{
    WavAudio audio;
    if (!readWav(filename, audio))
        return false;

    const double unit = std::ldexp(1.0, audio.bitsPerSample - 16);
    const size_t numFrames = audio.numFrames();
    const size_t numBlocks = (numFrames + n - 1) / n + 1;
    step = float(step);   // the codec stores the step as a float

    Mdct mdct(n);
    std::vector<double> input(2 * n), coeffs(n);
    std::vector<int32_t> cur(n), prev(n);

    for (size_t c = 0; c < audio.numChannels(); c++)
    {
        std::fill(prev.begin(), prev.end(), 0);
        for (size_t b = 0; b < numBlocks; b++)
        {
            const ptrdiff_t start = ptrdiff_t(b * n) - ptrdiff_t(n);
            for (size_t i = 0; i < 2 * n; i++)
            {
                const ptrdiff_t t = start + ptrdiff_t(i);
                input[i] = (t >= 0 && size_t(t) < numFrames) ? audio.samples[c][t] / unit : 0.0;
            }
            mdct.forward(input.data(), coeffs.data());

            size_t last = 0;
            for (size_t i = 0; i < n; i++)
            {
                cur[i] = int32_t(std::lround(coeffs[i] / step));
                if (cur[i] != 0)
                    last = i + 1;
            }

            f(c, b, cur, prev, last);
            prev.swap(cur);
        }
    }
    return true;
}

inline uint32_t magnitude(int32_t x)
{
    return uint32_t(std::abs(int64_t(x)));
}

// Largest k with 2^k <= mean (0 if mean < 2): the usual Rice parameter estimate.
inline int riceParameterForMean(double mean)
{
    int k = 0;
    while (k < MAX_RICE_PARAMETER && std::ldexp(1.0, k + 1) <= mean)
        k++;
    return k;
}

// The context used by the codec (v2, codec.cpp: contextRiceParameter):
// s = 4 previous coefficients of the block + 2 x same frequency in the previous
// block + its 2 neighbours; mean zigzag value ~ s / 4.
inline int codecContextParameter(const std::vector<int32_t>& cur,
                                 const std::vector<int32_t>& prev, size_t i)
{
    const size_t n = cur.size();
    double s = 2.0 * magnitude(prev[i]);
    if (i > 0)
        s += magnitude(prev[i - 1]);
    if (i + 1 < n)
        s += magnitude(prev[i + 1]);
    for (size_t j = 1; j <= 4 && j <= i; j++)
        s += magnitude(cur[i - j]);
    return riceParameterForMean(s / 4);
}

// Exhaustive search of the Rice parameter that codes values[from..to) with fewest bits.
inline int bestRiceParameter(const std::vector<uint32_t>& values, size_t from, size_t to,
                             uint64_t* bitsOut = nullptr)
{
    int best = 0;
    uint64_t bestBits = UINT64_MAX;
    for (int k = 0; k <= 20; k++)
    {
        uint64_t bits = 0;
        for (size_t i = from; i < to; i++)
            bits += riceLength(values[i], k);
        if (bits < bestBits)
        {
            bestBits = bits;
            best = k;
        }
    }
    if (bitsOut)
        *bitsOut = bestBits;
    return best;
}

// "16,64,256" -> {16, 64, 256}
inline std::vector<double> parseList(const char* text)
{
    std::vector<double> values;
    char* end = nullptr;
    for (const char* p = text; *p; p = (*end == ',') ? end + 1 : end)
    {
        values.push_back(std::strtod(p, &end));
        if (end == p)
            break;
    }
    return values;
}
