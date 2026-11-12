#include "mdct.hpp"

#include <cmath>

Mdct::Mdct(size_t n)
    : n_(n),
      scale_(std::sqrt(2.0 / n)),
      window_(2 * n),
      preTwiddle_(n / 2),
      postTwiddle_(n / 2),
      fft_(n / 2),
      folded_(n),
      buffer_(n / 2)
{
    const double pi = std::acos(-1.0);

    for (size_t i = 0; i < 2 * n; i++)
        window_[i] = std::sin(pi / (2 * n) * (i + 0.5));

    for (size_t j = 0; j < n / 2; j++)
    {
        preTwiddle_[j] = std::polar(1.0, -pi * (4 * j + 1) / (4 * n));
        postTwiddle_[j] = std::polar(1.0, -pi * j / n);
    }
}

void Mdct::dct4(const double* in, double* out)
{
    // Pack even inputs and reversed odd inputs into N/2 complex values,
    // so that a single FFT of size N/2 does all the work.
    for (size_t j = 0; j < n_ / 2; j++)
        buffer_[j] = mul(Complex(in[2 * j], in[n_ - 1 - 2 * j]), preTwiddle_[j]);

    fft_.transform(buffer_.data());

    // The real parts give the even outputs, the imaginary parts the odd ones.
    for (size_t k = 0; k < n_ / 2; k++)
    {
        const Complex z = mul(buffer_[k], postTwiddle_[k]);
        out[2 * k] = scale_ * z.real();
        out[n_ - 1 - 2 * k] = -scale_ * z.imag();
    }
}

void Mdct::forward(const double* in, double* out)
{
    const size_t h = n_ / 2;
    const double* w = window_.data();

    // Window and fold the quarters (a, b, c, d) into (-c_r - d, a - b_r).
    for (size_t i = 0; i < h; i++)
    {
        folded_[i] = -w[3 * h - 1 - i] * in[3 * h - 1 - i] - w[3 * h + i] * in[3 * h + i];
        folded_[h + i] = w[i] * in[i] - w[n_ - 1 - i] * in[n_ - 1 - i];
    }

    dct4(folded_.data(), out);
}

void Mdct::inverse(const double* in, double* out)
{
    const size_t h = n_ / 2;
    const double* u = folded_.data();

    dct4(in, folded_.data());

    // Unfold u = (u1, u2) into (u2, -u2_r, -u1_r, -u1) and apply the window.
    for (size_t i = 0; i < h; i++)
    {
        out[i] = u[h + i];
        out[h + i] = -u[n_ - 1 - i];
        out[n_ + i] = -u[h - 1 - i];
        out[3 * h + i] = -u[i];
    }

    for (size_t i = 0; i < 2 * n_; i++)
        out[i] *= window_[i];
}
