#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Bit-level writing and reading (most significant bit first), plus Rice codes.

// Zigzag maps signed to unsigned so that small magnitudes stay small:
// 0, -1, 1, -2, 2, ... -> 0, 1, 2, 3, 4, ...
inline uint32_t zigzagEncode(int32_t v)
{
    return (uint32_t(v) << 1) ^ uint32_t(v >> 31);
}

inline int32_t zigzagDecode(uint32_t u)
{
    return int32_t(u >> 1) ^ -int32_t(u & 1);
}

// Rice code of v with parameter k:  q = v >> k  zeros, a one, then the k low bits of v.
// Quotients >= RICE_ESCAPE are written as RICE_ESCAPE zeros followed by v in 32 bits,
// so that a rare large value cannot cost thousands of bits.
const uint32_t RICE_ESCAPE = 32;
const int MAX_RICE_PARAMETER = 30;

class BitWriter
{
public:
    std::vector<uint8_t> bytes;

    // Writes the 'count' low bits of value (count <= 32).
    void write(uint32_t value, int count)
    {
        acc_ = (acc_ << count) | (value & ((uint64_t(1) << count) - 1));
        bits_ += count;
        while (bits_ >= 8)
        {
            bits_ -= 8;
            bytes.push_back(uint8_t(acc_ >> bits_));
        }
    }

    void writeRice(uint32_t v, int k)
    {
        const uint32_t q = v >> k;
        if (q < RICE_ESCAPE)
        {
            write(1, int(q) + 1);   // q zeros and a one
            write(v, k);
        }
        else
        {
            write(0, RICE_ESCAPE);
            write(v, 32);
        }
    }

    // Pads the last byte with zeros.
    void flush()
    {
        if (bits_ > 0)
            write(0, 8 - bits_);
    }

private:
    uint64_t acc_ = 0;   // bits not yet stored in 'bytes' are the low 'bits_' bits
    int bits_ = 0;
};

// Reading past the end returns zeros and sets an error flag, checked with ok().
class BitReader
{
public:
    BitReader(const uint8_t* data, size_t size) : p_(data), end_(data + size) {}

    bool ok() const { return !failed_; }

    uint32_t read(int count)
    {
        while (bits_ < count)
        {
            uint8_t next = 0;
            if (p_ < end_)
                next = *p_++;
            else
                failed_ = true;
            acc_ = (acc_ << 8) | next;
            bits_ += 8;
        }
        bits_ -= count;
        return uint32_t((acc_ >> bits_) & ((uint64_t(1) << count) - 1));
    }

    uint32_t readRice(int k)
    {
        uint32_t q = 0;
        while (q < RICE_ESCAPE && read(1) == 0)
            q++;

        if (q == RICE_ESCAPE)
            return read(32);
        return (q << k) | read(k);
    }

private:
    const uint8_t* p_;
    const uint8_t* end_;
    uint64_t acc_ = 0;
    int bits_ = 0;
    bool failed_ = false;
};
