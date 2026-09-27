#include "comparison.h"

#include <stdio.h>

static void print_metrics(const char *title, const ChannelMetrics *metrics)
{
    printf("%s\n", title);
    printf("  MSE:                  %.10f\n", metrics->mse);
    printf("  Maximum absolute error: %.10f\n", metrics->max_error);
    printf("  SNR:                  %.2f dB\n", metrics->snr_db);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s original.wav quantized.wav\n", argv[0]);
        return 1;
    }

    AudioComparison comparison;
    char error_message[160];

    if (compare_wav_files(
            argv[1], argv[2], &comparison,
            error_message, sizeof(error_message)) != 0) {
        fprintf(stderr, "Comparison failed: %s\n", error_message);
        return 1;
    }

    printf("Comparison between:\n  Original:  %s\n  Quantized: %s\n\n",
        argv[1], argv[2]);

    for (int channel = 0; channel < comparison.num_channels; channel++) {
        char title[32];
        snprintf(title, sizeof(title), "Channel %d", channel + 1);
        print_metrics(title, &comparison.channels[channel]);
        putchar('\n');
    }

    print_metrics("Channel average", &comparison.average);
    return 0;
}