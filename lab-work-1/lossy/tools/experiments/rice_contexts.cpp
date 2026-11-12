// Experiment: which neighbours should predict the Rice parameter? (Tunes the
// context of the codec v2.)
//
// Usage:  rice_contexts [-n N] -q 16,64,256,1024 file.wav ...
// Output: CSV on stdout: one row per step and context, one column per scale,
//         in bits per coefficient.
//
// For each coefficient, a context gives the mean magnitude m of some neighbours
// (c1 = previous coefficient of the block, c2 the one before, ...; p0 = same
// frequency in the previous block, p-1 and p+1 its neighbours). The Rice parameter
// is the largest k with 2^k <= 2 * m * scale (2m ~ mean zigzag value).
// scale = 1 is the plain estimate; the other scales check that it is well centred.

#include "common.hpp"

#include <cstdio>
#include <cstring>

namespace
{

const char* const CONTEXT_NAMES[] = {
    "c1 c2",
    "c1 c2 p0",
    "2c1 c2 p0 (p-1 p+1)/2",
    "c1..c4 2p0 p-1 p+1 (codec)",
};
const int NUM_CONTEXTS = 4;
const double SCALES[] = {0.5, 0.7, 1.0, 1.4, 2.0};
const int NUM_SCALES = 5;

// Mean neighbour magnitude of coefficient i for each context.
void contextMeans(const std::vector<int32_t>& cur, const std::vector<int32_t>& prev, size_t i,
                  double* means)
{
    const size_t n = cur.size();
    auto c = [&](size_t j) { return j <= i ? double(magnitude(cur[i - j])) : 0.0; };
    const double p0 = magnitude(prev[i]);
    const double pm = i > 0 ? magnitude(prev[i - 1]) : 0.0;
    const double pp = i + 1 < n ? magnitude(prev[i + 1]) : 0.0;

    means[0] = (c(1) + c(2)) / 2;
    means[1] = (c(1) + c(2) + p0) / 3;
    means[2] = (2 * c(1) + c(2) + p0 + (pm + pp) / 2) / 5;
    means[3] = (c(1) + c(2) + c(3) + c(4) + 2 * p0 + pm + pp) / 8;
}

} // namespace

int main(int argc, char* argv[])
{
    size_t n = 1024;
    std::vector<double> steps;
    int arg = 1;
    for (; arg + 1 < argc && argv[arg][0] == '-'; arg += 2)
    {
        if (std::strcmp(argv[arg], "-n") == 0)
            n = std::strtoul(argv[arg + 1], nullptr, 10);
        else if (std::strcmp(argv[arg], "-q") == 0)
            steps = parseList(argv[arg + 1]);
    }
    if (steps.empty() || arg >= argc)
    {
        std::fprintf(stderr, "Usage: %s [-n N] -q step1,step2,... file.wav ...\n", argv[0]);
        return 1;
    }

    int lastBits = 0;
    while ((size_t(1) << lastBits) <= n)
        lastBits++;

    std::printf("step,context");
    for (double scale : SCALES)
        std::printf(",scale %.1f", scale);
    std::printf("\n");
    for (double step : steps)
    {
        double bits[NUM_CONTEXTS][NUM_SCALES] = {};
        double coeffs = 0;
        double means[NUM_CONTEXTS];

        for (int f = arg; f < argc; f++)
        {
            const bool ok = forEachBlock(argv[f], step, n,
                [&](size_t, size_t, const std::vector<int32_t>& cur,
                    const std::vector<int32_t>& prev, size_t last)
                {
                    coeffs += cur.size();
                    for (int x = 0; x < NUM_CONTEXTS; x++)
                        for (int s = 0; s < NUM_SCALES; s++)
                            bits[x][s] += lastBits;

                    for (size_t i = 0; i < last; i++)
                    {
                        const uint32_t v = zigzagEncode(cur[i]);
                        contextMeans(cur, prev, i, means);
                        for (int x = 0; x < NUM_CONTEXTS; x++)
                            for (int s = 0; s < NUM_SCALES; s++)
                                bits[x][s] += riceLength(
                                    v, riceParameterForMean(2 * means[x] * SCALES[s]));
                    }
                });
            if (!ok)
                return 1;
        }

        for (int x = 0; x < NUM_CONTEXTS; x++)
        {
            std::printf("%g,\"%s\"", step, CONTEXT_NAMES[x]);
            for (int s = 0; s < NUM_SCALES; s++)
                std::printf(",%.3f", bits[x][s] / coeffs);
            std::printf("\n");
        }
    }
    return 0;
}
