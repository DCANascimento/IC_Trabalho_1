#!/usr/bin/env bash
# Compares version 1 of the codec (git tag lossy-v1) with the current version and
# measures what each v2 improvement adds (ablation). Writes CSVs and Markdown tables.
#
# Usage:   tools/compare_versions.sh [audio_folder] [results_folder]
# Default: ../AudioFiles  results/versions   (relative to the lossy folder)
#
# Configurations (v2 options switch the improvements off):
#   v1             the tagged version 1
#   v2_rice        v2 with only the context Rice coding  (--no-ms -z 0.5 --no-centroid)
#   v2_rice_ms     + adaptive Mid/Side                     (-z 0.5 --no-centroid)
#   v2             + deadzone and centroid offsets (all of v2, default options)
#
# Needs git, cmake, a C++17 compiler and python3. Takes a few minutes.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"   # the lossy folder
AUDIO="${1:-$ROOT/../AudioFiles}"
OUT="${2:-$ROOT/results/versions}"
# Steps a factor sqrt(2) apart: enough points for the BD-rate curves.
STEPS=(11 16 23 32 45 64 91 128 181 256 362 512 724 1024)
TABLE_STEPS=" 16 32 64 128 256 512 1024 "   # rows of the summary tables

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Building v1 (tag lossy-v1)..."
git -C "$(git -C "$ROOT" rev-parse --show-toplevel)" archive lossy-v1 lossy | tar -x -C "$TMP"
cmake -S "$TMP/lossy" -B "$TMP/lossy/build" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$TMP/lossy/build" > /dev/null

echo "Building the current version..."
cmake -S "$ROOT" -B "$ROOT/build" > /dev/null
cmake --build "$ROOT/build" > /dev/null

# run <name> <bin folder> <encoder options>: per-file CSV + table of averages
run() {
    echo "Measuring $1..."
    BIN="$2" ENC_OPTS="$3" CSV="$OUT/$1.csv" "$ROOT/tools/bench.sh" "$AUDIO" "${STEPS[@]}" |
        awk -v keep="$TABLE_STEPS" 'NR <= 2 || index(keep, " " $2 " ")' > "$OUT/$1.md"
}

run v1         "$TMP/lossy/build" ""
run v2_rice    "$ROOT/build"      "--no-ms -z 0.5 --no-centroid"
run v2_rice_ms "$ROOT/build"      "-z 0.5 --no-centroid"
run v2         "$ROOT/build"      ""

cd "$OUT"
{
    echo "# Lossy codec: version 1 vs version 2"
    echo
    echo "Date: $(date '+%Y-%m-%d %H:%M'). Files:"
    for f in "$AUDIO"/*.wav; do echo "- $(basename "$f")"; done
    echo
    echo "Every row is the mean over the files."
    echo
    echo "## Version 1"
    echo
    cat v1.md
    echo
    echo "## Version 2"
    echo
    cat v2.md
    echo
    echo "## Δ version 1 → version 2"
    echo
    echo "At the same step v2 has a slightly different SNR (deadzone), so the bitrates are"
    echo "compared at equal SNR: for each step of v1, the change of bitrate v2 needs to reach"
    echo "the same SNR."
    echo
    echo "(— : v2 does not reach that SNR with the steps measured.)"
    echo
    "$ROOT/tools/compare.py" v1.csv v2.csv --per-step | sed -n '/^Bitrate/,$p' | tail -n +3 |
        sed 's/| v2.csv |/| bitrate change of v2 at the same SNR (%) |/; s/| step |/| step q (v1) |/;
             s/| kbps |/| v1 bitrate (kbps) |/; s/| SNR (dB) |/| v1 SNR (dB) |/' |
        awk -v keep="$TABLE_STEPS" 'NR <= 2 || index(keep, " " $2 " ")'
    echo
    echo "## What each improvement adds (BD-rate, equal SNR)"
    echo
    echo "Cumulative, each row against v1:"
    echo
    "$ROOT/tools/compare.py" v1.csv v2_rice.csv v2_rice_ms.csv v2.csv | tail -n +3 |
        sed 's/| compared with | BD-rate (%) | BD-SNR (dB) |/| configuration (against v1) | BD-rate (bits at equal SNR, %) | BD-SNR (SNR at equal bitrate, dB) |/;
             s/| v2_rice.csv |/| v1 + context-adaptive Rice parameter |/;
             s/| v2_rice_ms.csv |/| + adaptive Mid\/Side |/; s/| v2.csv |/| + deadzone and centroid (v2) |/'
    echo
    echo "Each step against the previous one:"
    echo
    echo "| improvement (added to the previous row) | BD-rate (bits at equal SNR, %) | BD-SNR (SNR at equal bitrate, dB) |"
    echo "|---|---:|---:|"
    "$ROOT/tools/compare.py" v1.csv v2_rice.csv | tail -1 | sed 's/v2_rice.csv/v1 + context-adaptive Rice parameter/'
    "$ROOT/tools/compare.py" v2_rice.csv v2_rice_ms.csv | tail -1 | sed 's/v2_rice_ms.csv/+ adaptive Mid\/Side/'
    "$ROOT/tools/compare.py" v2_rice_ms.csv v2.csv | tail -1 | sed 's/v2.csv/+ deadzone and centroid/'
} > README.md

echo "Done. Results in $OUT/README.md"
