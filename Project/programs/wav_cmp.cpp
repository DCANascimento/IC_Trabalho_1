#include <iostream>
#include <string>
#include <cstdio>

extern "C" {
#include "comparison.h"
}

static void print_metrics(const std::string &title, const ChannelMetrics *metrics)
{
    std::cout << title << "\n";
    std::printf("  MSE:                  %.10f\n", metrics->mse);
    std::printf("  Maximum absolute error: %.10f\n", metrics->max_error);
    std::printf("  SNR:                  %.2f dB\n", metrics->snr_db);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " original.wav quantized.wav\n";
        return 1;
    }

    AudioComparison comparison;
    char error_message[160];

    if (compare_wav_files(
            argv[1], argv[2], &comparison,
            error_message, sizeof(error_message)) != 0) {
        std::cerr << "Comparison failed: " << error_message << "\n";
        return 1;
    }

    std::cout << "Comparison between:\n  Original:  " << argv[1] 
              << "\n  Quantized: " << argv[2] << "\n\n";

    for (int channel = 0; channel < comparison.num_channels; channel++) {
        char title[32];
        std::snprintf(title, sizeof(title), "Channel %d", channel + 1);
        print_metrics(title, &comparison.channels[channel]);
        std::cout << "\n";
    }

    print_metrics("Channel average", &comparison.average);
    return 0;
}