#include "wav.h"
#include "histogram.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    /*
     * Expected:
     *
     * ./wav_hist input.wav --bits 10
     */

    if (argc != 4) {
        printf(
            "Usage: %s input.wav --bits N\n",
            argv[0]
        );

        return 1;
    }

    const char *filename = argv[1];

    if (strcmp(argv[2], "--bits") != 0) {
        printf("Expected --bits\n");
        return 1;
    }

    int bits = atoi(argv[3]);

    if (bits <= 0) {
        printf("Bits must be greater than 0\n");
        return 1;
    }


    /* Read WAV */

    Audio audio;

    if (wav_read(filename, &audio) != 0) {
        return 1;
    }

    if (audio.num_channels != 2) {
        printf("Histogram mode requires a stereo WAV file\n");
        audio_free(&audio);
        return 1;
    }


    /* Create histograms */

    Histogram left;
    Histogram right;
    Histogram mid;
    Histogram side;

    create_histograms(
        &audio,
        bits,
        &left,
        &right,
        &mid,
        &side
    );


    /* Free memory */

    histogram_free(&left);
    histogram_free(&right);
    histogram_free(&mid);
    histogram_free(&side);

    audio_free(&audio);

    return 0;
}