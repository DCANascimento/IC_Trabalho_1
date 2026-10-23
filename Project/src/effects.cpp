extern "C" {
#include "effects.h"
#include "wav.h"
}

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define PI 3.14159265358979323846

static int set_error(char *message, size_t size, const char *text)
{
    if (message != nullptr && size > 0) {
        std::snprintf(message, size, "%s", text);
    }
    return 1;
}

static size_t frames_in(const Audio *audio)
{
    return audio->num_samples / static_cast<size_t>(audio->num_channels);
}

static int allocate_audio(Audio *audio, size_t frames, int channels, int rate)
{
    if (frames > static_cast<size_t>(-1) / static_cast<size_t>(channels) ||
        frames * static_cast<size_t>(channels) > static_cast<size_t>(-1) / sizeof(double)) {
        return 1;
    }

    audio->num_samples = frames * static_cast<size_t>(channels);
    audio->num_channels = channels;
    audio->sample_rate = rate;
    audio->samples_16 = nullptr;
    audio->samples = static_cast<double *>(std::calloc(audio->num_samples, sizeof(double)));
    return audio->samples == nullptr;
}

static void normalize(Audio *audio)
{
    double peak = 0.0;

    for (size_t i = 0; i < audio->num_samples; i++) {
        double value = std::fabs(audio->samples[i]);
        if (value > peak) {
            peak = value;
        }
    }

    if (peak > 1.0) {
        for (size_t i = 0; i < audio->num_samples; i++) {
            audio->samples[i] /= peak;
        }
    }
}

static double sample_at(const Audio *audio, int channel, double position)
{
    size_t frames = frames_in(audio);
    if (position < 0.0 || position > static_cast<double>(frames - 1)) {
        return 0.0;
    }

    long left = static_cast<long>(std::floor(position));
    double fraction = position - static_cast<double>(left);
    double first = 0.0;
    double second = 0.0;

    if (left >= 0 && static_cast<size_t>(left) < frames) {
        first = audio->samples[static_cast<size_t>(left) * static_cast<size_t>(audio->num_channels) +
            static_cast<size_t>(channel)];
    }
    if (left + 1 >= 0 && static_cast<size_t>(left + 1) < frames) {
        second = audio->samples[static_cast<size_t>(left + 1) *
            static_cast<size_t>(audio->num_channels) + static_cast<size_t>(channel)];
    }

    return first + fraction * (second - first);
}

const char *audio_effect_name(AudioEffect effect)
{
    switch (effect) {
    case AUDIO_EFFECT_ECHO_1: return "echo1";
    case AUDIO_EFFECT_ECHO_2: return "echo2";
    case AUDIO_EFFECT_ECHO_3: return "echo3";
    case AUDIO_EFFECT_MULTIPLE_FEEDFORWARD: return "multiecho_ff";
    case AUDIO_EFFECT_MULTIPLE_FEEDBACK: return "multiecho_fb";
    case AUDIO_EFFECT_AM: return "am";
    case AUDIO_EFFECT_TREMOLO: return "tremolo";
    case AUDIO_EFFECT_VIBRATO: return "vibrato";
    case AUDIO_EFFECT_CHORUS: return "chorus";
    case AUDIO_EFFECT_FLANGER: return "flanger";
    case AUDIO_EFFECT_DOPPLER: return "doppler";
    default: return "effect";
    }
}

static int validate_parameters(
    const Audio *input,
    AudioEffect effect,
    double frequency,
    double depth,
    char *error_message,
    size_t error_message_size)
{
    double nyquist = input->sample_rate / 2.0;

    if (effect == AUDIO_EFFECT_AM &&
        (frequency < 20.0 || frequency >= nyquist)) {
        return set_error(error_message, error_message_size,
            "AM carrier must be at least 20 Hz and below Nyquist.");
    }
    if (effect == AUDIO_EFFECT_TREMOLO &&
        (frequency < 1.0 || frequency > 15.0)) {
        return set_error(error_message, error_message_size,
            "Tremolo frequency must be between 1 and 15 Hz.");
    }
    if (effect == AUDIO_EFFECT_VIBRATO &&
        (frequency < 1.0 || frequency > 10.0 ||
         depth < 1.0 || depth > 10.0)) {
        return set_error(error_message, error_message_size,
            "Vibrato requires 1-10 Hz frequency and 1-10 ms depth.");
    }
    if (effect == AUDIO_EFFECT_CHORUS &&
        (frequency < 0.2 || frequency > 3.0)) {
        return set_error(error_message, error_message_size,
            "Chorus frequency must be between 0.2 and 3 Hz.");
    }
    if (effect == AUDIO_EFFECT_FLANGER &&
        (frequency < 0.1 || frequency > 2.0)) {
        return set_error(error_message, error_message_size,
            "Flanger frequency must be between 0.1 and 2 Hz.");
    }
    return 0;
}

