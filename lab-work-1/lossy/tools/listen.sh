#!/usr/bin/env bash
# Makes WAV files to listen to the codec: an excerpt of a file, the excerpt coded at
# several bitrates, and the noise the codec added (original - decoded), amplified so
# it can be heard. Play them with aplay (or any player).
#
# Usage:   tools/listen.sh [file.wav] [start_s] [length_s]
# Default: ../AudioFiles/sample01.wav, the whole file (length 0 = until the end)
# Output:  results/listen/<file>/   (WAV files, not in git) + a table of kbps and SNR
#
# Files made (kbps = 32 64 96 128 192 256):
#   00_original.wav       the excerpt (or the whole file)
#   b<kbps>.wav           coded with -b <kbps> (MDCT, all of v2)
#   b<kbps>_noise.wav     original - decoded, amplified (same gain for all bitrates,
#                         so the noise of different bitrates can be compared)
#   dct_b64.wav           the DCT without overlap at 64 kbps: clicks at the block borders
#   noms_b64.wav          without Mid/Side at 64 kbps

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"   # the lossy folder
WAV="${1:-$ROOT/../AudioFiles/sample01.wav}"
START="${2:-0}"
LENGTH="${3:-0}"
BIN="$ROOT/build"
OUT="$ROOT/results/listen/$(basename "$WAV" .wav)"
RATES=(32 64 96 128 192 256)

[ -x "$BIN/wav_lossy_enc" ] || { echo "Build the codec first: cmake --build build" >&2; exit 1; }
mkdir -p "$OUT"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Copies the excerpt (start_s, length_s; length 0 = until the end) of the WAV file.
python3 - "$WAV" "$OUT/00_original.wav" "$START" "$LENGTH" << 'EOF'
import sys, wave
src, dst, start, length = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
with wave.open(src) as w:
    p = w.getparams()
    first = int(start * p.framerate)
    if first >= p.nframes:
        sys.exit(f"{src} has only {p.nframes / p.framerate:.1f} s: start {start} s is after the end")
    w.setpos(first)
    frames = w.readframes(int(length * p.framerate) if length > 0 else p.nframes)
with wave.open(dst, "wb") as o:
    o.setparams(p)
    o.writeframes(frames)
EOF

# noise <original> <decoded> <out> <gain>: (original - decoded) * gain, clipped
noise() {
    python3 - "$@" << 'EOF'
import sys, wave
a, b, out, gain = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
with wave.open(a) as wa, wave.open(b) as wb:
    p = wa.getparams()
    x, y = wa.readframes(p.nframes), wb.readframes(p.nframes)
n = p.sampwidth
signed = n > 1                                   # 8-bit WAV is unsigned
peak = 2 ** (8 * n - 1) - 1
res = bytearray()
for i in range(0, len(x), n):
    d = (int.from_bytes(x[i:i + n], "little", signed=signed)
         - int.from_bytes(y[i:i + n], "little", signed=signed))
    v = max(-peak - 1, min(peak, round(d * gain)))
    res += (v + 128).to_bytes(1, "little") if n == 1 else v.to_bytes(n, "little", signed=True)
with wave.open(out, "wb") as o:
    o.setparams(p)
    o.writeframes(bytes(res))
EOF
}

snr() { "$BIN/wav_snr" "$1" "$2" | awk '/^all/ {print $4}'; }

ORIG="$OUT/00_original.wav"
SECS=$(python3 -c "import wave,sys; w=wave.open(sys.argv[1]); print(w.getnframes()/w.getframerate())" "$ORIG")
kbps() { awk -v b="$(stat -c %s "$1")" -v s="$SECS" 'BEGIN { printf "%.1f", b * 8 / s / 1000 }'; }

# code <name> <options...>: codes the excerpt, prints a table row
code() {
    local name="$1"; shift
    "$BIN/wav_lossy_enc" "$@" "$ORIG" "$TMP/x.lsy"
    "$BIN/wav_lossy_dec" "$TMP/x.lsy" "$OUT/$name.wav"
    printf '| %s | %s | %s | %s |\n' "$name.wav" "$*" "$(kbps "$TMP/x.lsy")" "$(snr "$ORIG" "$OUT/$name.wav")"
}

echo "Excerpt: $(basename "$WAV"), from ${START} s, ${SECS} s"
echo
echo "| file | options | kbps | SNR (dB) |"
echo "|---|---|---:|---:|"
for r in "${RATES[@]}"; do code "b$r" -b "$r"; done
code dct_b64 -t dct -b 64
code noms_b64 --no-ms -b 64

# One gain for all noise files: the noise of the lowest bitrate (the loudest) gets a
# peak of half the full scale.
GAIN=$(python3 - "$ORIG" "$OUT/b${RATES[0]}.wav" << 'EOF'
import sys, wave
with wave.open(sys.argv[1]) as wa, wave.open(sys.argv[2]) as wb:
    p = wa.getparams(); x, y = wa.readframes(p.nframes), wb.readframes(p.nframes)
n = p.sampwidth; s = n > 1
m = max((abs(int.from_bytes(x[i:i+n], "little", signed=s) - int.from_bytes(y[i:i+n], "little", signed=s))
         for i in range(0, len(x), n)), default=0)
print(max(1.0, (2 ** (8 * n - 1) - 1) / 2 / m) if m else 1.0)
EOF
)
for r in "${RATES[@]}"; do noise "$ORIG" "$OUT/b$r.wav" "$OUT/b${r}_noise.wav" "$GAIN"; done

echo
printf 'Noise files amplified %.1f times (+%.1f dB).\n' "$GAIN" "$(awk -v g="$GAIN" 'BEGIN { print 20 * log(g) / log(10) }')"
echo "Files in $OUT. To play them (from any folder):"
echo "  D=$OUT"
echo "  aplay \$D/00_original.wav \$D/b64.wav"
