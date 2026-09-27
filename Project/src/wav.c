#include "wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int wav_read(const char *filename, Audio *audio)
{
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        printf("Could not open file\n");
        return 1;
    }

    /* RIFF header */
    char riff[4];
    uint32_t file_size;
    char wave[4];

    fread(riff, 1, 4, file);
    fread(&file_size, 4, 1, file);
    fread(wave, 1, 4, file);

    /* fmt chunk */
    char fmt[4];
    uint32_t fmt_size;

    fread(fmt, 1, 4, file);
    fread(&fmt_size, 4, 1, file);

    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;

    fread(&audio_format, 2, 1, file);
    fread(&num_channels, 2, 1, file);
    fread(&sample_rate, 4, 1, file);
    fread(&byte_rate, 4, 1, file);
    fread(&block_align, 2, 1, file);
    fread(&bits_per_sample, 2, 1, file);

    /* data chunk */
    char data[4];
    uint32_t data_size;

    fread(data, 1, 4, file);
    fread(&data_size, 4, 1, file);

    if ((num_channels != 1 && num_channels != 2) ||
        audio_format != 1 || bits_per_sample != 16) {
        printf("Only 16-bit mono or stereo PCM WAV files are supported\n");
        fclose(file);
        return 1;
    }

    size_t num_samples = data_size / sizeof(int16_t);

    audio->samples = malloc(num_samples * sizeof(double));
    audio->samples_16 = malloc(num_samples * sizeof(int16_t));

    if (audio->samples == NULL || audio->samples_16 == NULL) {
        printf("Memory allocation failed\n");
        free(audio->samples);
        free(audio->samples_16);
        fclose(file);
        return 1;
    }

    audio->num_samples = num_samples;
    audio->sample_rate = sample_rate;
    audio->num_channels = num_channels;

    /* Read samples */
    for (size_t i = 0; i < num_samples; i++) {

        int16_t sample;

        fread(&sample, sizeof(int16_t), 1, file);

        /*
         * Convert from 16-bit integer
         * to approximately [-1, 1].
         */
        audio->samples_16[i] = sample;
        audio->samples[i] = (double)sample / 32768.0;
    }

    fclose(file);

    return 0;
}

void audio_free(Audio *audio)
{
    free(audio->samples);
    free(audio->samples_16);

    audio->samples = NULL;
    audio->samples_16 = NULL;
}

int wav_write_16(
    const char *filename,
    const Audio *audio,
    const int16_t *samples)
{
    FILE *file = fopen(filename, "wb");

    if (file == NULL) {
        return 1;
    }

    uint32_t data_size =
        (uint32_t)(audio->num_samples * sizeof(int16_t));
    uint32_t file_size = 36 + data_size;
    uint16_t audio_format = 1;
    uint16_t channels = (uint16_t)audio->num_channels;
    uint32_t sample_rate = (uint32_t)audio->sample_rate;
    uint16_t bits_per_sample = 16;
    uint16_t block_align = channels * sizeof(int16_t);
    uint32_t byte_rate = sample_rate * block_align;

    fwrite("RIFF", 1, 4, file);
    fwrite(&file_size, sizeof(file_size), 1, file);
    fwrite("WAVE", 1, 4, file);
    fwrite("fmt ", 1, 4, file);

    uint32_t fmt_size = 16;
    fwrite(&fmt_size, sizeof(fmt_size), 1, file);
    fwrite(&audio_format, sizeof(audio_format), 1, file);
    fwrite(&channels, sizeof(channels), 1, file);
    fwrite(&sample_rate, sizeof(sample_rate), 1, file);
    fwrite(&byte_rate, sizeof(byte_rate), 1, file);
    fwrite(&block_align, sizeof(block_align), 1, file);
    fwrite(&bits_per_sample, sizeof(bits_per_sample), 1, file);
    fwrite("data", 1, 4, file);
    fwrite(&data_size, sizeof(data_size), 1, file);
    fwrite(samples, sizeof(int16_t), audio->num_samples, file);

    int failed = ferror(file);
    fclose(file);
    return failed ? 1 : 0;
}