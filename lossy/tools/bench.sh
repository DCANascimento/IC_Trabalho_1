#!/usr/bin/env bash
# Encodes and decodes every WAV file of a folder with several quantization steps
# and prints, for each step, the averages over all files:
#   bitrate, compression ratio, SNR, encoding/decoding time and peak memory.
#
# Usage:   tools/bench.sh [audio_folder] [step ...]
# Example: tools/bench.sh ../AudioFiles 16 64 256
#          ENC_OPTS="-t dct" tools/bench.sh      (extra encoder options)
#
# The per-file results are also saved in bench.csv (or in $CSV).

set -euo pipefail

BIN="$(cd "$(dirname "$0")/.." && pwd)/build"
DIR="${1:-../AudioFiles}"
shift || true
STEPS=("$@")
[ ${#STEPS[@]} -eq 0 ] && STEPS=(16 32 64 128 256 512 1024)
CSV="${CSV:-bench.csv}"
ENC_OPTS="${ENC_OPTS:-}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Runs a command and prints "<seconds> <peak memory in KB>".
measure() {
    local t0 t1
    t0=$(date +%s%N)
    /usr/bin/time -f "%M" -o "$TMP/mem" "$@" > /dev/null
    t1=$(date +%s%N)
    echo "$(( (t1 - t0) / 1000 )) $(cat "$TMP/mem")" | awk '{printf "%.4f %d\n", $1 / 1e6, $2}'
}

# Little-endian unsigned integer of 'size' bytes at 'offset' of a file.
readHeader() {
    od -An -t "u$3" -j "$2" -N "$3" "$1" | tr -d ' '
}

echo "file,step,kbps,ratio,snr_db,enc_s,dec_s,enc_kb,dec_kb" > "$CSV"

for q in "${STEPS[@]}"; do
    for f in "$DIR"/*.wav; do
        read -r encS encKB < <(measure "$BIN/wav_lossy_enc" $ENC_OPTS -q "$q" "$f" "$TMP/x.lsy")
        read -r decS decKB < <(measure "$BIN/wav_lossy_dec" "$TMP/x.lsy" "$TMP/x.wav")
        snr=$("$BIN/wav_snr" "$f" "$TMP/x.wav" | awk '/^all/ {print $4}')

        # The decoded WAV has a 44-byte header written by us: byte rate at offset 28.
        byteRate=$(readHeader "$TMP/x.wav" 28 4)
        pcmBytes=$(( $(stat -c %s "$TMP/x.wav") - 44 ))
        lsyBytes=$(stat -c %s "$TMP/x.lsy")

        awk -v f="$(basename "$f")" -v q="$q" -v pcm="$pcmBytes" -v lsy="$lsyBytes" \
            -v br="$byteRate" -v snr="$snr" -v es="$encS" -v ds="$decS" -v ek="$encKB" -v dk="$decKB" \
            'BEGIN { printf "%s,%s,%.1f,%.2f,%s,%s,%s,%s,%s\n",
                     f, q, lsy * 8 / (pcm / br) / 1000, pcm / lsy, snr, es, ds, ek, dk }' >> "$CSV"
    done
done

# Averages per step, as a Markdown table.
awk -F, 'NR > 1 {
        n[$2]++; kbps[$2] += $3; ratio[$2] += $4; snr[$2] += $5
        es[$2] += $6; ds[$2] += $7; ek[$2] += $8; dk[$2] += $9
        if (!($2 in seen)) { seen[$2] = 1; order[++count] = $2 }
    }
    END {
        print "| step | kbps | ratio | SNR (dB) | enc (s) | dec (s) | enc mem (MB) | dec mem (MB) |"
        print "|---:|---:|---:|---:|---:|---:|---:|---:|"
        for (i = 1; i <= count; i++) {
            q = order[i]; c = n[q]
            printf "| %s | %.1f | %.2f | %.2f | %.3f | %.3f | %.1f | %.1f |\n",
                q, kbps[q] / c, ratio[q] / c, snr[q] / c, es[q] / c, ds[q] / c,
                ek[q] / c / 1024, dk[q] / c / 1024
        }
    }' "$CSV"
