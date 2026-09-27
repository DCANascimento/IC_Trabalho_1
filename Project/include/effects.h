#ifndef EFFECTS_H
#define EFFECTS_H

#include "audio.h"

typedef enum {
    AUDIO_EFFECT_ECHO_1 = 1,
    AUDIO_EFFECT_ECHO_2,
    AUDIO_EFFECT_ECHO_3,
    AUDIO_EFFECT_MULTIPLE_FEEDFORWARD,
    AUDIO_EFFECT_MULTIPLE_FEEDBACK,
    AUDIO_EFFECT_AM,
    AUDIO_EFFECT_TREMOLO,
    AUDIO_EFFECT_VIBRATO,
    AUDIO_EFFECT_CHORUS,
    AUDIO_EFFECT_FLANGER,
    AUDIO_EFFECT_DOPPLER
} AudioEffect;

int audio_apply_effect(
    const Audio *input,
    Audio *output,
    AudioEffect effect,
    double frequency_hz,
    double depth_ms,
    char *error_message,
    size_t error_message_size
);

const char *audio_effect_name(AudioEffect effect);

int audio_effect_write_wav(const char *filename, const Audio *audio);

#endif