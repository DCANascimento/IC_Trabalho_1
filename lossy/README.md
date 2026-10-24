# Lossy audio codec (Part III) — version 2

A lossy audio codec based on the **MDCT** (Modified Discrete Cosine Transform), written in C++17.

```
encoder:  WAV → MDCT (sine window, 50% overlap) → [Mid/Side] → quantization (deadzone) → context Rice coding → file
decoder:  file → context Rice decoding → dequantization (centroid) → [L/R] → inverse MDCT + overlap-add → WAV
```

Version 1 (git tag `lossy-v1`) had the MDCT, uniform quantization and one Rice parameter per block.
At equal SNR, version 2 needs **about 33 % fewer bits** (see [Results](#results)).

## Build

Requirements: a C++17 compiler (`g++` or `clang++`) and CMake ≥ 3.20.
The tools also use `bash`, `awk`, GNU `time` (`sudo apt install time`) and `python3`.

```bash
cd lossy
cmake -S . -B build
cmake --build build
```

This creates three programs in `build/`. The build is optimized (`Release`) by default.

| Program | Purpose |
|---|---|
| `wav_lossy_enc` | encoder |
| `wav_lossy_dec` | decoder |
| `wav_snr` | compares a decoded file with the original (MSE, maximum error, SNR) |

## Usage

```bash
./build/wav_lossy_enc [options] input.wav compressed_file
./build/wav_lossy_dec compressed_file output.wav
./build/wav_snr original.wav decoded.wav
```

Encoder options:

| Option | Meaning | Default |
|---|---|---|
| `-q <step>` | quantization step (≥ 0.01). Larger = smaller file and lower quality; doubling it costs about 6 dB of SNR | 16 |
| `-b <kbps>` | target bitrate: the step is chosen to reach it (replaces `-q`) | off |
| `-n <size>` | block size N (power of two, 16 … 4096) | 1024 |
| `-t <type>` | transform: `mdct` or `dct` (non-overlapping DCT-II blocks, kept for comparison) | `mdct` |
| `-z <r>` | quantizer rounding, 0 … 0.5: index = floor(\|X\| / step + r); 0.5 = round to nearest | 0.35 |
| `--no-centroid` | decode at the middle of each interval (no reconstruction offsets) | off |
| `--no-ms` | stereo: always code L/R, never Mid/Side | off |
| `-v` | print statistics: Mid/Side use, zero coefficients, entropy, bits per coefficient, ratio, kbps | off |

The `--no-…` options and `-z 0.5` switch the v2 improvements off; they exist to measure what each one adds.

Example (from `lossy/`):

```bash
# 1. compress (choose ONE way to set the quality)
./build/wav_lossy_enc -v -b 128 ../AudioFiles/sample01.wav s1.lsy   # target bitrate (kbps)
#   or: ./build/wav_lossy_enc -v -q 64 ../AudioFiles/sample01.wav s1.lsy   # quantization step
#   (with neither, the default step 16 is used)
# 2. decompress (no options: everything it needs is in the header of s1.lsy)
./build/wav_lossy_dec s1.lsy s1.wav
# 3. measure the error and listen
./build/wav_snr ../AudioFiles/sample01.wav s1.wav                    # MSE, max error, SNR
aplay s1.wav
```

`-q` fixes the error and lets the file size vary; `-b` fixes the file size and lets the
error vary (useful to compare with other codecs at the same bitrate).

Supported input: integer PCM WAV with 8, 16, 24 or 32 bits and any number of channels.
`WAVE_FORMAT_EXTENSIBLE` is accepted, and extra chunks such as `LIST` are skipped.
Floating-point WAV files are not supported (the encoder stops with a message).
The decoded file has the same sample rate, number of channels and bits per sample as the input.

## How it works

1. **WAV reading.** The whole file is loaded with a single read. The RIFF chunks are scanned for `fmt ` and `data`, and the samples are de-interleaved (one vector per channel). Samples are scaled so that one unit is one 16-bit step. This way `-q` means the same thing for 8, 16 and 24-bit files.
2. **MDCT.** Each block of 2N samples, multiplied by a sine window, gives N coefficients. Blocks advance N samples at a time, so the number of coefficients equals the number of samples. The aliasing of each block is cancelled by the overlap-add of its neighbours (TDAC). With the sine window (Princen-Bradley condition), the reconstruction is perfect and the transform is orthonormal. It is computed in O(N log N): the windowed input is folded into a DCT-IV, which is computed with a complex FFT of N/2 points.
3. **Mid/Side (v2).** For stereo, the coefficients of each block can be rotated into M = (L + R)/√2 and S = (L − R)/√2. When L ≈ R, S is almost zero and costs very little. The rotation is orthonormal, so the quantization error (and the SNR) is the same either way. The encoder codes the block both ways, counts the exact bits, and keeps the cheaper one (1 bit per block tells the decoder).
4. **Quantization (v2: deadzone + centroid).** Each coefficient becomes `sign(X)·floor(|X|/q + 0.35)`. This is the only lossy step. Compared with plain rounding (0.5), values just above a multiple of q go to the lower index: more zeros and smaller values, so fewer bits. The coefficients are concentrated near zero, so inside each interval most of them lie on the side closer to zero. The encoder measures their mean position (one offset for |i| = 1 and one for |i| ≥ 2) and stores it in the header. The decoder reconstructs at that point, the centroid, instead of at the middle of the interval. The two only help together: the deadzone alone increases the error more than it saves bits.
5. **Entropy coding (v2: context Rice).** For each block and channel:
   - `last` is written: the position after the last non-zero coefficient. The zeros after it, usually the high frequencies, cost nothing.
   - The coefficients before `last` are zigzag-mapped (0, −1, 1, −2 … → 0, 1, 2, 3 …) and written with a **Rice code**.
   - The Rice parameter `k` of **each coefficient** is predicted from the magnitudes of 8 neighbours that the decoder already knows: the 4 previous coefficients of the block and the 3 nearest ones in the previous block (the one at the same frequency counts twice). With s their sum, k is the largest value with 2^k ≤ s/4, an estimate of log2 of the mean. Encoder and decoder compute the same `k`, so it is not stored. In v1 there was a single `k` per block, which was too large for the many small high-frequency values and too small for the large low-frequency ones.
   - Very large values use an escape code, so a single outlier cannot cost thousands of bits.
6. **Rate control (v2).** With `-b`, a binary search on log2(step) (16 full encodings in memory) finds the step that gives the largest file not above the target. The same step is used for the whole file. With the squared error as the distortion this is close to optimal: every coefficient then trades error for bits at the same rate, so moving bits between blocks cannot lower the total error.
7. **Decoding** is the same chain in reverse. The decoded values are rounded and clipped to the sample range. Truncated or corrupted files are detected and reported.

## File format (`LSY6`)

| Field | Size |
|---|---|
| magic `"LSY6"` | 4 bytes |
| channels | u16 |
| sample rate | u32 |
| bits per sample | u16 |
| samples per channel | u32 |
| transform (0 = DCT, 1 = MDCT) | u8 |
| block size N | u16 |
| quantization step | f32 |
| reconstruction offsets for \|i\| = 1 and \|i\| ≥ 2 | 2 × f32 |
| bit stream: for each block, `ms` (1 bit, stereo only), then for each channel: `last` (log2(N)+1 bits) and `last` Rice codes | — |

Files from version 1 (`LSY3`) are not compatible.

## Experimental decisions

Measured on `sample01` and on the averages of the 7 files in `../AudioFiles`.

### Version 1

* **MDCT instead of block DCT.**
  - The SNR is the same, because both transforms are orthonormal.
  - The MDCT needs 1–3 % fewer bits.
  - With the DCT, the jump of the error at block boundaries is 2.0× the jump elsewhere, which is heard as a periodic buzz. With the MDCT it is 1.0×.
  - The fast MDCT encodes 29 s of audio in 0.05 s, against 2.5 s for the O(N²) DCT.
* **Block size N = 1024.** The minimum number of bits is reached at N = 1024–2048. 1024 was chosen because the shorter blocks spread less pre-echo before sharp attacks.

| block size N | 128 | 256 | 512 | 1024 | 2048 | 4096 |
|---|---:|---:|---:|---:|---:|---:|
| bitrate (kbps) at step 256, from the entropy of the quantized values (lower is better) | 256.8 | 249.8 | 242.8 | **238.2** | **238.0** | 243.1 |

### Version 2

* **Entropy coder.** Four schemes were measured on the same quantized coefficients (L/R, N = 1024), in bits per coefficient (`tools/run_experiments.sh`):

| entropy coding scheme | bits/coeff., step 16 | step 64 | step 256 | step 1024 |
|---|---:|---:|---:|---:|
| A. one Rice `k` per block (v1) | 7.08 | 4.86 | 3.00 | 1.56 |
| B. one `k` per band of 32 coefficients (5 bits each) | 5.15 | 3.56 | 2.24 | 1.26 |
| C. ideal adaptive arithmetic coding with context (estimate) | 6.06 | 3.79 | 2.14 | **1.07** |
| **D. Rice with `k` from the context (chosen)** | **5.00** | **3.42** | **2.12** | 1.16 |

  D is the simplest scheme and the best one except at very low rates. Several neighbourhoods were tried (2 to 8 neighbours, with scale factors 0.5–2). The 8-neighbour context with scale 1 was the best at every step, but only by 1–3 % over the simpler ones. The figure `results/experiments/rice_k_profile.pdf` shows, for one block, the single `k` of v1 against the per-coefficient `k` of v2. The v2 `k` follows the best local value, from about 7 at low frequencies down to 0. In that block it codes the same values with 25 % fewer bits.
* **Mid/Side chosen per block, by exact bit count.** Depending on the file, it is used in 30–96 % of the blocks (q = 64–512), but in only about 3 % of the blocks of `sample02`, where L and R differ. A fixed choice for the whole file would lose in either case. Choosing per block can never cost more than 1 bit per block.
* **Quantizer rounding 0.35 with centroid offsets.** Measured over 13 steps as BD-rate (bitrate change at equal SNR; negative = fewer bits) against plain rounding (r = 0.5, no offsets):

| rounding r (`-z`) | BD-rate with centroid offsets | BD-rate without offsets (`--no-centroid`) |
|---:|---:|---:|
| 0.5 | −0.70 % | 0 (reference) |
| 0.45 | −1.10 % | −0.86 % |
| 0.4 | −1.28 % | −0.66 % |
| **0.35** | **−1.31 %** | +0.36 % |
| 0.3 | −1.21 % | +1.93 % |
| 0.25 | −1.00 % | +3.86 % |
| 0.2 | −0.71 % | +6.02 % |

## Results

Obtained with `tools/compare_versions.sh`, which builds v1 from the git tag and measures both versions. Each row is the average over the 7 files (44.1 kHz, 16-bit stereo; original 1411 kbps) for one quantization step q. The compression ratio is the average of the per-file ratios, so it is not exactly 1411 / bitrate. Times are for 29 s of audio on average; memory is the peak resident memory of the program. All tables and the per-file values are in `results/versions/`.

### Version 1

| step q | bitrate (kbps) | compression ratio | SNR (dB) | encoding time (s) | decoding time (s) | encoder memory (MB) | decoder memory (MB) |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 627.4 | 2.40 | 58.16 | 0.040 | 0.038 | 17.1 | 15.3 |
| 32 | 524.3 | 3.32 | 52.56 | 0.038 | 0.037 | 17.1 | 15.1 |
| 64 | 432.6 | 4.18 | 47.02 | 0.037 | 0.035 | 17.1 | 14.9 |
| 128 | 343.3 | 5.42 | 41.57 | 0.036 | 0.035 | 17.0 | 14.7 |
| 256 | 263.4 | 7.26 | 36.24 | 0.035 | 0.035 | 17.1 | 14.4 |
| 512 | 194.4 | 10.09 | 31.03 | 0.034 | 0.033 | 17.1 | 14.3 |
| 1024 | 135.5 | 14.77 | 26.03 | 0.033 | 0.033 | 17.1 | 14.1 |

### Version 2

| step q | bitrate (kbps) | compression ratio | SNR (dB) | encoding time (s) | decoding time (s) | encoder memory (MB) | decoder memory (MB) |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 426.7 | 4.12 | 57.81 | 0.092 | 0.067 | 17.1 | 14.8 |
| 32 | 354.2 | 5.13 | 52.16 | 0.090 | 0.063 | 17.1 | 14.7 |
| 64 | 285.4 | 6.50 | 46.56 | 0.086 | 0.058 | 17.1 | 14.5 |
| 128 | 224.8 | 8.37 | 41.07 | 0.082 | 0.055 | 17.1 | 14.4 |
| 256 | 171.0 | 11.12 | 35.69 | 0.077 | 0.050 | 17.1 | 14.2 |
| 512 | 126.5 | 15.20 | 30.45 | 0.073 | 0.046 | 17.1 | 14.1 |
| 1024 | 89.7 | 21.63 | 25.45 | 0.068 | 0.043 | 17.1 | 14.1 |

With `-b`, encoding takes 0.5–1.5 s (16 encodings); decoding is unchanged.

### Δ version 1 → version 2

At the same step, v2 has a slightly lower SNR (about −0.5 dB, because of the deadzone), so the two versions are compared **at equal SNR**: for each step of v1, the change of bitrate that v2 needs to reach the same SNR.

| step q (v1) | v1 SNR (dB) | v1 bitrate (kbps) | bitrate change of v2 at the same SNR |
|---:|---:|---:|---:|
| 16 | 58.16 | 627.4 | −33.5 % |
| 32 | 52.56 | 524.3 | −32.4 % |
| 64 | 47.02 | 432.6 | −33.4 % |
| 128 | 41.57 | 343.3 | −33.4 % |
| 256 | 36.24 | 263.4 | −33.2 % |
| 512 | 31.03 | 194.4 | −32.2 % |
| 1024 | 26.03 | 135.5 | −30.0 % |

Over the whole curve: **BD-rate −32.7 %** (bits at equal SNR) and **BD-SNR +8.1 dB** (SNR at equal bitrate).
Encoding and decoding are about twice as slow (still under 0.1 s for 29 s of audio); memory is the same.

### What each improvement adds

| improvement (added to the previous row) | BD-rate (bits at equal SNR) | BD-SNR (SNR at equal bitrate) |
|---|---:|---:|
| v1 + context-adaptive Rice parameter | −30.4 % | +7.40 dB |
| + adaptive Mid/Side | −2.0 % | +0.49 dB |
| + deadzone and centroid | −1.3 % | +0.25 dB |
| **total (v1 → v2)** | **−32.7 %** | **+8.06 dB** |

Each row is measured against the previous one. Almost all of the gain comes from the entropy coder: v1 spent most of its bits on a badly chosen Rice parameter, not on information.

## Limitations and possible improvements

* **RDO quantization.** The deadzone shrinks every coefficient in the same way. A rate-distortion optimized quantizer would decide, for each coefficient, between rounding down and up by minimizing `error² + λ·bits`, using the exact Rice cost in its context (the encoder already knows it). For a uniform quantizer, λ ≈ 0.115·step² (from dD/dR = −2 ln2 · D). It may gain more than the deadzone (not measured), and the decoder would not change.
* **Arithmetic coding.** A Rice code never spends less than 1 bit on a coefficient, even when a zero is almost certain. At low rates (step ≥ 1024), an adaptive arithmetic coder with the same contexts would need about 8 % fewer bits (estimate C above).
* **No perceptual model.** The noise is shaped only by the step, evenly over all frequencies. This is the right choice for the SNR, the measure used here, but a perceptual codec (MP3, AAC) moves the noise under the masking threshold and sounds better at the same bitrate while having a lower SNR.
* **Fixed block size.** A single N = 1024 spreads pre-echo before sharp attacks; codecs such as AAC switch to short blocks around transients.

## Tools

See `tools/README.md`.

| Script | Output |
|---|---|
| `tools/bench.sh` | kbps, ratio, SNR, time and memory for several steps → `results/bench.csv` |
| `tools/compare.py` | BD-rate / BD-SNR between two `bench.sh` CSVs, and Δ per step |
| `tools/compare_versions.sh` | v1 vs v2 and the contribution of each improvement → `results/versions/` |
| `tools/run_experiments.sh` | entropy coder experiments and the `k` profile figure → `results/experiments/` |
| `tools/listen.sh` | WAV files to listen to: a file coded at 32 … 256 kbps and the noise of each → `results/listen/` (not in git) |
| `tools/system_test.sh` | end-to-end tests with sanitizers (real, synthetic, invalid and corrupted files) → PASS/FAIL |

## Source files

| File | Content |
|---|---|
| `src/wav.*` | WAV reading/writing (8/16/24/32-bit PCM) |
| `src/bytes.hpp` | little-endian helpers, `ByteWriter`/`ByteReader` |
| `src/bitstream.hpp` | `BitWriter`/`BitReader`, zigzag, Rice codes |
| `src/fft.*` | radix-2 FFT |
| `src/mdct.*` | fast MDCT (sine window, DCT-IV via FFT) |
| `src/dct.*` | block DCT-II (comparison only) |
| `src/transform.*` | common interface of the two transforms |
| `src/codec.*` | encoder/decoder, Mid/Side, quantizer, context Rice coding, rate control, file format |
| `src/wav_lossy_enc.cpp`, `src/wav_lossy_dec.cpp`, `src/wav_snr.cpp` | command-line programs |
