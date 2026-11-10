#include "codec.hpp"
#include "wav.hpp"

#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " compressed_file output.wav\n";
        return 1;
    }

    WavAudio audio;
    if (!decodeFile(argv[1], audio))
        return 1;

    return writeWav(argv[2], audio) ? 0 : 1;
}
