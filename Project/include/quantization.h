#ifndef QUANTIZATION_H
#define QUANTIZATION_H

int quantize_wav_file(
    const char *input_filename,
    const char *output_filename,
    int bits
);

#endif