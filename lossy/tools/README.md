# Tools

Run them from the `lossy` folder. Results go to `lossy/results/`.

| Script | Purpose | Output |
|---|---|---|
| `bench.sh` | encodes and decodes every WAV file with several steps: kbps, ratio, SNR, time, memory | table on screen + `results/bench.csv` |
| `compare.py` | BD-rate / BD-SNR between `bench.sh` CSVs (equal-SNR comparison), Δ per step | table on screen |
| `compare_versions.sh` | builds v1 from the tag `lossy-v1`, measures v1, v2 and the intermediate stages | `results/versions/` |
| `run_experiments.sh` | experiments that justify the entropy coder of v2 | `results/experiments/` |
| `listen.sh` | WAV files to listen to: an excerpt coded at 32 … 256 kbps, the noise of each (amplified), DCT and no Mid/Side at 64 kbps | `results/listen/<file>/` (not in git) + table |
| `system_test.sh` | end-to-end tests (build, real and synthetic files, options, rate control, invalid input, corrupted files) with ASan/UBSan | PASS/FAIL on screen (about 2 minutes) |

## compare.py

```bash
CSV=results/a.csv ENC_OPTS="--no-ms" tools/bench.sh ../AudioFiles 16 23 32 45 64 91 128 181 256
CSV=results/b.csv tools/bench.sh ../AudioFiles 16 23 32 45 64 91 128 181 256
tools/compare.py results/a.csv results/b.csv --per-step --per-file
```

BD-rate: mean change of the bitrate at equal SNR (negative = better).

BD-SNR: mean change of the SNR at equal bitrate (positive = better).

Use many steps (a factor √2 apart): the curves are interpolated between the points.

## compare_versions.sh

```bash
tools/compare_versions.sh                    # ../AudioFiles -> results/versions (about 1 minute)
```

Needs the git tag `lossy-v1`. Writes the per-file CSVs, the tables of each configuration and `README.md` with all the tables (v1, v2, Δ, contribution of each improvement).

## bench.sh

```bash
tools/bench.sh                          # ../AudioFiles, steps 16 … 1024
tools/bench.sh ../AudioFiles 64 256     # chosen steps
ENC_OPTS="--no-ms" tools/bench.sh       # extra encoder options (e.g. the "before" of a comparison)
CSV=results/lr.csv tools/bench.sh       # other CSV file
```

The codec must be built first (`cmake --build build`).

## listen.sh

```bash
tools/listen.sh                                   # sample01, whole file
tools/listen.sh ../AudioFiles/sample04.wav 5 8    # 8 s from 5 s (length 0 = until the end)
aplay results/listen/sample01/00_original.wav results/listen/sample01/b64.wav
```

The `_noise` files are original − decoded, all with the same gain (printed), so the
noise of two bitrates can be compared by ear. Uses only `aplay` and the Python standard
library; takes about 20 s.

## system_test.sh

```bash
tools/system_test.sh                          # ../AudioFiles
tools/system_test.sh my_wavs                  # the .wav/.WAV files of a folder (not its subfolders)
tools/system_test.sh song.wav                 # one file
```

Builds the codec twice (release, and with AddressSanitizer + UndefinedBehaviorSanitizer)
and runs every test with the sanitizer build, so a memory error also fails a test.
Groups 1–5 use the given files; groups 6–10 use synthetic files made by the script.

| Group | Checks |
|---|---|
| 1 | every file: encode + decode, same format and length, error within the step (see below) |
| 2 | first file: step 1 … 4096 → size and SNR decrease |
| 3 | encoding and decoding twice give identical files |
| 4 | every encoder option works, error within the step |
| 5 | `-b`: bitrate within 97–100.5 % of the target; impossible targets do not fail |
| 6 | 8/16/24/32 bits, 1–6 channels, 8–96 kHz: header kept |
| 7 | `-q 0.01`: lossless for 8/16 bits, ≤ 8 LSB for 24 bits |
| 8 | 0 … 2049 samples, silence, full-scale square, white noise, L = R |
| 9 | invalid options and files: exit code 1 and a message |
| 10 | 200 randomly corrupted compressed files: the decoder does not crash |

