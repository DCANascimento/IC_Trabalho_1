#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Integer PCM audio, with the channels stored separately (de-interleaved).
struct WavAudio
{
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 16;                 // 8, 16, 24 or 32
    std::vector<std::vector<int32_t>> samples;   // samples[c][i] = sample i of channel c

    size_t numChannels() const { return samples.size(); }
    size_t numFrames() const { return samples.empty() ? 0 : samples[0].size(); }
};

// Reads an integer PCM WAV file (8/16/24/32 bits, any number of channels).
bool readWav(const std::string& filename, WavAudio& audio);

// Writes a PCM WAV file with the same sample format as 'audio'.
bool writeWav(const std::string& filename, const WavAudio& audio);
