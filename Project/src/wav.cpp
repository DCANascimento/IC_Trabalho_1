#include "wav.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>

int wav_read(const char *filename, Audio *audio)
{
    FILE *file = std::fopen(filename, "rb");

    if (file == nullptr) {
        std::printf("Could not open file\n");
        return 1;
    }

    /* RIFF header */
    char riff[4];
    uint32_t file_size;
    char wave[4];

    std::fread(riff, 1, 4, file);
    std::fread(&file_size, 4, 1, file);
    std::fread(wave, 1, 4, file);

    /* fmt chunk */
    char fmt[4];
    uint32_t fmt_size;

    std::fread(fmt, 1, 4, file);
    std::fread(&fmt_size, 4, 1, file);

    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;

    std::fread(&audio_format, 2, 1, file);
    std::fread(&num_channels, 2, 1, file);
    std::fread(&sample_rate, 4, 1, file);
    std::fread(&byte_rate, 4, 1, file);
    std::fread(&block_align, 2, 1, file);
    std::fread(&bits_per_sample, 2, 1, file);

    /* data chunk */
    char data[4];
    uint32_t data_size;

    std::fread(data, 1, 4, file);
    std::fread(&data_size, 4, 1, file);

    if ((num_channels != 1 && num_channels != 2) ||
        audio_format != 1 || bits_per_sample != 16) {
        std::printf("Only 16-bit mono or stereo PCM WAV files are supported\n");
        std::fclose(file);
        return 1;
    }

    size_t num_samples = data_size / sizeof(int16_t);

    audio->samples = static_cast<double *>(std::malloc(num_samples * sizeof(double)));
    audio->samples_16 = static_cast<int16_t *>(std::malloc(num_samples * sizeof(int16_t)));

    if (audio->samples == nullptr || audio->samples_16 == nullptr) {
        std::printf("Memory allocation failed\n");
        std::free(audio->samples);
        std::free(audio->samples_16);
        std::fclose(file);
        return 1;
    }

    audio->num_samples = num_samples;
    audio->sample_rate = sample_rate;
    audio->num_channels = num_channels;

    /* Read samples */
    for (size_t i = 0; i < num_samples; i++) {
        int16_t sample;
        std::fread(&sample, sizeof(int16_t), 1, file);

        /*
         * Convert from 16-bit integer
         * to approximately [-1, 1].
         */
        audio->samples_16[i] = sample;
        audio->samples[i] = static_cast<double>(sample) / 32768.0;
    }

    std::fclose(file);

    return 0;
}

void audio_free(Audio *audio)
{
    std::free(audio->samples);
    std::free(audio->samples_16);

    audio->samples = nullptr;
    audio->samples_16 = nullptr;
}

int wav_write_16(
    const char *filename,
    const Audio *audio,
    const int16_t *samples)
{
    FILE *file = std::fopen(filename, "wb");

    if (file == nullptr) {
        return 1;
    }

    uint32_t data_size = static_cast<uint32_t>(audio->num_samples * sizeof(int16_t));
    uint32_t file_size = 36 + data_size;
    uint16_t audio_format = 1;
    uint16_t channels = static_cast<uint16_t>(audio->num_channels);
    uint32_t sample_rate = static_cast<uint32_t>(audio->sample_rate);
    uint16_t bits_per_sample = 16;
    uint16_t block_align = static_cast<uint16_t>(channels * sizeof(int16_t));
    uint32_t byte_rate = sample_rate * block_align;

    std::fwrite("RIFF", 1, 4, file);
    std::fwrite(&file_size, sizeof(file_size), 1, file);
    std::fwrite("WAVE", 1, 4, file);
    std::fwrite("fmt ", 1, 4, file);

    uint32_t fmt_size = 16;
    std::fwrite(&fmt_size, sizeof(fmt_size), 1, file);
    std::fwrite(&audio_format, sizeof(audio_format), 1, file);
    std::fwrite(&channels, sizeof(channels), 1, file);
    std::fwrite(&sample_rate, sizeof(sample_rate), 1, file);
    std::fwrite(&byte_rate, sizeof(byte_rate), 1, file);
    std::fwrite(&block_align, sizeof(block_align), 1, file);
    std::fwrite(&bits_per_sample, sizeof(bits_per_sample), 1, file);
    std::fwrite("data", 1, 4, file);
    std::fwrite(&data_size, sizeof(data_size), 1, file);
    std::fwrite(samples, sizeof(int16_t), audio->num_samples, file);

    int failed = std::ferror(file);
    std::fclose(file);
    return failed ? 1 : 0;
}