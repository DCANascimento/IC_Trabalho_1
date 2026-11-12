#ifndef WAV_H
#define WAV_H

#include "audio.h"

#ifdef __cplusplus
extern "C" {
#endif

int wav_read(const char *filename, Audio *audio);

int wav_write_16(
	const char *filename,
	const Audio *audio,
	const int16_t *samples
);

#ifdef __cplusplus
}
#endif

#endif