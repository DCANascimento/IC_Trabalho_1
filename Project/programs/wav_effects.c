#include "effects.h"
#include "wav.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <direct.h>
#endif

#define INPUT_DIRECTORY "../AudioFiles"
#define OUTPUT_DIRECTORY "EffectsFiles"
#define MAX_PATH_LENGTH 2048

static int has_wav_extension(const char *name)
{
    size_t length = strlen(name);
    if (length < 4 || name[length - 4] != '.') {
        return 0;
    }
    return tolower((unsigned char)name[length - 3]) == 'w' &&
        tolower((unsigned char)name[length - 2]) == 'a' &&
        tolower((unsigned char)name[length - 1]) == 'v';
}

static int compare_names(const void *first, const void *second)
{
    return strcmp(*(char *const *)first, *(char *const *)second);
}

static char **list_wav_files(size_t *count)
{
    DIR *directory = opendir(INPUT_DIRECTORY);
    struct dirent *entry;
    char **names = NULL;
    *count = 0;

    if (directory == NULL) {
        return NULL;
    }

    while ((entry = readdir(directory)) != NULL) {
        if (!has_wav_extension(entry->d_name)) {
            continue;
        }

        char **resized = realloc(names, (*count + 1) * sizeof(*names));
        if (resized == NULL) {
            for (size_t i = 0; i < *count; i++) free(names[i]);
            free(names);
            closedir(directory);
            *count = 0;
            return NULL;
        }
        names = resized;
        size_t length = strlen(entry->d_name) + 1;
        names[*count] = malloc(length);
        if (names[*count] == NULL) {
            for (size_t i = 0; i < *count; i++) free(names[i]);
            free(names);
            closedir(directory);
            *count = 0;
            return NULL;
        }
        memcpy(names[*count], entry->d_name, length);
        (*count)++;
    }

    closedir(directory);
    qsort(names, *count, sizeof(*names), compare_names);
    return names;
}

static void free_names(char **names, size_t count)
{
    for (size_t i = 0; i < count; i++) free(names[i]);
    free(names);
}

static int prompt_integer(const char *prompt, int *value)
{
    char line[128];
    char *end;

    for (;;) {
        printf("%s", prompt);
        if (fgets(line, sizeof(line), stdin) == NULL) return 0;
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "q") == 0 || strcmp(line, "Q") == 0) return 0;

        long parsed = strtol(line, &end, 10);
        while (*end == ' ' || *end == '\t') end++;
        if (end != line && *end == '\0' && parsed >= -2147483647L &&
            parsed <= 2147483647L) {
            *value = (int)parsed;
            return 1;
        }
        printf("Enter an integer, or q to quit.\n");
    }
}

static int prompt_frequency(const char *prompt, double minimum,
    double maximum, int maximum_inclusive, double *value)
{
    char line[128];
    char *end;

    for (;;) {
        printf("%s", prompt);
        if (fgets(line, sizeof(line), stdin) == NULL) return 0;
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "q") == 0 || strcmp(line, "Q") == 0) return 0;

        double parsed = strtod(line, &end);
        while (*end == ' ' || *end == '\t') end++;
        if (end != line && *end == '\0' && isfinite(parsed) &&
            parsed >= minimum &&
            (maximum_inclusive ? parsed <= maximum : parsed < maximum)) {
            *value = parsed;
            return 1;
        }
        printf("Frequency is outside the allowed range.\n");
    }
}

static int choose_source(char **names, size_t count, char *path, size_t path_size)
{
    printf("Available WAV files in %s:\n", INPUT_DIRECTORY);
    for (size_t i = 0; i < count; i++) {
        printf("  %2lu - %s\n", (unsigned long)(i + 1), names[i]);
    }

    int selection;
    for (;;) {
        if (!prompt_integer("Choose a file (0 or q to exit): ", &selection)) {
            return 0;
        }
        if (selection == 0) return 0;
        if (selection < 1 || (size_t)selection > count) {
            printf("Choose a number from 1 to %lu.\n", (unsigned long)count);
            continue;
        }
        int length = snprintf(path, path_size, "%s/%s",
            INPUT_DIRECTORY, names[selection - 1]);
        return length > 0 && (size_t)length < path_size;
    }
}

static int select_effect(AudioEffect *effect)
{
    int category;
    printf("\nAUDIO EFFECTS\n");
    printf("1. Single echo\n");
    printf("2. Multiple echoes\n");
    printf("3. Amplitude modulation\n");
    printf("4. Time-varying delays\n");
    printf("5. Exit\n");
    if (!prompt_integer("Option: ", &category)) return 0;

    int option;
    switch (category) {
    case 1:
        printf("1. Echo 1 (450 ms, gain 0.75)\n");
        printf("2. Echo 2 (750 ms, gain 0.60)\n");
        printf("3. Echo 3 (300 ms, gain 0.90)\n");
        if (!prompt_integer("Echo: ", &option)) return 0;
        if (option >= 1 && option <= 3) {
            *effect = (AudioEffect)(AUDIO_EFFECT_ECHO_1 + option - 1);
            return 1;
        }
        break;
    case 2:
        printf("1. Feedforward echoes\n2. Feedback echoes\n");
        if (!prompt_integer("Multiple echo type: ", &option)) return 0;
        if (option == 1) *effect = AUDIO_EFFECT_MULTIPLE_FEEDFORWARD;
        else if (option == 2) *effect = AUDIO_EFFECT_MULTIPLE_FEEDBACK;
        else break;
        return 1;
    case 3:
        printf("1. Classic AM\n2. Tremolo\n");
        if (!prompt_integer("Modulation type: ", &option)) return 0;
        if (option == 1) *effect = AUDIO_EFFECT_AM;
        else if (option == 2) *effect = AUDIO_EFFECT_TREMOLO;
        else break;
        return 1;
    case 4:
        printf("1. Vibrato\n2. Chorus\n3. Flanger\n4. Doppler\n");
        if (!prompt_integer("Delay effect: ", &option)) return 0;
        if (option >= 1 && option <= 4) {
            *effect = (AudioEffect)(AUDIO_EFFECT_VIBRATO + option - 1);
            return 1;
        }
        break;
    case 5:
        return 0;
    default:
        break;
    }

    printf("Invalid option.\n");
    return -1;
}

