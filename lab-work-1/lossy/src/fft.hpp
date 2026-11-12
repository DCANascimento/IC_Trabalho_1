#pragma once

#include <complex>
#include <cstddef>
#include <vector>

using Complex = std::complex<double>;

// Complex product written out by hand: std::complex's operator* also handles
// infinities and NaN, which makes it several times slower.
inline Complex mul(Complex a, Complex b)
{
    return {a.real() * b.real() - a.imag() * b.imag(),
            a.real() * b.imag() + a.imag() * b.real()};
}

// In-place radix-2 FFT of a fixed size (a power of two):
//   X[k] = sum_{n=0}^{N-1} x[n] * exp(-2*pi*i*n*k / N)
class Fft
{
public:
    explicit Fft(size_t n);

    void transform(Complex* data) const;

private:
    size_t n_;
    std::vector<size_t> bitReversed_;   // input permutation
    std::vector<Complex> twiddles_;     // exp(-2*pi*i*k / N), k < N/2
};
