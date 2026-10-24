# Lossy codec: version 1 vs version 2

Files:
- sample01.wav
- sample02.wav
- sample03.wav
- sample04.wav
- sample05.wav
- sample06.wav
- sample07.wav

Every row is the mean over the files.

## Version 1

| step q | bitrate (kbps) | compression ratio | SNR (dB) | encoding time (s) | decoding time (s) | encoder memory (MB) | decoder memory (MB) |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 627.4 | 2.40 | 58.16 | 0.043 | 0.040 | 17.0 | 15.2 |
| 32 | 524.3 | 3.32 | 52.56 | 0.039 | 0.038 | 17.0 | 15.0 |
| 64 | 432.6 | 4.18 | 47.02 | 0.084 | 0.044 | 17.0 | 14.8 |
| 128 | 343.3 | 5.42 | 41.57 | 0.087 | 0.157 | 17.0 | 14.6 |
| 256 | 263.4 | 7.26 | 36.24 | 0.125 | 0.125 | 17.0 | 14.4 |
| 512 | 194.4 | 10.09 | 31.03 | 0.062 | 0.160 | 17.0 | 14.2 |
| 1024 | 135.5 | 14.77 | 26.03 | 0.064 | 0.129 | 17.0 | 14.1 |

## Version 2

| step q | bitrate (kbps) | compression ratio | SNR (dB) | encoding time (s) | decoding time (s) | encoder memory (MB) | decoder memory (MB) |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 426.7 | 4.12 | 57.81 | 0.084 | 0.065 | 17.0 | 14.8 |
| 32 | 354.2 | 5.13 | 52.16 | 0.078 | 0.061 | 17.0 | 14.6 |
| 64 | 285.4 | 6.50 | 46.56 | 0.075 | 0.056 | 17.0 | 14.5 |
| 128 | 224.8 | 8.37 | 41.07 | 0.069 | 0.053 | 17.0 | 14.4 |
| 256 | 171.0 | 11.12 | 35.69 | 0.071 | 0.047 | 17.0 | 14.2 |
| 512 | 126.5 | 15.20 | 30.45 | 0.064 | 0.045 | 17.0 | 14.1 |
| 1024 | 89.7 | 21.63 | 25.45 | 0.059 | 0.041 | 17.0 | 14.0 |

## Δ version 1 → version 2

At the same step v2 has a slightly different SNR (deadzone), so the bitrates are
compared at equal SNR: for each step of v1, the change of bitrate v2 needs to reach
the same SNR.

(— : v2 does not reach that SNR with the steps measured.)

| step q (v1) | v1 SNR (dB) | v1 bitrate (kbps) | bitrate change of v2 at the same SNR (%) |
|---:|---:|---:|---:|
| 16 | 58.16 | 627.4 | -33.5 |
| 32 | 52.56 | 524.3 | -32.4 |
| 64 | 47.02 | 432.6 | -33.4 |
| 128 | 41.57 | 343.3 | -33.4 |
| 256 | 36.24 | 263.4 | -33.2 |
| 512 | 31.03 | 194.4 | -32.2 |
| 1024 | 26.03 | 135.5 | -30.0 |

## What each improvement adds (BD-rate, equal SNR)

Cumulative, each row against v1:

| configuration (against v1) | BD-rate (bits at equal SNR, %) | BD-SNR (SNR at equal bitrate, dB) |
|---|---:|---:|
| v1 + context-adaptive Rice parameter | -30.44 | +7.396 |
| + adaptive Mid/Side | -31.87 | +7.848 |
| + deadzone and centroid (v2) | -32.66 | +8.056 |

Each step against the previous one:

| improvement (added to the previous row) | BD-rate (bits at equal SNR, %) | BD-SNR (SNR at equal bitrate, dB) |
|---|---:|---:|
| v1 + context-adaptive Rice parameter | -30.44 | +7.396 |
| + adaptive Mid/Side | -1.98 | +0.493 |
| + deadzone and centroid | -1.27 | +0.248 |
