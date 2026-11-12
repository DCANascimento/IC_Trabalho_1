#pragma once

#include "fft.hpp"
#include "transform.hpp"

#include <vector>

// MDCT (Modified Discrete Cosine Transform): 2N windowed samples -> N coefficients.
//
//   X[k] = sqrt(2/N) * sum_{i=0}^{2N-1} w[i] x[i] cos(pi/N * (i + 1/2 + N/2) * (k + 1/2))
//
// Blocks overlap by 50% (hop of N samples). The inverse of one block alone contains
// time-domain aliasing, which cancels out when the outputs of consecutive blocks
// are added (TDAC). With the sine window, w[i]^2 + w[i+N]^2 = 1 (Princen-Bradley),
// so the reconstruction is perfect and the transform is orthonormal.
//
// Fast algorithm, O(N log N):
//   the 2N windowed inputs, split in quarters (a, b, c, d), are folded into the
//   N values (-c_r - d, a - b_r)  (_r = reversed); the MDCT is the DCT-IV of these,
//   and the DCT-IV is computed with a complex FFT of size N/2.
class Mdct : public BlockTransform
{
public:
    explicit Mdct(size_t n);

    size_t blockSize() const override { return n_; }
    size_t inputSize() const override { return 2 * n_; }

    void forward(const double* in, double* out) override;
    void inverse(const double* in, double* out) override;

private:
    // Orthonormal DCT-IV: X[k] = sqrt(2/N) * sum u[i] cos(pi/N * (i + 1/2) * (k + 1/2)).
    // It is its own inverse.
    void dct4(const double* in, double* out);

    size_t n_;
    double scale_;
    std::vector<double> window_;       // 2N sine window
    std::vector<Complex> preTwiddle_;  // N/2 values, applied before the FFT
    std::vector<Complex> postTwiddle_; // N/2 values, applied after the FFT
    Fft fft_;
    std::vector<double> folded_;       // N, scratch
    std::vector<Complex> buffer_;      // N/2, scratch
};
