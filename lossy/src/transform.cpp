#include "transform.hpp"

#include "dct.hpp"
#include "mdct.hpp"

bool isValidBlockSize(size_t n)
{
    const bool powerOfTwo = (n & (n - 1)) == 0;
    return powerOfTwo && n >= MIN_BLOCK_SIZE && n <= MAX_BLOCK_SIZE;
}

std::unique_ptr<BlockTransform> makeTransform(TransformType type, size_t n)
{
    if (type == TransformType::Dct)
        return std::make_unique<Dct>(n);
    return std::make_unique<Mdct>(n);
}
