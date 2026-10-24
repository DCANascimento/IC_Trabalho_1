# Lossy codec vs MP3, AAC and Opus

Files:
- sample01.wav
- sample02.wav
- sample03.wav
- sample04.wav
- sample05.wav
- sample06.wav
- sample07.wav

Every value is the mean over the files. Versions: ffmpeg version 6.1.1-3ubuntu5.

## SNR (dB) at the same target bitrate (higher is better)

| target bitrate (kbps) | ours (v2) | MP3 (LAME) | AAC (FFmpeg) | Opus |
|---:|---:|---:|---:|---:|
| 32 | 16.43 | 10.28 | 7.53 | 10.16 |
| 48 | 20.63 | 13.28 | 9.82 | 12.63 |
| 64 | 24.00 | 15.16 | 11.02 | 14.55 |
| 96 | 29.38 | 17.59 | 14.70 | 18.01 |
| 128 | 33.56 | 19.58 | 18.63 | 20.36 |
| 160 | 36.77 | 21.42 | 21.57 | 21.84 |
| 192 | 39.48 | 25.30 | 23.93 | 23.37 |
| 256 | 44.76 | 32.41 | 28.41 | 25.90 |
| 320 | 49.89 | 37.21 | 33.34 | 27.90 |

## Real bitrate (kbps) and compression ratio

The bitrate is measured from the size of the compressed file (container included).

| target bitrate (kbps) | ours (v2) kbps | ratio | MP3 (LAME) kbps | ratio | AAC (FFmpeg) kbps | ratio | Opus kbps | ratio |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 32 | 32.0 | 44.10 | 32.2 | 43.87 | 33.1 | 42.64 | 33.6 | 42.54 |
| 48 | 48.0 | 29.40 | 48.2 | 29.28 | 49.6 | 28.50 | 50.4 | 28.33 |
| 64 | 64.0 | 22.05 | 64.2 | 21.97 | 65.5 | 21.58 | 67.3 | 21.21 |
| 96 | 96.0 | 14.70 | 96.3 | 14.65 | 97.3 | 14.53 | 100.6 | 14.19 |
| 128 | 128.0 | 11.03 | 128.4 | 10.99 | 129.8 | 10.89 | 134.4 | 10.62 |
| 160 | 160.0 | 8.82 | 160.6 | 8.79 | 161.5 | 8.75 | 167.5 | 8.51 |
| 192 | 192.0 | 7.35 | 192.7 | 7.32 | 194.1 | 7.28 | 199.7 | 7.13 |
| 256 | 256.0 | 5.51 | 256.9 | 5.49 | 257.2 | 5.50 | 264.4 | 5.38 |
| 320 | 320.0 | 4.41 | 321.1 | 4.39 | 314.9 | 4.50 | 325.8 | 4.36 |

## Time and memory at 128 kbps

Average per file (20.5 s of audio on average). Peak resident memory of the program.

| codec | encoding time (s) | decoding time (s) | encoder memory (MB) | decoder memory (MB) |
|---|---:|---:|---:|---:|
| ours (v2) | 0.923 | 0.041 | 17.0 | 14.1 |
| MP3 (LAME) | 0.349 | 0.061 | 51.9 | 52.1 |
| AAC (FFmpeg) | 0.448 | 0.061 | 52.1 | 52.5 |
| Opus | 0.237 | 0.103 | 52.3 | 52.0 |
| ours, one encoding with the step found by `-b` (`-q`) | 0.064 | — | 17.2 | — |
| ffmpeg without codec (WAV → WAV, reference) | 0.061 | — | 51.3 | — |

## BD-rate against our codec (equal SNR)

Positive: the codec needs more bits than ours for the same SNR.
Only the SNR range shared with our codec is used.

| codec | BD-rate (bits at equal SNR, %) | BD-SNR (SNR at equal bitrate, dB) |
|---|---:|---:|
| MP3 (LAME) | +148.14 | -11.141 |
| AAC (FFmpeg) | +196.00 | -13.933 |
| Opus | +215.40 | -12.885 |

## How it was measured

* MP3, AAC and Opus are encoded by ffmpeg with its default settings for each codec and
  bitrate (`-b:a`), into the usual file formats: .mp3, .m4a and .opus.
* These formats store the encoder delay, so ffmpeg removes it when decoding. The script
  checks once per codec that the decoded file is aligned with the original (shift 0);
  without this the SNR would be meaningless.
* The decoded file is cut to the exact length of the original (AAC adds padding at the
  end), at the original sample rate. Opus always decodes at 48 kHz, so it is resampled
  to the original rate. The resampling alone (to 48 kHz and back, sample01.wav)
  has an SNR of 65.95 dB, so it does not limit the SNR of Opus.
* The bitrate and the ratio use the real size of the file, container headers included.
  The real bitrate can be a few % above the target (Opus is variable-bitrate by default;
  .m4a and .opus have container headers; ours has a 30-byte header). The SNR table is at
  the target bitrate, which slightly favours the codecs that use more bits; the BD-rate
  and the figure use the real bitrates.
* AAC uses FFmpeg's own encoder; the best AAC encoder (fdk-aac) is not included in
  Ubuntu's ffmpeg because of its licence, and would do somewhat better.
* Time and memory are measured for the whole program. ffmpeg is a large program: the
  reference row (WAV → WAV without any codec) shows how much of it is not the codec.
  Our encoder with `-b` runs 16 encodings to find the step (rate control); the row
  with `-q` is the time of one encoding, comparable to the one-pass MPEG encoders.
* The SNR is not perceived quality: MP3, AAC and Opus put the noise where it is not
  heard (psychoacoustic model) and remove inaudible high frequencies. They are expected
  to have a lower SNR than our codec and still sound better at the same bitrate.
