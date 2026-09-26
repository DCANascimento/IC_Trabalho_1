#include "quantization.h"

#include "wav.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

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
        malloc(audio.num_samples * sizeof(int16_t));

    if (quantized == NULL) {
        audio_free(&audio);
        return 1;
    }

    for (size_t i = 0; i < audio.num_samples; i++) {
        double lower =
            floor((double)audio.samples_16[i] / step) * step;
        double reconstructed = lower + floor(step / 2.0);

        if (reconstructed < -32768.0) {
            reconstructed = -32768.0;
        }

        if (reconstructed > 32767.0) {
            reconstructed = 32767.0;
        }

        quantized[i] = (int16_t)reconstructed;
    }

    int result = wav_write_16(output_filename, &audio, quantized);

    free(quantized);
    audio_free(&audio);
    return result;
}