int audio_apply_effect(
    const Audio *input,
    Audio *output,
    AudioEffect effect,
    double frequency_hz,
    double depth_ms,
    char *error_message,
    size_t error_message_size)
{
    if (input == nullptr || output == nullptr || input->samples == nullptr ||
        input->num_channels <= 0 || input->sample_rate <= 0 ||
        input->num_samples == 0 ||
        input->num_samples % static_cast<size_t>(input->num_channels) != 0) {
        return set_error(error_message, error_message_size,
            "Input audio is empty or invalid.");
    }

    if (validate_parameters(input, effect, frequency_hz, depth_ms,
            error_message, error_message_size) != 0) {
        return 1;
    }

    size_t frames = frames_in(input);
    int channels = input->num_channels;
    int rate = input->sample_rate;
    size_t delay_samples = 0;
    size_t output_frames = frames;

    switch (effect) {
    case AUDIO_EFFECT_ECHO_1:
    case AUDIO_EFFECT_ECHO_2:
    case AUDIO_EFFECT_ECHO_3:
        delay_samples = static_cast<size_t>(std::llround(
            (effect == AUDIO_EFFECT_ECHO_1 ? 0.45 :
             effect == AUDIO_EFFECT_ECHO_2 ? 0.75 : 0.30) * rate));
        output_frames += delay_samples;
        break;
    case AUDIO_EFFECT_MULTIPLE_FEEDFORWARD:
        delay_samples = static_cast<size_t>(std::llround(0.35 * rate));
        output_frames += 5 * delay_samples;
        break;
    case AUDIO_EFFECT_MULTIPLE_FEEDBACK:
        delay_samples = static_cast<size_t>(std::llround(0.35 * rate));
        output_frames += static_cast<size_t>(std::ceil(std::log(0.001) / std::log(0.60))) * delay_samples;
        break;
    case AUDIO_EFFECT_AM:
    case AUDIO_EFFECT_TREMOLO:
    case AUDIO_EFFECT_VIBRATO:
    case AUDIO_EFFECT_CHORUS:
    case AUDIO_EFFECT_FLANGER:
    case AUDIO_EFFECT_DOPPLER:
        if (effect == AUDIO_EFFECT_DOPPLER && frames < 2) {
            return set_error(error_message, error_message_size,
                "Audio is too short to apply Doppler.");
        }
        break;
    default:
        return set_error(error_message, error_message_size,
            "Unknown audio effect.");
    }

    if (allocate_audio(output, output_frames, channels, rate) != 0) {
        return set_error(error_message, error_message_size,
            "Could not allocate effect output.");
    }

    for (size_t i = 0; i < input->num_samples; i++) {
        output->samples[i] = input->samples[i];
    }

    if (effect == AUDIO_EFFECT_ECHO_1 ||
        effect == AUDIO_EFFECT_ECHO_2 ||
        effect == AUDIO_EFFECT_ECHO_3) {
        double gain = effect == AUDIO_EFFECT_ECHO_1 ? 0.75 :
            effect == AUDIO_EFFECT_ECHO_2 ? 0.60 : 0.90;
        for (size_t frame = 0; frame < frames; frame++) {
            for (int channel = 0; channel < channels; channel++) {
                size_t destination = (frame + delay_samples) *
                    static_cast<size_t>(channels) + static_cast<size_t>(channel);
                size_t source = frame * static_cast<size_t>(channels) + static_cast<size_t>(channel);
                output->samples[destination] += gain * input->samples[source];
            }
        }
    } else if (effect == AUDIO_EFFECT_MULTIPLE_FEEDFORWARD) {
        const double gains[] = {0.75, 0.55, 0.40, 0.28, 0.18};
        for (size_t echo = 0; echo < 5; echo++) {
            for (size_t frame = 0; frame < frames; frame++) {
                for (int channel = 0; channel < channels; channel++) {
                    size_t destination = (frame + (echo + 1) * delay_samples) *
                        static_cast<size_t>(channels) + static_cast<size_t>(channel);
                    size_t source = frame * static_cast<size_t>(channels) + static_cast<size_t>(channel);
                    output->samples[destination] +=
                        gains[echo] * input->samples[source];
                }
            }
        }
    } else if (effect == AUDIO_EFFECT_MULTIPLE_FEEDBACK) {
        for (size_t frame = delay_samples; frame < output_frames; frame++) {
            for (int channel = 0; channel < channels; channel++) {
                size_t index = frame * static_cast<size_t>(channels) + static_cast<size_t>(channel);
                size_t delayed = (frame - delay_samples) *
                    static_cast<size_t>(channels) + static_cast<size_t>(channel);
                double source = frame < frames ? input->samples[index] : 0.0;
                output->samples[index] = source + 0.60 * output->samples[delayed];
            }
        }
    } else {
        for (size_t frame = 0; frame < frames; frame++) {
            double time = static_cast<double>(frame) / rate;
            double multiplier = 1.0;

            if (effect == AUDIO_EFFECT_AM) {
                multiplier = 1.0 + std::cos(2.0 * PI * frequency_hz * time);
            } else if (effect == AUDIO_EFFECT_TREMOLO) {
                multiplier = 1.0 + 0.85 * std::cos(2.0 * PI * frequency_hz * time);
            }

            double delay_seconds = 0.0;
            if (effect == AUDIO_EFFECT_VIBRATO) {
                delay_seconds = depth_ms / 1000.0 *
                    std::sin(2.0 * PI * frequency_hz * time);
            } else if (effect == AUDIO_EFFECT_CHORUS) {
                delay_seconds = 0.018 + 0.008 *
                    std::sin(2.0 * PI * frequency_hz * time);
            } else if (effect == AUDIO_EFFECT_FLANGER) {
                delay_seconds = 0.005 + 0.004 *
                    std::sin(2.0 * PI * frequency_hz * time);
            } else if (effect == AUDIO_EFFECT_DOPPLER) {
                double duration = static_cast<double>(frames - 1) / rate;
                delay_seconds = 0.040 + 0.025 *
                    std::sin(2.0 * PI * time / duration);
            }

            for (int channel = 0; channel < channels; channel++) {
                size_t index = frame * static_cast<size_t>(channels) + static_cast<size_t>(channel);
                if (effect == AUDIO_EFFECT_AM || effect == AUDIO_EFFECT_TREMOLO) {
                    output->samples[index] = input->samples[index] * multiplier;
                } else if (effect == AUDIO_EFFECT_VIBRATO) {
                    output->samples[index] = sample_at(
                        input, channel, static_cast<double>(frame) - delay_seconds * rate);
                } else if (effect == AUDIO_EFFECT_CHORUS) {
                    double delayed = sample_at(
                        input, channel, static_cast<double>(frame) - delay_seconds * rate);
                    output->samples[index] = 0.20 * input->samples[index] +
                        0.80 * delayed;
                } else if (effect == AUDIO_EFFECT_FLANGER) {
                    double delayed = sample_at(
                        input, channel, static_cast<double>(frame) - delay_seconds * rate);
                    output->samples[index] = input->samples[index] +
                        0.90 * delayed;
                } else if (effect == AUDIO_EFFECT_DOPPLER) {
                    output->samples[index] = sample_at(
                        input, channel, static_cast<double>(frame) - delay_seconds * rate);
                }
            }
        }
    }

    normalize(output);
    if (error_message != nullptr && error_message_size > 0) {
        error_message[0] = '\0';
    }
    return 0;
}

int audio_effect_write_wav(const char *filename, const Audio *audio)
{
    if (audio == nullptr || audio->samples == nullptr || audio->num_channels <= 0) {
        return 1;
    }

    int16_t *samples = static_cast<int16_t *>(std::malloc(audio->num_samples * sizeof(int16_t)));
    if (samples == nullptr) {
        return 1;
    }

    for (size_t i = 0; i < audio->num_samples; i++) {
        double value = audio->samples[i];
        if (value > 1.0) value = 1.0;
        if (value < -1.0) value = -1.0;
        long quantized = std::lrint(value * 32768.0);
        if (quantized > 32767) quantized = 32767;
        if (quantized < -32768) quantized = -32768;
        samples[i] = static_cast<int16_t>(quantized);
    }

    int result = wav_write_16(filename, audio, samples);
    std::free(samples);
    return result;
}