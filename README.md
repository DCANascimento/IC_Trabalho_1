# IC_Trabalho_1
 
Information and Coding (2026/27) — Lab work n.º 1
Universidade de Aveiro, DETI
 
#### Diogo Nascimento - 120031
#### Ana Sofia Almeida - 113892
#### Beatriz Saraiva - 108944
 
<!-- Audio analysis tools and audio codecs (lossless and lossy) in C/C++,
with MATLAB prototypes used during development. -->
 

 
 ## Repository structure
 
```
AudioFiles/      Test audio files (WAV, 16-bit)
Project/          C/C++ implementation
  include/        Header files
  src/            Library code (WAV I/O, histogram, ...)
  programs/       Command-line programs
  gui/            GTK4 graphical interface
  makefile
lossy/            Part III: lossy audio codec (C++17, CMake) — see lossy/README.md
  src/            codec and programs
  tools/          test, measurement and comparison scripts
  results/        tables and figures used in the report
matlab/           MATLAB prototypes
report/           Report 1 (LaTeX)
report_ai/        Report 2 — use of AI tools (LaTeX)
```
 
## Requirements
 
- `gcc` / `g++` (C11 and C++17)
- `make`, `pkg-config`
- GTK4 (`libgtk-4-dev`), only for the graphical interface
- At least one audio player: `paplay` or `aplay` for GUI and terminal playback; `ffplay` is an additional GUI fallback
- MATLAB, only for the `.m` prototypes
- `cmake` ≥ 3.20, only for the lossy codec (Part III)
- `python3`, only for the lossy codec's test and measurement scripts



#### On Debian/Ubuntu:
 
```bash
sudo apt install build-essential make cmake python3 pkg-config libgtk-4-dev pulseaudio-utils alsa-utils ffmpeg
```
 
## Build

```bash
cd Project
make              # builds the command-line tools and wav_app
make wav_hist     # command-line tool only (no GTK4 needed)
make wav_quant    # quantization command-line tool only (no GTK4 needed)
make wav_cmp      # comparison command-line tool only (no GTK4 needed)
make wav_effects  # interactive effects tool only (no GTK4 needed)
make clean
```

### Lossy codec (Part III)

```bash
cd lossy
cmake -S . -B build && cmake --build build   # wav_lossy_enc, wav_lossy_dec, wav_snr in lossy/build/
```
 
## Usage
 
```bash
cd Project
./wav_hist "../AudioFiles/sample01.wav"   # histogram of an audio file
./wav_quant "../AudioFiles/sample01.wav" "../QuantizedFiles/sample01_8b.wav" --bits 8
./wav_cmp "../AudioFiles/sample01.wav" "../QuantizedFiles/sample01_8b.wav"
./wav_effects
./wav_app                                  # graphical interface
```

Run `wav_effects` from `Project/`. It lists WAV files in `../AudioFiles/`,
offers the echo, modulation, and time-varying-delay menus, saves results in
`Project/EffectsFiles/`, and attempts terminal playback with `aplay` or
`paplay` when either player is installed.

The command-line quantizer accepts 1 to 16 bits and writes a new 16-bit WAV
file. Its syntax is:

```bash
./wav_quant input.wav output.wav --bits N
```

The comparison command accepts an original WAV and a quantized WAV, then
prints per-channel MSE, maximum absolute error, and SNR, followed by the
arithmetic average of each statistic across channels:

```bash
./wav_cmp original.wav quantized.wav
```

The graphical application starts with four options: `WAV Histogram` opens the
existing four-channel histogram view, `WAV Quantization` opens the quantization
view, `WAV Comparison` lets you select an original and quantized WAV and
displays the same comparison statistics, and `WAV Effects` lets you select a
16-bit mono or stereo WAV and generate/play effects from speaker-icon buttons.
Frequency and vibrato depth controls are used by the applicable effects.
In-app playback uses the first available `paplay`, `aplay`, or `ffplay`
executable and reports when no player is installed. Effects accept 16-bit mono
or stereo PCM input. Effected files are saved in `Project/EffectsFiles/`. The quantization view
accepts between 1 and 16 bits, can process one or more selected WAV files, or
can process every WAV file in `../AudioFiles`. Outputs are written to
`Project/QuantizedFiles/` using names such as `sample01_8b.wav`. Run the
application from the `Project` directory when using the all-files action.
 
The MATLAB scripts are run from the `matlab/` folder. They list the WAV
files in the input folder and ask which one to process; results are written
to a folder created automatically.
 
| Script | Purpose | Output folder |
|---|---|---|
| `wav_hist.m` | Histograms of the L, R, MID and SIDE channels | — |
| `wav_quant.m` | Uniform scalar quantization (bit reduction) | `QuantizedFiles/` |
| `wav_cmp.m` | Compare original and quantized WAV statistics | — |
| `wav_effects.m` | Echo, amplitude modulation, time-varying delays | `EffectsFiles/` |
 
### Lossy codec (Part III)

DCT-based lossy codec (MDCT, adaptive Mid/Side, deadzone quantizer, context-adaptive
Rice coding). Run from `lossy/`:

```bash
# 1. compress (choose ONE way to set the quality)
build/wav_lossy_enc -b 128 ../AudioFiles/sample01.wav s1.lsy   # target bitrate (kbps)
#   or: build/wav_lossy_enc -q 64 ../AudioFiles/sample01.wav s1.lsy   # quantization step
# 2. decompress
build/wav_lossy_dec s1.lsy s1.wav
# 3. measure the error
build/wav_snr ../AudioFiles/sample01.wav s1.wav                # MSE, max error, SNR
```

All options, the file format, the design decisions and the results are in
[`lossy/README.md`](lossy/README.md). The scripts that test the codec, measure it and
reproduce the tables of the report are described in [`lossy/tools/README.md`](lossy/tools/README.md).

## Status
 
- [x] Part I — `wav_hist`
- [x] Part I — `wav_quant` (MATLAB prototype and C/C++ application)
- [x] Part I — `wav_cmp` (MATLAB prototype and C/C++ application)
- [x] Part I — `wav_effects` (MATLAB prototype and C/C++ application)
- [ ] Part II — lossless codec
- [x] Part III — lossy codec
- [ ] Part IV — reports
 