static int get_effect_parameters(
    AudioEffect effect,
    const Audio *audio,
    double *frequency,
    double *depth)
{
    char prompt[160];
    double nyquist = audio->sample_rate / 2.0;
    *frequency = 5.0;
    *depth = 5.0;

    if (effect == AUDIO_EFFECT_AM) {
        snprintf(prompt, sizeof(prompt),
            "Carrier frequency (20 to less than %.1f Hz, q cancels): ", nyquist);
        return prompt_frequency(prompt, 20.0, nyquist, 0, frequency);
    }
    if (effect == AUDIO_EFFECT_TREMOLO) {
        return prompt_frequency("Tremolo frequency (1-15 Hz, q cancels): ",
            1.0, 15.0, 1, frequency);
    }
    if (effect == AUDIO_EFFECT_VIBRATO) {
        if (!prompt_frequency("Vibrato frequency (1-10 Hz, q cancels): ",
                1.0, 10.0, 1, frequency)) return 0;
        return prompt_frequency("Vibrato depth (1-10 ms, q cancels): ",
            1.0, 10.0, 1, depth);
    }
    if (effect == AUDIO_EFFECT_CHORUS) {
        return prompt_frequency("Chorus frequency (0.2-3 Hz, q cancels): ",
            0.2, 3.0, 1, frequency);
    }
    if (effect == AUDIO_EFFECT_FLANGER) {
        return prompt_frequency("Flanger frequency (0.1-2 Hz, q cancels): ",
            0.1, 2.0, 1, frequency);
    }
    return 1;
}

static void make_output_path(const char *input, AudioEffect effect,
    char *output, size_t output_size)
{
    const char *base = strrchr(input, '/');
    const char *windows_base = strrchr(input, '\\');
    if (windows_base != NULL && (base == NULL || windows_base > base)) {
        base = windows_base;
    }
    base = base == NULL ? input : base + 1;

    char stem[512];
    snprintf(stem, sizeof(stem), "%s", base);
    char *extension = strrchr(stem, '.');
    if (extension != NULL) *extension = '\0';
    snprintf(output, output_size, "%s/%s_%s.wav",
        OUTPUT_DIRECTORY, stem, audio_effect_name(effect));
}

static void try_play_file(const char *path)
{
#ifndef _WIN32
    pid_t child = fork();
    if (child == 0) {
        execlp("aplay", "aplay", "-q", path, (char *)NULL);
        execlp("paplay", "paplay", path, (char *)NULL);
        _exit(127);
    }
    if (child > 0) {
        int status;
        waitpid(child, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
            printf("Playback unavailable (install aplay or paplay).\n");
        }
    }
#else
    (void)path;
    printf("Playback is not configured for this terminal build.\n");
#endif
}

int main(void)
{
    size_t file_count;
    char **files = list_wav_files(&file_count);
    if (files == NULL || file_count == 0) {
        fprintf(stderr, "No WAV files found in %s. Run from Project/.\n",
            INPUT_DIRECTORY);
        free_names(files, file_count);
        return 1;
    }

    char input_path[MAX_PATH_LENGTH];
    if (!choose_source(files, file_count, input_path, sizeof(input_path))) {
        free_names(files, file_count);
        return 0;
    }
    free_names(files, file_count);

    Audio input = {0};
    if (wav_read(input_path, &input) != 0) {
        return 1;
    }

    for (;;) {
        AudioEffect effect;
        int selection = select_effect(&effect);
        if (selection == 0) break;
        if (selection < 0) continue;

        double frequency;
        double depth;
        if (!get_effect_parameters(effect, &input, &frequency, &depth)) {
            continue;
        }

        Audio output = {0};
        char error[160];
        if (audio_apply_effect(
                &input, &output, effect, frequency, depth,
                error, sizeof(error)) != 0) {
            fprintf(stderr, "Effect failed: %s\n", error);
            continue;
        }

#ifdef _WIN32
        _mkdir(OUTPUT_DIRECTORY);
#else
        mkdir(OUTPUT_DIRECTORY, 0755);
#endif
        char output_path[MAX_PATH_LENGTH];
        make_output_path(input_path, effect, output_path, sizeof(output_path));
        if (audio_effect_write_wav(output_path, &output) != 0) {
            fprintf(stderr, "Could not save %s\n", output_path);
            audio_free(&output);
            continue;
        }

        printf("Saved: %s\n", output_path);
        try_play_file(output_path);
        audio_free(&output);
    }

    audio_free(&input);
    return 0;
}
