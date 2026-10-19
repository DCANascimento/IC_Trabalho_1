# Lossy audio codec (Part III) — version 1

A lossy audio codec based on the **MDCT** (Modified Discrete Cosine Transform),
written in C++17.

```
encoder:  WAV → MDCT (sine window, 50% overlap) → uniform quantization → Rice coding → file
decoder:  file → Rice decoding → dequantization → inverse MDCT + overlap-add → WAV
```

## Build

Requirements: a C++17 compiler (`g++` or `clang++`) and CMake ≥ 3.20.
The benchmark script also uses `bash`, `awk` and GNU `time` (`sudo apt install time`).

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
| `-n <size>` | block size N (power of two, 16 … 4096) | 1024 |
| `-t <type>` | transform: `mdct` or `dct` (non-overlapping DCT-II blocks, kept for comparison) | `mdct` |
| `-v` | print statistics: zero coefficients, entropy, bits per coefficient, ratio, kbps | off |

Example:

```bash
./build/wav_lossy_enc -v -q 128 ../AudioFiles/sample01.wav s1.lsy
./build/wav_lossy_dec s1.lsy s1.wav
./build/wav_snr ../AudioFiles/sample01.wav s1.wav
aplay s1.wav
```

Supported input: integer PCM WAV with 8, 16, 24 or 32 bits and any number of channels.
`WAVE_FORMAT_EXTENSIBLE` is accepted, and extra chunks such as `LIST` are skipped.
The decoded file has the same sample rate, number of channels and bits per sample as the input.

## How it works

1. **WAV reading.** The whole file is loaded with a single read. The RIFF chunks are scanned for `fmt ` and `data`, and the samples are de-interleaved (one vector per channel). Samples are scaled so that one unit is one 16-bit step. This way `-q` means the same thing for 8, 16 and 24-bit files.
2. **MDCT.** Each block of 2N samples, multiplied by a sine window, gives N coefficients. Blocks advance N samples at a time, so the number of coefficients equals the number of samples. The aliasing of each block is cancelled by the overlap-add of its neighbours (TDAC). With the sine window (Princen-Bradley condition), the reconstruction is perfect and the transform is orthonormal. It is computed in O(N log N): the windowed input is folded into a DCT-IV, which is computed with a complex FFT of N/2 points.
3. **Quantization.** Each coefficient becomes `round(X / q)`. This is the only lossy step. Because the transform is orthonormal, the time-domain error has MSE ≈ q²/12.
4. **Entropy coding.** For each block and channel:
   - `last` is written: the position after the last non-zero coefficient. The zeros after it, usually the high frequencies, cost nothing.
   - The coefficients before `last` are zigzag-mapped (0, −1, 1, −2 … → 0, 1, 2, 3 …) and written with a **Rice code**.
   - The Rice parameter `k` is chosen per block: it starts near log2(mean) and the exact cost of the neighbouring values is compared.
   - Very large values use an escape code, so a single outlier cannot cost thousands of bits.
5. **Decoding** is the same chain in reverse. The decoded values are rounded and clipped to the sample range. Truncated or corrupted files are detected and reported.

## File format (`LSY3`)

| Field | Size |
|---|---|
| magic `"LSY3"` | 4 bytes |
| channels | u16 |
| sample rate | u32 |
| bits per sample | u16 |
| samples per channel | u32 |
| transform (0 = DCT, 1 = MDCT) | u8 |
| block size N | u16 |
| quantization step | f32 |
| bit stream: for each block, for each channel: `last` (log2(N)+1 bits), `k` (5 bits, only if `last > 0`), `last` Rice codes | — |

## Experimental decisions

Measured on `sample01` and on the averages of the 7 files in `AudioFiles/`.

* **MDCT instead of block DCT.**
  - The SNR is the same, because both transforms are orthonormal.
  - The MDCT needs 1–3 % fewer bits.
  - With the DCT, the jump of the error at block boundaries is 2.0× the jump elsewhere, which is heard as a periodic buzz. With the MDCT it is 1.0×.
  - The fast MDCT encodes 29 s of audio in 0.05 s, against 2.5 s for the O(N²) DCT.
* **Block size N = 1024.** The minimum number of bits is reached at N = 1024–2048. 1024 was chosen because the shorter blocks spread less pre-echo before sharp attacks.

| N | 128 | 256 | 512 | 1024 | 2048 | 4096 |
|---|---|---|---|---|---|---|
| ideal kbps (entropy, q = 256) | 256.8 | 249.8 | 242.8 | **238.2** | **238.0** | 243.1 |

* **Rice coding.** The coded size is 8–14 % above the zeroth-order entropy of the quantized coefficients, which is the limit for coding them one by one.

## Results — version 1

Obtained with `tools/bench.sh ../AudioFiles`. Each row is the average over the 7 files (44.1 kHz, 16-bit stereo; original 1411 kbps).

| step | kbps | ratio | SNR (dB) | enc (s) | dec (s) | enc mem (MB) | dec mem (MB) |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 627.4 | 2.40 | 58.16 | 0.039 | 0.038 | 17.1 | 15.3 |
| 32 | 524.3 | 3.32 | 52.56 | 0.041 | 0.040 | 17.1 | 15.1 |
| 64 | 432.6 | 4.18 | 47.02 | 0.039 | 0.037 | 17.1 | 14.8 |
| 128 | 343.3 | 5.42 | 41.57 | 0.037 | 0.036 | 17.1 | 14.6 |
| 256 | 263.4 | 7.26 | 36.24 | 0.034 | 0.033 | 17.1 | 14.4 |
| 512 | 194.4 | 10.09 | 31.03 | 0.034 | 0.032 | 17.1 | 14.3 |
| 1024 | 135.5 | 14.77 | 26.03 | 0.032 | 0.032 | 17.1 | 14.1 |

`ratio` is the average of the per-file ratios, so it is not exactly 1411 / kbps.
The per-file values are saved in `bench.csv`.

To compare other settings, pass extra encoder options, for example:
`ENC_OPTS="-t dct" tools/bench.sh ../AudioFiles 64 256`.

### Known limitations (planned for version 2)

* **Uniform quantization of every coefficient.** The noise is spread evenly over all frequencies. There is no per-band step and no perceptual weighting.
* **No stereo decorrelation.** L and R are coded independently; Mid/Side is not used yet.
* **One Rice parameter per block.** The low frequencies (large values) and the high frequencies (mostly 0 and ±1) share the same `k`, and every zero before `last` still costs at least 1 bit.
* **No rate control.** The size is set indirectly with `-q`; a target bitrate in kbps is not supported.

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
| `src/codec.*` | encoder/decoder and file format |
| `src/wav_lossy_enc.cpp`, `src/wav_lossy_dec.cpp`, `src/wav_snr.cpp` | command-line programs |
| `tools/bench.sh` | benchmark (bitrate, ratio, SNR, time, memory) |
