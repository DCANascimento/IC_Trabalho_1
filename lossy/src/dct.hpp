#pragma once

#include "transform.hpp"

#include <vector>

// Orthonormal DCT-II of a fixed size N and its inverse (DCT-III), on
// non-overlapping blocks.
//
//   X[k] = s(k) * sum_{i=0}^{N-1} x[i] * cos(pi/N * (i + 1/2) * k)
//   s(0) = sqrt(1/N),  s(k > 0) = sqrt(2/N)
//
// Being orthonormal, the inverse uses the same matrix transposed and the energy
// is preserved: sum x[i]^2 == sum X[k]^2.
//
// Multiplies by a precomputed N x N matrix: O(N^2) per block. Only used to
// compare with the MDCT, so it is not optimized.
class Dct : public BlockTransform
{
public:
    explicit Dct(size_t n);

    size_t blockSize() const override { return n_; }
    size_t inputSize() const override { return n_; }

    void forward(const double* in, double* out) override;
    void inverse(const double* in, double* out) override;

private:
    size_t n_;
    std::vector<double> matrix_;   // matrix_[k * N + i] = s(k) * cos(pi/N * (i + 1/2) * k)
};
