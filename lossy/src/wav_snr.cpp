// Compares a decoded WAV file with the original one:
// MSE, maximum absolute error and SNR, per channel and for all channels together.
// Errors are measured in units of the original file's samples.

#include "wav.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

static void printRow(const char* name, double signal, double noise, double maxError, size_t count)
{
    const double snr = noise > 0 ? 10 * std::log10(signal / noise) : INFINITY;
    std::printf("%-10s %14.4f %12.0f %10.2f\n", name, noise / count, maxError, snr);
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::fprintf(stderr, "Usage: %s original.wav decoded.wav\n", argv[0]);
        return 1;
    }

    WavAudio original;
    WavAudio decoded;
    if (!readWav(argv[1], original) || !readWav(argv[2], decoded))
        return 1;

    if (original.numChannels() != decoded.numChannels() ||
        original.numFrames() != decoded.numFrames() ||
        original.bitsPerSample != decoded.bitsPerSample)
    {
        std::fprintf(stderr, "The files have different channels, lengths or sample formats\n");
        return 1;
    }

    const size_t frames = original.numFrames();
    double totalSignal = 0, totalNoise = 0, totalMax = 0;

    std::printf("%-10s %14s %12s %10s\n", "channel", "MSE", "max error", "SNR (dB)");

    for (size_t c = 0; c < original.numChannels(); c++)
    {
        double signal = 0, noise = 0, maxError = 0;

        for (size_t i = 0; i < frames; i++)
        {
            const double x = original.samples[c][i];
            const double e = x - decoded.samples[c][i];
            signal += x * x;
            noise += e * e;
            maxError = std::max(maxError, std::abs(e));
        }

        printRow(std::to_string(c).c_str(), signal, noise, maxError, frames);

        totalSignal += signal;
        totalNoise += noise;
        totalMax = std::max(totalMax, maxError);
    }

    printRow("all", totalSignal, totalNoise, totalMax, frames * original.numChannels());
    return 0;
}
