#include "wav.hpp"

#include "bytes.hpp"

#include <algorithm>
#include <iostream>

namespace
{

const uint16_t FORMAT_PCM = 1;
const uint16_t FORMAT_EXTENSIBLE = 0xFFFE;

bool fail(const std::string& filename, const char* message)
{
    std::cerr << filename << ": " << message << '\n';
    return false;
}

// Converts one stored sample to a signed integer.
int32_t decodeSample(const uint8_t* p, int bytesPerSample)
{
    switch (bytesPerSample)
    {
    case 1:  return int32_t(p[0]) - 128;   // 8-bit WAV samples are unsigned
    case 2:  return int16_t(readLE16(p));
    case 3:  return int32_t(uint32_t(p[0]) << 8 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 24) >> 8;
    default: return int32_t(readLE32(p));
    }
}

void encodeSample(ByteWriter& out, int32_t v, int bytesPerSample)
{
    switch (bytesPerSample)
    {
    case 1:  out.put8(uint8_t(v + 128)); break;
    case 2:  out.put16(uint16_t(v)); break;
    case 3:  out.put8(uint8_t(v)); out.put16(uint16_t(v >> 8)); break;
    default: out.put32(uint32_t(v)); break;
    }
}

} // namespace

bool readWav(const std::string& filename, WavAudio& audio)
{
    std::vector<uint8_t> bytes;
    if (!readFile(filename, bytes))
        return fail(filename, "cannot read file");

    if (bytes.size() < 12 || std::memcmp(&bytes[0], "RIFF", 4) != 0 ||
        std::memcmp(&bytes[8], "WAVE", 4) != 0)
        return fail(filename, "not a WAV file");

    // Walk through the chunks looking for "fmt " and "data"; skip any other (LIST, fact, ...).
    const uint8_t* fmt = nullptr;
    const uint8_t* data = nullptr;
    size_t fmtSize = 0;
    size_t dataSize = 0;
    size_t pos = 12;

    while (pos + 8 <= bytes.size())
    {
        const uint8_t* chunk = &bytes[pos];
        const size_t size = readLE32(chunk + 4);
        const size_t available = std::min(size, bytes.size() - pos - 8);

        if (std::memcmp(chunk, "fmt ", 4) == 0)
        {
            fmt = chunk + 8;
            fmtSize = available;
        }
        else if (std::memcmp(chunk, "data", 4) == 0)
        {
            data = chunk + 8;
            dataSize = available;   // tolerates a wrong size field (e.g. files written while streaming)
            break;
        }

        pos += 8 + size + (size & 1);   // chunks are padded to an even size
    }

    if (fmt == nullptr || fmtSize < 16)
        return fail(filename, "missing 'fmt ' chunk");
    if (data == nullptr)
        return fail(filename, "missing 'data' chunk");

    uint16_t format = readLE16(fmt);
    const uint16_t channels = readLE16(fmt + 2);
    const uint16_t bits = readLE16(fmt + 14);

    // In WAVE_FORMAT_EXTENSIBLE the real format code is at the start of the SubFormat GUID.
    if (format == FORMAT_EXTENSIBLE && fmtSize >= 26)
        format = readLE16(fmt + 24);

    if (format != FORMAT_PCM)
        return fail(filename, "only integer PCM WAV files are supported");
    if (channels == 0)
        return fail(filename, "invalid number of channels");
    if (bits != 8 && bits != 16 && bits != 24 && bits != 32)
        return fail(filename, "only 8, 16, 24 or 32 bits per sample are supported");

    audio.sampleRate = readLE32(fmt + 4);
    audio.bitsPerSample = bits;

    // De-interleave: L R L R ... -> one vector per channel.
    const int bytesPerSample = bits / 8;
    const size_t numFrames = dataSize / (size_t(bytesPerSample) * channels);

    audio.samples.assign(channels, std::vector<int32_t>(numFrames));

    const uint8_t* p = data;
    for (size_t i = 0; i < numFrames; i++)
    {
        for (size_t c = 0; c < channels; c++)
        {
            audio.samples[c][i] = decodeSample(p, bytesPerSample);
            p += bytesPerSample;
        }
    }

    return true;
}

bool writeWav(const std::string& filename, const WavAudio& audio)
{
    const uint16_t channels = uint16_t(audio.numChannels());
    const int bytesPerSample = audio.bitsPerSample / 8;
    const uint16_t blockAlign = uint16_t(channels * bytesPerSample);
    const size_t dataSize = audio.numFrames() * blockAlign;
    const size_t padding = dataSize & 1;

    if (dataSize > 0xFFFFFFFFu - 36 - padding)
        return fail(filename, "audio too long for a WAV file");

    ByteWriter out;
    out.bytes.reserve(44 + dataSize + padding);

    // RIFF header
    out.putTag("RIFF");
    out.put32(uint32_t(36 + dataSize + padding));
    out.putTag("WAVE");

    // fmt chunk (16 bytes, plain PCM)
    out.putTag("fmt ");
    out.put32(16);
    out.put16(FORMAT_PCM);
    out.put16(channels);
    out.put32(audio.sampleRate);
    out.put32(audio.sampleRate * blockAlign);   // byte rate
    out.put16(blockAlign);
    out.put16(audio.bitsPerSample);

    // data chunk: interleave the channels again
    out.putTag("data");
    out.put32(uint32_t(dataSize));

    for (size_t i = 0; i < audio.numFrames(); i++)
        for (size_t c = 0; c < channels; c++)
            encodeSample(out, audio.samples[c][i], bytesPerSample);

    if (padding)
        out.put8(0);

    if (!writeFile(filename, out.bytes))
        return fail(filename, "cannot write file");

    return true;
}
