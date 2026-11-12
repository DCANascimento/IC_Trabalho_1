extern "C" {
#include "quantization.h"
#include "wav.h"
}

#include <cmath>
#include <cstdint>
#include <cstdlib>

int quantize_wav_file(
    const char *input_filename,
    const char *output_filename,
    int bits)
{
    if (bits < 1 || bits > 16) {
        return 1;
    }

    Audio audio = {0};

    if (wav_read(input_filename, &audio) != 0) {
        return 1;
    }

    int step = 1 << (16 - bits);
    int16_t *quantized =
        static_cast<int16_t *>(std::malloc(audio.num_samples * sizeof(int16_t)));

    if (quantized == nullptr) {
        audio_free(&audio);
        return 1;
    }

    for (size_t i = 0; i < audio.num_samples; i++) {
        double lower =
            std::floor(static_cast<double>(audio.samples_16[i]) / step) * step;
        double reconstructed = lower + std::floor(step / 2.0);

        if (reconstructed < -32768.0) {
            reconstructed = -32768.0;
        }

        if (reconstructed > 32767.0) {
            reconstructed = 32767.0;
        }

        quantized[i] = static_cast<int16_t>(reconstructed);
    }

    int result = wav_write_16(output_filename, &audio, quantized);

    std::free(quantized);
    audio_free(&audio);
    return result;
}