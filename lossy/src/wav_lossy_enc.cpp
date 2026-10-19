#include "codec.hpp"
#include "wav.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

static void usage(const char* prog)
{
    std::cerr << "Usage: " << prog << " [options] input.wav compressed_file\n"
              << "Options:\n"
              << "  -q <step>   quantization step, >= " << MIN_STEP
              << " (default 16; larger = smaller file, lower quality)\n"
              << "  -n <size>   block size N, a power of two in " << MIN_BLOCK_SIZE << ".."
              << MAX_BLOCK_SIZE << " (default 1024)\n"
              << "  -t <type>   transform: mdct (default) or dct (non-overlapping blocks)\n"
              << "  -v          print statistics\n";
}

int main(int argc, char* argv[])
{
    EncoderParams params;
    int arg = 1;

    // Options come before the two file names.
    for (; arg < argc && argv[arg][0] == '-'; arg++)
    {
        const bool hasValue = arg + 1 < argc;

        if (std::strcmp(argv[arg], "-v") == 0)
        {
            params.verbose = true;
        }
        else if (std::strcmp(argv[arg], "-q") == 0 && hasValue)
        {
            params.step = std::atof(argv[++arg]);
            if (!(params.step >= MIN_STEP))
            {
                std::cerr << "The quantization step must be at least " << MIN_STEP << '\n';
                return 1;
            }
        }
        else if (std::strcmp(argv[arg], "-n") == 0 && hasValue)
        {
            params.blockSize = std::strtoul(argv[++arg], nullptr, 10);
            if (!isValidBlockSize(params.blockSize))
            {
                std::cerr << "Invalid block size\n";
                return 1;
            }
        }
        else if (std::strcmp(argv[arg], "-t") == 0 && hasValue)
        {
            const char* type = argv[++arg];
            if (std::strcmp(type, "mdct") == 0)
                params.transform = TransformType::Mdct;
            else if (std::strcmp(type, "dct") == 0)
                params.transform = TransformType::Dct;
            else
            {
                std::cerr << "Unknown transform: " << type << '\n';
                return 1;
            }
        }
        else
        {
            usage(argv[0]);
            return 1;
        }
    }

    if (argc - arg != 2)
    {
        usage(argv[0]);
        return 1;
    }

    WavAudio audio;
    if (!readWav(argv[arg], audio))
        return 1;

    return encodeFile(audio, argv[arg + 1], params) ? 0 : 1;
}
