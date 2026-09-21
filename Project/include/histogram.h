#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include "audio.h"

typedef struct {
    double bin_width;
    int num_bins;
    int *counts;
} Histogram;

void histogram_create(
    Histogram *hist,
    double min,
    double max,
    double bin_width
);

void histogram_add(
    Histogram *hist,
    double value,
    double min
);

void histogram_free(Histogram *hist);

void create_histograms(
    const Audio *audio,
    int bits,
    Histogram *left,
    Histogram *right,
    Histogram *mid,
    Histogram *side
);

#endif