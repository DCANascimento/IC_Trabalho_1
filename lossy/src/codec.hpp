#pragma once

#include "transform.hpp"
#include "wav.hpp"

#include <string>

struct EncoderParams
{
    TransformType transform = TransformType::Mdct;
    size_t blockSize = 1024;   // N: coefficients per block (= hop size)
    double step = 16.0;        // quantization step, in units of a 16-bit sample
    bool midSide = true;       // stereo: allow Mid/Side coding (chosen per block)
    bool verbose = false;
};

// Smaller steps could produce quantized values that do not fit in 32 bits.
const double MIN_STEP = 0.01;

bool encodeFile(const WavAudio& audio, const std::string& filename,
                const EncoderParams& params);

bool decodeFile(const std::string& filename, WavAudio& audio);
