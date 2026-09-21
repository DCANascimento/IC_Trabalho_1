#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>

typedef struct {
    double *samples;
    size_t num_samples;
    int sample_rate;
    int num_channels;
} Audio;

void audio_free(Audio *audio);

#endif