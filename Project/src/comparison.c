#include "comparison.h"

#include "wav.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int set_error(char *message, size_t message_size, const char *text)
{
    if (message != NULL && message_size > 0) {
        snprintf(message, message_size, "%s", text);
    }
    return 1;
}

int compare_wav_files(
    const char *original_filename,
    const char *processed_filename,
    AudioComparison *comparison,
    char *error_message,
    size_t error_message_size)
{
    if (comparison == NULL) {
        return set_error(error_message, error_message_size,
            "Comparison result is missing.");
    }

    Audio original = {0};
    Audio processed = {0};

    if (wav_read(original_filename, &original) != 0) {
        return set_error(error_message, error_message_size,
            "Could not read the original WAV file (expected 16-bit stereo PCM).");
    }

    if (wav_read(processed_filename, &processed) != 0) {
        audio_free(&original);
        return set_error(error_message, error_message_size,
            "Could not read the quantized WAV file (expected 16-bit stereo PCM).");
    }

    if (original.sample_rate != processed.sample_rate) {
        audio_free(&original);
        audio_free(&processed);
        return set_error(error_message, error_message_size,
            "The WAV files have different sample rates.");
    }

    if (original.num_channels != processed.num_channels) {
        audio_free(&original);
        audio_free(&processed);
        return set_error(error_message, error_message_size,
            "The WAV files have different channel counts.");
    }

    if (original.num_samples != processed.num_samples) {
        audio_free(&original);
        audio_free(&processed);
        return set_error(error_message, error_message_size,
            "The WAV files have different lengths.");
    }

    memset(comparison, 0, sizeof(*comparison));
    comparison->num_channels = original.num_channels;
    size_t samples_per_channel =
        original.num_samples / (size_t)original.num_channels;

    for (int channel = 0; channel < comparison->num_channels; channel++) {
        double squared_error_sum = 0.0;
        double signal_power_sum = 0.0;
        double max_error = 0.0;

        for (size_t frame = 0; frame < samples_per_channel; frame++) {
            size_t index = frame * (size_t)original.num_channels +
                (size_t)channel;
            double signal = original.samples[index];
            double difference = signal - processed.samples[index];
            double absolute_error = fabs(difference);

            squared_error_sum += difference * difference;
            signal_power_sum += signal * signal;
            if (absolute_error > max_error) {
                max_error = absolute_error;
            }
        }

        double mse = squared_error_sum / (double)samples_per_channel;
        double signal_power = signal_power_sum / (double)samples_per_channel;
        double snr_db;

        if (mse == 0.0) {
            snr_db = signal_power == 0.0 ? NAN : INFINITY;
        } else {
            snr_db = 10.0 * log10(signal_power / mse);
        }

        comparison->channels[channel].mse = mse;
        comparison->channels[channel].max_error = max_error;
        comparison->channels[channel].snr_db = snr_db;
        comparison->average.mse += mse;
        comparison->average.max_error += max_error;
        comparison->average.snr_db += snr_db;
    }

    comparison->average.mse /= comparison->num_channels;
    comparison->average.max_error /= comparison->num_channels;
    comparison->average.snr_db /= comparison->num_channels;

    if (error_message != NULL && error_message_size > 0) {
        error_message[0] = '\0';
    }

    audio_free(&original);
    audio_free(&processed);
    return 0;
}