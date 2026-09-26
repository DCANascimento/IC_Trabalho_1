#include "quantization.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 5) {
        printf("Usage: %s input.wav output.wav --bits N\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[3], "--bits") != 0) {
        printf("Expected --bits\n");
        return 1;
    }

    int bits = atoi(argv[4]);
    if (bits < 1 || bits > 16) {
        printf("Bits must be between 1 and 16\n");
        return 1;
    }

    if (quantize_wav_file(argv[1], argv[2], bits) != 0) {
        printf("Could not quantize WAV file\n");
        return 1;
    }

    printf("Quantized %s to %s using %d bits\n", argv[1], argv[2], bits);
    return 0;
}