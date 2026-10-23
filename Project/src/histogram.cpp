extern "C" {
#include "histogram.h"
}

#include <cstdlib>
#include <cmath>

void histogram_create(
    Histogram *hist,
    double min,
    double max,
    double bin_width)
{
    hist->bin_width = bin_width;

    hist->num_bins =
        static_cast<int>(std::ceil((max - min) / bin_width));

    hist->counts =
        static_cast<int *>(std::calloc(hist->num_bins, sizeof(int)));
}

void histogram_add(
    Histogram *hist,
    double value,
    double min)
{
    int bin =
        static_cast<int>((value - min) / hist->bin_width);

    if (bin >= 0 && bin < hist->num_bins) {
        hist->counts[bin]++;
    }
}

void histogram_free(Histogram *hist)
{
    std::free(hist->counts);
}

void create_histograms(
    const Audio *audio,
    int bits,
    Histogram *left,
    Histogram *right,
    Histogram *mid,
    Histogram *side)
{
    double bin_width =
        static_cast<double>(1 << bits) /
        audio->sample_rate;

    double min = -1.0;
    double max = 1.0;

    histogram_create(left, min, max, bin_width);
    histogram_create(right, min, max, bin_width);
    histogram_create(mid, min, max, bin_width);
    histogram_create(side, min, max, bin_width);

    for (size_t i = 0;
         i < audio->num_samples;
         i += 2) {

        double L = audio->samples[i];
        double R = audio->samples[i + 1];

        double M = (L + R) / 2.0;
        double S = (L - R) / 2.0;

        histogram_add(left, L, min);
        histogram_add(right, R, min);
        histogram_add(mid, M, min);
        histogram_add(side, S, min);
    }
}