# IC_Trabalho_1
 
Information and Coding (2026/27) — Lab work n.º 1
Universidade de Aveiro, DETI
 
#### Diogo Nascimento - 120031
#### Ana Sofia Almeida - 113892
#### Beatriz Saraiva - 108944
 
<!-- Audio analysis tools and audio codecs (lossless and lossy) in C/C++,
with MATLAB prototypes used during development. -->
 

 
<!-- ## Repository structure
 
```
Audio Files/      Test audio files (WAV, 16-bit)
Project/          C/C++ implementation
  include/        Header files
  src/            Library code (WAV I/O, histogram, ...)
  programs/       Command-line programs
  gui/            GTK4 graphical interface
  makefile
report/           Report 1 (LaTeX)
report_ai/        Report 2 — use of AI tools (LaTeX)
*.m               MATLAB prototypes
``` -->
 
## Requirements
 
- `gcc` / `g++` (C11 and C++17)
- `make`, `pkg-config`
- GTK4 (`libgtk-4-dev`), only for the graphical interface
- MATLAB, only for the `.m` prototypes


#### On Debian/Ubuntu:
 
```bash
sudo apt install build-essential pkg-config libgtk-4-dev
```
 
## Build

```bash
cd Project
make              # builds wav_hist and wav_app
make wav_hist     # command-line tool only (no GTK4 needed)
make wav_quant    # quantization command-line tool only (no GTK4 needed)
make clean
```
 
## Usage
 
```bash
cd Project
./wav_hist "../AudioFiles/sample01.wav"   # histogram of an audio file
./wav_quant "../AudioFiles/sample01.wav" "../QuantizedFiles/sample01_8b.wav" --bits 8
./wav_app                                  # graphical interface
```

The command-line quantizer accepts 1 to 16 bits and writes a new 16-bit WAV
file. Its syntax is:

```bash
./wav_quant input.wav output.wav --bits N
```

The graphical application starts with two options: `WAV Histogram` opens the
existing four-channel histogram view, and `WAV Quantization` opens the
quantization view. The quantization view accepts between 1 and 16 bits, can
process one or more selected WAV files, or can process every WAV file in
`../AudioFiles`. Outputs are written to `Project/QuantizedFiles/` using names
such as `sample01_8b.wav`. Run the application from the `Project` directory
when using the all-files action.
 
The MATLAB scripts are run from the repository root. They list the WAV
files in the input folder and ask which one to process; results are written
to a folder created automatically.
 
| Script | Purpose | Output folder |
|---|---|---|
| `wav_hist.m` | Histograms of the L, R, MID and SIDE channels | — |
| `wav_quant.m` | Uniform scalar quantization (bit reduction) | `QuantizedFiles/` |
| `wav_effects.m` | Echo, amplitude modulation, time-varying delays | `EffectsFiles/` |
 
## Status
 
- [x] Part I — `wav_hist`
- [x] Part I — `wav_quant` (MATLAB prototype and C/C++ application)
- [ ] Part I — `wav_cmp`, `wav_effects` (MATLAB prototypes done, C/C++ pending)
- [ ] Part II — lossless codec
- [ ] Part III — lossy codec
- [ ] Part IV — reports
 