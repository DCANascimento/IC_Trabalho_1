#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

// A transform that turns blocks of samples into N coefficients.
// Consecutive blocks start N samples apart (the hop size). When inputSize() > N
// the blocks overlap, and the decoder must add the overlapping outputs together.
class BlockTransform
{
public:
    virtual ~BlockTransform() = default;

    virtual size_t blockSize() const = 0;   // N: coefficients per block
    virtual size_t inputSize() const = 0;   // samples that go into one block

    virtual void forward(const double* in, double* out) = 0;   // inputSize() -> N
    virtual void inverse(const double* in, double* out) = 0;   // N -> inputSize()
};

enum class TransformType : uint8_t
{
    Dct = 0,    // DCT-II on non-overlapping blocks (step 1, kept for comparison)
    Mdct = 1,   // MDCT with a sine window and 50% overlap
};

const size_t MIN_BLOCK_SIZE = 16;
const size_t MAX_BLOCK_SIZE = 4096;

// Block sizes must be powers of two in [MIN_BLOCK_SIZE, MAX_BLOCK_SIZE].
bool isValidBlockSize(size_t n);

std::unique_ptr<BlockTransform> makeTransform(TransformType type, size_t n);
