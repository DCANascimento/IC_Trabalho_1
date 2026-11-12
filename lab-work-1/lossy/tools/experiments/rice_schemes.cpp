// Experiment: how many bits do different entropy coding schemes need for the
// quantized MDCT coefficients? (Justifies the entropy coder of the codec v2.)
//
// Usage:  rice_schemes [-n N] -q 16,64,256,1024 file.wav ...
// Output: CSV on stdout, one row per step, in bits per coefficient (all files together):
//
//   A  one Rice parameter per block, the best one (codec v1)
//   B  one Rice parameter per band of 32 coefficients, 5 bits of side information each
//   C  ideal adaptive arithmetic coding: magnitudes 0..15 + escape, 12 contexts from
//      the neighbour magnitudes, cost -log2(p) of adaptive counts (an estimate: no
//      arithmetic coder is implemented), sign 1 bit, escape as Exp-Golomb
//   D  Rice with the parameter predicted from the neighbours, no side information (codec v2)
//
// Every scheme also pays log2(N) + 1 bits per block for 'last' and codes only
// the coefficients before it.

#include "common.hpp"

#include <cstdio>
#include <cstring>

namespace
{

struct Totals
{
    double a = 0, b = 0, c = 0, d = 0, coeffs = 0;
};

const int CONTEXTS = 12;
const int SYMBOLS = 17;   // magnitudes 0..15 and an escape

// Adaptive frequency counts of the arithmetic coding estimate (scheme C).
struct AdaptiveModel
{
    std::vector<std::vector<double>> count;
    std::vector<double> total;

    AdaptiveModel() : count(CONTEXTS, std::vector<double>(SYMBOLS, 1.0)), total(CONTEXTS, SYMBOLS) {}

    double cost(int context, int symbol)
    {
        const double bits = -std::log2(count[context][symbol] / total[context]);
        count[context][symbol] += 32;
        total[context] += 32;
        if (total[context] > 65536)   // halve the counts: keeps the model adaptive
        {
            total[context] = 0;
            for (double& x : count[context])
            {
                x = std::ceil(x / 2);
                total[context] += x;
            }
        }
        return bits;
    }
};

void addBlock(const std::vector<int32_t>& cur, const std::vector<int32_t>& prev, size_t last,
              int lastBits, AdaptiveModel& model, Totals& t)
{
    const size_t n = cur.size();
    std::vector<uint32_t> values(n);
    for (size_t i = 0; i < n; i++)
        values[i] = zigzagEncode(cur[i]);

    t.coeffs += n;
    t.a += lastBits;
    t.b += lastBits;
    t.c += lastBits;
    t.d += lastBits;
    if (last == 0)
        return;

    // A: one parameter per block (5 bits).
    uint64_t bits;
    bestRiceParameter(values, 0, last, &bits);
    t.a += 5 + bits;

    // B: one parameter per band of 32 coefficients (5 bits each).
    for (size_t from = 0; from < last; from += 32)
    {
        bestRiceParameter(values, from, std::min(last, from + 32), &bits);
        t.b += 5 + bits;
    }

    for (size_t i = 0; i < last; i++)
    {
        // C: context = weighted sum of neighbour magnitudes, in 12 logarithmic classes.
        double s = 2.0 * (i > 0 ? magnitude(cur[i - 1]) : 0) + (i > 1 ? magnitude(cur[i - 2]) : 0) +
                   magnitude(prev[i]) + (i > 0 ? magnitude(prev[i - 1]) : 0) +
                   (i + 1 < n ? magnitude(prev[i + 1]) : 0);
        int context = 0;
        while (s >= 0.5 && context < CONTEXTS - 1)
        {
            context++;
            s /= 1.6;
        }
        const uint32_t m = magnitude(cur[i]);
        t.c += model.cost(context, m < 16 ? int(m) : 16);
        if (m >= 16)
        {
            int length = 0;   // Exp-Golomb of m - 16
            while (((m - 16 + 1) >> length) > 1)
                length++;
            t.c += 2 * length + 1;
        }
        if (m != 0)
            t.c += 1;   // sign

        // D: Rice parameter from the neighbours, as in the codec.
        t.d += riceLength(values[i], codecContextParameter(cur, prev, i));
    }
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

    std::printf("step,A_rice_per_block,B_rice_per_band32,C_arithmetic_context,D_rice_context\n");
    for (double step : steps)
    {
        Totals t;
        for (int f = arg; f < argc; f++)
        {
            AdaptiveModel model;   // starts empty for each file, then adapts
            const bool ok = forEachBlock(argv[f], step, n,
                [&](size_t, size_t, const std::vector<int32_t>& cur,
                    const std::vector<int32_t>& prev, size_t last)
                { addBlock(cur, prev, last, lastBits, model, t); });
            if (!ok)
                return 1;
        }
        std::printf("%g,%.3f,%.3f,%.3f,%.3f\n", step, t.a / t.coeffs, t.b / t.coeffs,
                    t.c / t.coeffs, t.d / t.coeffs);
    }
    return 0;
}
