#ifndef QUANTIZATION_H
#define QUANTIZATION_H

#ifdef __cplusplus
extern "C" {
#endif

int quantize_wav_file(
    const char *input_filename,
    const char *output_filename,
    int bits
);

#ifdef __cplusplus
}
#endif

#endif