Files the codec does not support (float WAV) are reported as SKIP, not FAIL: that
is a known limitation, not an error.

### Why the error is checked against the step, not the SNR

The first version of the test required SNR > 30 dB at the default step. It passed
for `AudioFiles` but failed for quieter files although the codec worked correctly.
The noise added by the codec depends only on the step q, not on the signal, so the
SNR at a given step changes with the loudness of the file:

| file | SNR, q = 16 | MSE / q², q = 16 | SNR, q = 256 | MSE / q², q = 256 |
|---|---:|---:|---:|---:|
| sample01 … sample07 | 52 – 66 dB | 0.048 – 0.087 | 32 – 42 dB | 0.016 – 0.081 |
| sample01, 20 dB quieter | 38.5 dB | 0.084 | 18.0 dB | 0.037 |
| sample01, 34 dB quieter | 25.7 dB | 0.064 | 9.0 dB | 0.012 |

The SNR changes by 40 dB, while the mean squared error stays near or below q²/12
(0.083, the error of a uniform quantizer) for every file. So the test checks what the
codec actually guarantees, an error bounded by the step:

    MSE ≤ (1 − r)² q² / 3  +  1/12 LSB²        (MSE in 16-bit units, like q)

* `(1 − r)² q² / 3`: the widest interval of the quantizer is the deadzone, of width
  2(1 − r)q (1.3q for the default r = 0.35). A uniformly distributed value in it has
  mean squared error (2(1 − r)q)²/12. All other intervals have width q, so this is the
  worst case: 0.141 q² for r = 0.35, q²/12 for r = 0.5, q²/3 for r = 0. The
  transform is orthonormal, so the error of the coefficients is the error of the samples.
* `1/12 LSB²`: the decoder rounds the output to integer samples of the original
  format. It only matters for 8-bit files (one 8-bit LSB is 256 units of 16 bits).

A quieter file has a lower error (more coefficients fall in the deadzone, and their
error is at most their own size), so it cannot fail. A real bug (wrong reconstruction
offsets, wrong inverse Mid/Side, misaligned overlap-add) gives an error many times
larger and still fails. The SNR and the compression ratio are printed but not checked;
whether the quality is good enough for a file is a different question, answered by the
rate-distortion curves (`bench.sh`) and by listening.

## run_experiments.sh

```bash
tools/run_experiments.sh                                  # ../AudioFiles -> results/experiments
tools/run_experiments.sh my_wavs results/mine             # other files, other results folder
STEPS="32,128,512" PROFILE_FILE=../AudioFiles/sample04.wav PROFILE_OPTS="-q 256 -b 300" \
    tools/run_experiments.sh
```

Requirements: `g++` with C++17. The figure also needs `pdflatex` with `pgfplots`.
The programs are compiled into a temporary folder each time, from the codec sources in `src/`, so nothing has to be built first.

| File in `experiments/` | Content |
|---|---|
| `common.hpp` | quantized MDCT coefficients of every block, computed as the codec does it (L/R, no Mid/Side) |
| `rice_schemes.cpp` | bits per coefficient of 4 entropy coding schemes: one Rice `k` per block (v1), one `k` per band of 32, ideal context arithmetic coding, `k` from the context (v2) |
| `rice_contexts.cpp` | bits per coefficient for several context definitions and scale factors |
| `rice_k_profile.cpp` | per-coefficient Rice parameter of one block (v1 vs v2) |
| `rice_k_profile.tex` | the figure drawn from the profile (pgfplots) |

Output in `results/experiments/`:

| File | Content |
|---|---|
| `README.md` | all tables in Markdown, with the date and the list of files |
| `rice_schemes.csv`, `rice_contexts.csv` | the tables |
| `rice_k_profile.csv`, `rice_k_profile.txt` | profile data and its summary |
| `rice_k_profile.pdf` | figure for the report |
