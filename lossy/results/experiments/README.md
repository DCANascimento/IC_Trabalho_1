# Lossy codec experiments

Date: 2026-10-21 20:49
Files:
- sample01.wav
- sample02.wav
- sample03.wav
- sample04.wav
- sample05.wav
- sample06.wav
- sample07.wav

## Entropy coding schemes (bits per coefficient, L/R, N = 1024)

A: one Rice k per block (v1); B: one k per band of 32 coefficients;
C: ideal adaptive arithmetic coding with context (estimate); D: k from the context (v2).

| step | A_rice_per_block | B_rice_per_band32 | C_arithmetic_context | D_rice_context |
|---|---|---|---|---|
| 16 | 7.082 | 5.152 | 6.057 | 5.003 |
| 64 | 4.864 | 3.558 | 3.786 | 3.419 |
| 256 | 3.003 | 2.241 | 2.144 | 2.120 |
| 1024 | 1.561 | 1.255 | 1.071 | 1.164 |

## Rice parameter contexts (bits per coefficient)

| step | context | scale 0.5 | scale 0.7 | scale 1.0 | scale 1.4 | scale 2.0 |
|---|---|---|---|---|---|---|
| 16 | c1 c2 | 6.097 | 5.662 | 5.379 | 5.320 | 5.389 |
| 16 | c1 c2 p0 | 5.589 | 5.278 | 5.096 | 5.099 | 5.233 |
| 16 | 2c1 c2 p0 (p-1 p+1)/2 | 5.453 | 5.161 | 5.030 | 5.052 | 5.200 |
| 16 | c1..c4 2p0 p-1 p+1 (codec) | 5.355 | 5.099 | 5.003 | 5.049 | 5.217 |
| 64 | c1 c2 | 4.180 | 3.888 | 3.693 | 3.665 | 3.742 |
| 64 | c1 c2 p0 | 3.827 | 3.617 | 3.484 | 3.493 | 3.610 |
| 64 | 2c1 c2 p0 (p-1 p+1)/2 | 3.743 | 3.533 | 3.436 | 3.457 | 3.581 |
| 64 | c1..c4 2p0 p-1 p+1 (codec) | 3.678 | 3.491 | 3.419 | 3.457 | 3.597 |
| 256 | c1 c2 | 2.553 | 2.395 | 2.290 | 2.286 | 2.366 |
| 256 | c1 c2 p0 | 2.351 | 2.236 | 2.159 | 2.170 | 2.266 |
| 256 | 2c1 c2 p0 (p-1 p+1)/2 | 2.309 | 2.187 | 2.130 | 2.148 | 2.244 |
| 256 | c1..c4 2p0 p-1 p+1 (codec) | 2.274 | 2.163 | 2.120 | 2.150 | 2.256 |
| 1024 | c1 c2 | 1.342 | 1.282 | 1.246 | 1.255 | 1.321 |
| 1024 | c1 c2 p0 | 1.263 | 1.218 | 1.183 | 1.193 | 1.256 |
| 1024 | 2c1 c2 p0 (p-1 p+1)/2 | 1.251 | 1.197 | 1.169 | 1.182 | 1.241 |
| 1024 | c1..c4 2p0 p-1 p+1 (codec) | 1.239 | 1.186 | 1.164 | 1.182 | 1.247 |

## Rice parameter profile (figure rice_k_profile.pdf)

```
file sample01.wav, channel 0, block 632 of 1265, N = 1024, step = 64
last = 888, k_block = 4
bits: one k per block 5853 (+5 for k), k from context 4367 (25.5 % less)
```
