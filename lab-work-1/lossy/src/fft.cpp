#include "fft.hpp"

#include <cmath>
#include <utility>

Fft::Fft(size_t n) : n_(n), bitReversed_(n), twiddles_(n / 2)
{
    const double pi = std::acos(-1.0);

    int bits = 0;
    while ((size_t(1) << bits) < n)
        bits++;

    for (size_t i = 0; i < n; i++)
    {
        size_t r = 0;
        for (int b = 0; b < bits; b++)
            if (i >> b & 1)
                r |= size_t(1) << (bits - 1 - b);
        bitReversed_[i] = r;
    }

    for (size_t k = 0; k < n / 2; k++)
        twiddles_[k] = std::polar(1.0, -2 * pi * k / n);
}

void Fft::transform(Complex* data) const
{
    // Reorder the input so that the butterflies can work in place.
    for (size_t i = 0; i < n_; i++)
        if (i < bitReversed_[i])
            std::swap(data[i], data[bitReversed_[i]]);

    // Combine pairs of FFTs of size len/2 into FFTs of size len.
    for (size_t len = 2; len <= n_; len *= 2)
    {
        const size_t half = len / 2;
        const size_t step = n_ / len;

        for (size_t start = 0; start < n_; start += len)
        {
            for (size_t j = 0; j < half; j++)
            {
                const Complex u = data[start + j];
                const Complex v = mul(data[start + j + half], twiddles_[j * step]);
                data[start + j] = u + v;
                data[start + j + half] = u - v;
            }
        }
    }
}
