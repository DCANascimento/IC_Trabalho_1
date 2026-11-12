#ifndef COMPARISON_H
#define COMPARISON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMPARISON_MAX_CHANNELS 2

typedef struct {
    double mse;
    double max_error;
    double snr_db;
} ChannelMetrics;

typedef struct {
    int num_channels;
    ChannelMetrics channels[COMPARISON_MAX_CHANNELS];
    ChannelMetrics average;
} AudioComparison;

int compare_wav_files(
    const char *original_filename,
    const char *processed_filename,
    AudioComparison *comparison,
    char *error_message,
    size_t error_message_size
);

#ifdef __cplusplus
}
#endif

#endif