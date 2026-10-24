#!/usr/bin/env bash
# Reproduces the experiments behind the entropy coder of the lossy codec (v2)
# with any set of WAV files, and saves the results (CSV, Markdown tables, figure).
#
# Usage:   tools/run_experiments.sh [audio_folder] [results_folder]
# Default: ../AudioFiles  results/experiments   (relative to the lossy folder)
# Options through environment variables:
#   STEPS="16,64,256,1024"     quantization steps of the tables
#   PROFILE_FILE=file.wav      file of the k profile figure (default: first file of the folder)
#   PROFILE_OPTS="-q 64 -b 600 -c 0"   step, block and channel of the figure
#
# Requires g++ (C++17) and pdflatex with pgfplots (only for the figure).
# Run it from anywhere; paths are relative to the current directory.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"   # the lossy folder
SRC="$ROOT/src"
EXP="$ROOT/tools/experiments"
AUDIO="${1:-$ROOT/../AudioFiles}"
OUT="${2:-$ROOT/results/experiments}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"   # absolute: pdflatex runs in another folder
STEPS="${STEPS:-16,64,256,1024}"
PROFILE_OPTS="${PROFILE_OPTS:--q 64}"

FILES=("$AUDIO"/*.wav)
[ -e "${FILES[0]}" ] || { echo "No .wav files in $AUDIO" >&2; exit 1; }
PROFILE_FILE="${PROFILE_FILE:-${FILES[0]}}"

BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

# CSV -> Markdown table
toMarkdown() {
    awk -F, 'NR == 1 { line = "|"; sep = "|"
                       for (i = 1; i <= NF; i++) { line = line " " $i " |"; sep = sep "---|" }
                       print line; print sep; next }
             { line = "|"; for (i = 1; i <= NF; i++) line = line " " $i " |"; print line }' "$1" |
        tr -d '"'
}

echo "Building the experiments..."
for prog in rice_schemes rice_contexts rice_k_profile; do
    g++ -O2 -std=c++17 -Wall -I"$SRC" "$EXP/$prog.cpp" "$SRC/mdct.cpp" "$SRC/fft.cpp" \
        "$SRC/wav.cpp" -o "$BUILD/$prog"
done

{
    echo "# Lossy codec experiments"
    echo
    echo "Date: $(date '+%Y-%m-%d %H:%M')"
    echo "Files:"
    for f in "${FILES[@]}"; do echo "- $(basename "$f")"; done
    echo
} > "$OUT/README.md"

echo "Entropy coding schemes (steps $STEPS)..."
"$BUILD/rice_schemes" -q "$STEPS" "${FILES[@]}" > "$OUT/rice_schemes.csv"
{
    echo "## Entropy coding schemes (bits per coefficient, L/R, N = 1024)"
    echo
    echo "A: one Rice k per block (v1); B: one k per band of 32 coefficients;"
    echo "C: ideal adaptive arithmetic coding with context (estimate); D: k from the context (v2)."
    echo
    toMarkdown "$OUT/rice_schemes.csv"
    echo
} >> "$OUT/README.md"

echo "Rice parameter contexts..."
"$BUILD/rice_contexts" -q "$STEPS" "${FILES[@]}" > "$OUT/rice_contexts.csv"
{
    echo "## Rice parameter contexts (bits per coefficient)"
    echo
    toMarkdown "$OUT/rice_contexts.csv"
    echo
} >> "$OUT/README.md"

echo "Rice parameter profile of one block..."
# shellcheck disable=SC2086
"$BUILD/rice_k_profile" $PROFILE_OPTS "$PROFILE_FILE" > "$OUT/rice_k_profile.csv" \
    2> "$OUT/rice_k_profile.txt"
{
    echo "## Rice parameter profile (figure rice_k_profile.pdf)"
    echo
    echo '```'
    cat "$OUT/rice_k_profile.txt"
    echo '```'
} >> "$OUT/README.md"

if command -v pdflatex > /dev/null; then
    echo "Drawing the figure..."
    (cd "$BUILD" && pdflatex -interaction=batchmode \
        "\def\csvfile{$OUT/rice_k_profile.csv}\input{$EXP/rice_k_profile.tex}" > /dev/null) ||
        { cp "$BUILD/rice_k_profile.log" "$OUT/";
          echo "pdflatex failed, see $OUT/rice_k_profile.log" >&2; exit 1; }
    cp "$BUILD/rice_k_profile.pdf" "$OUT/rice_k_profile.pdf"
else
    echo "pdflatex not found: figure not drawn (the CSV is in $OUT)" >&2
fi

echo "Done. Results in $OUT"
