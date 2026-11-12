#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// Helpers to work with binary data in memory.
// Both WAV files and our compressed format store integers in little-endian order.

inline uint16_t readLE16(const uint8_t* p)
{
    return uint16_t(p[0] | p[1] << 8);
}

inline uint32_t readLE32(const uint8_t* p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

// Loads a whole file into memory with a single read.
inline bool readFile(const std::string& filename, std::vector<uint8_t>& bytes)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file)
        return false;

    bytes.resize(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    return static_cast<bool>(file);
}

inline bool writeFile(const std::string& filename, const std::vector<uint8_t>& bytes)
{
    std::ofstream file(filename, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return static_cast<bool>(file);
}

// Appends values to a growing byte buffer.
class ByteWriter
{
public:
    std::vector<uint8_t> bytes;

    void put8(uint8_t v) { bytes.push_back(v); }
    void put16(uint16_t v) { put8(uint8_t(v)); put8(uint8_t(v >> 8)); }
    void put32(uint32_t v) { put16(uint16_t(v)); put16(uint16_t(v >> 16)); }
    void putTag(const char* tag) { bytes.insert(bytes.end(), tag, tag + 4); }

    void putFloat(float v)
    {
        uint32_t u;
        std::memcpy(&u, &v, 4);
        put32(u);
    }
};

// Reads values from a byte buffer. Reading past the end sets an error flag
// (checked once with ok()) instead of crashing.
class ByteReader
{
public:
    ByteReader(const uint8_t* data, size_t size) : p_(data), end_(data + size) {}

    bool ok() const { return !failed_; }

    uint8_t get8()
    {
        if (p_ >= end_)
        {
            failed_ = true;
            return 0;
        }
        return *p_++;
    }

    uint16_t get16()
    {
        uint16_t lo = get8();
        return uint16_t(lo | get8() << 8);
    }

    uint32_t get32()
    {
        uint32_t lo = get16();
        return lo | uint32_t(get16()) << 16;
    }

    float getFloat()
    {
        uint32_t u = get32();
        float v;
        std::memcpy(&v, &u, 4);
        return v;
    }

    bool tagIs(const char* tag)
    {
        char read[4] = {char(get8()), char(get8()), char(get8()), char(get8())};
        return ok() && std::memcmp(read, tag, 4) == 0;
    }

    const uint8_t* position() const { return p_; }
    size_t remaining() const { return size_t(end_ - p_); }

private:
    const uint8_t* p_;
    const uint8_t* end_;
    bool failed_ = false;
};
