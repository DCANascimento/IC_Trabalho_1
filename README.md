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
 
<!-- ```bash
cd Project
make              # builds wav_hist and wav_app
make wav_hist     # command-line tool only (no GTK4 needed)
make clean
``` -->
 
## Usage
 
```bash
cd Project
./wav_hist "../AudioFiles/sample01.wav"   # histogram of an audio file
./wav_app                                  # graphical interface
```
 
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
- [] Part I — `wav_quant`, `wav_cmp`, `wav_effects` (MATLAB prototypes done, C/C++ pending)
- [ ] Part II — lossless codec
- [ ] Part III — lossy codec
- [ ] Part IV — reports
 