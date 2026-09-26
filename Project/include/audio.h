#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    double *samples;
    int16_t *samples_16;
    size_t num_samples;
    int sample_rate;
    int num_channels;
} Audio;

void audio_free(Audio *audio);

#endif