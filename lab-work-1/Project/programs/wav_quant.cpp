#include <iostream>
#include <string>
#include <cstdlib>

extern "C" {
#include "quantization.h"
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        std::cout << "Usage: " << argv[0] << " input.wav output.wav --bits N\n";
        return 1;
    }

    if (std::string(argv[3]) != "--bits") {
        std::cerr << "Expected --bits\n";
        return 1;
    }

    int bits = std::stoi(argv[4]);
    if (bits < 1 || bits > 16) {
        std::cerr << "Bits must be between 1 and 16\n";
        return 1;
    }

    if (quantize_wav_file(argv[1], argv[2], bits) != 0) {
        std::cerr << "Could not quantize WAV file\n";
        return 1;
    }

    std::cout << "Quantized " << argv[1] << " to " << argv[2] << " using " << bits << " bits\n";
    return 0;
}