#include "dct.hpp"

#include <algorithm>
#include <cmath>

Dct::Dct(size_t n) : n_(n), matrix_(n * n)
{
    const double pi = std::acos(-1.0);

    for (size_t k = 0; k < n; k++)
    {
        const double scale = std::sqrt((k == 0 ? 1.0 : 2.0) / n);
        for (size_t i = 0; i < n; i++)
            matrix_[k * n + i] = scale * std::cos(pi / n * (i + 0.5) * k);
    }
}

void Dct::forward(const double* in, double* out)
{
    // out[k] = dot product of row k with the input.
    for (size_t k = 0; k < n_; k++)
    {
        const double* row = &matrix_[k * n_];
        double sum = 0.0;
        for (size_t i = 0; i < n_; i++)
            sum += row[i] * in[i];
        out[k] = sum;
    }
}

void Dct::inverse(const double* in, double* out)
{
    // out = sum over k of in[k] * (row k). Coefficients quantized to zero
    // are skipped, which makes decoding much faster.
    std::fill(out, out + n_, 0.0);

    for (size_t k = 0; k < n_; k++)
    {
        const double c = in[k];
        if (c == 0.0)
            continue;

        const double* row = &matrix_[k * n_];
        for (size_t i = 0; i < n_; i++)
            out[i] += c * row[i];
    }
}
