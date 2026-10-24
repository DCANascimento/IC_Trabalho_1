// Experiment: the Rice parameter along the coefficients of one block, showing
// why a single parameter per block (codec v1) wastes bits.
//
// Usage:  rice_k_profile [-n N] [-q step] [-b block] [-c channel] file.wav
//         (default: N = 1024, step = 64, the middle block, channel 0)
// Output: CSV on stdout, one row per coefficient:
//   i            coefficient index (frequency)
//   value        quantized coefficient
//   magnitude    |value|
//   k_block      the single best parameter for the whole block (v1)
//   k_context    the parameter predicted from the neighbours (v2)
//   k_local      the best parameter for the 16 coefficients around i (reference)
//   bits_block, bits_context        cumulative coded size with each method
// A summary goes to stderr.

#include "common.hpp"

#include <cstdio>
#include <cstring>

int main(int argc, char* argv[])
{
    size_t n = 1024;
    double step = 64;
    long wantedBlock = -1;
    size_t wantedChannel = 0;
    int arg = 1;
    for (; arg + 1 < argc && argv[arg][0] == '-'; arg += 2)
    {
        if (std::strcmp(argv[arg], "-n") == 0)
            n = std::strtoul(argv[arg + 1], nullptr, 10);
        else if (std::strcmp(argv[arg], "-q") == 0)
            step = std::atof(argv[arg + 1]);
        else if (std::strcmp(argv[arg], "-b") == 0)
            wantedBlock = std::atol(argv[arg + 1]);
        else if (std::strcmp(argv[arg], "-c") == 0)
            wantedChannel = std::strtoul(argv[arg + 1], nullptr, 10);
    }
    if (arg + 1 != argc)
    {
        std::fprintf(stderr, "Usage: %s [-n N] [-q step] [-b block] [-c channel] file.wav\n",
                     argv[0]);
        return 1;
    }

    WavAudio audio;
    if (!readWav(argv[arg], audio))
        return 1;
    const size_t numBlocks = (audio.numFrames() + n - 1) / n + 1;
    const size_t block = wantedBlock >= 0 ? size_t(wantedBlock) : numBlocks / 2;
    if (block >= numBlocks || wantedChannel >= audio.numChannels())
    {
        std::fprintf(stderr, "Block or channel out of range (%zu blocks, %zu channels)\n",
                     numBlocks, audio.numChannels());
        return 1;
    }

    std::vector<int32_t> cur, prev;
    size_t last = 0;
    forEachBlock(argv[arg], step, n,
        [&](size_t c, size_t b, const std::vector<int32_t>& cu, const std::vector<int32_t>& pr,
            size_t l)
        {
            if (c == wantedChannel && b == block)
            {
                cur = cu;
                prev = pr;
                last = l;
            }
        });

    std::vector<uint32_t> values(n);
    for (size_t i = 0; i < n; i++)
        values[i] = zigzagEncode(cur[i]);
    const int kBlock = bestRiceParameter(values, 0, last);

    std::printf("i,value,magnitude,k_block,k_context,k_local,bits_block,bits_context\n");
    uint64_t bitsBlock = 0, bitsContext = 0;
    for (size_t i = 0; i < n; i++)
    {
        const int kContext = codecContextParameter(cur, prev, i);
        const size_t from = i >= 8 ? i - 8 : 0;
        const int kLocal = bestRiceParameter(values, from, std::min(n, from + 16));
        if (i < last)
        {
            bitsBlock += riceLength(values[i], kBlock);
            bitsContext += riceLength(values[i], kContext);
        }
        std::printf("%zu,%d,%u,%d,%d,%d,%llu,%llu\n", i, cur[i], magnitude(cur[i]), kBlock,
                    kContext, kLocal, (unsigned long long)bitsBlock,
                    (unsigned long long)bitsContext);
    }

    const std::string path = argv[arg];
    const std::string name = path.substr(path.find_last_of('/') + 1);
    std::fprintf(stderr,
                 "file %s, channel %zu, block %zu of %zu, N = %zu, step = %g\n"
                 "last = %zu, k_block = %d\n"
                 "bits: one k per block %llu (+5 for k), k from context %llu (%.1f %% less)\n",
                 name.c_str(), wantedChannel, block, numBlocks, n, step, last, kBlock,
                 (unsigned long long)bitsBlock, (unsigned long long)bitsContext,
                 100.0 * (1.0 - double(bitsContext) / double(bitsBlock + 5)));
    return 0;
}
