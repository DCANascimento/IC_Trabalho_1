#!/usr/bin/env bash
# Compares the lossy codec with MP3 (LAME), AAC (FFmpeg's encoder) and Opus at the
# same bitrates: bitrate, compression ratio, SNR, encoding/decoding time and memory.
#
# Usage:   tools/compare_mpeg.sh [audio_folder] [results_folder]
# Default: ../AudioFiles  results/mpeg   (relative to the lossy folder)
#          RATES="64 128" tools/compare_mpeg.sh     (other target bitrates, kbps)
#
# Needs ffmpeg with libmp3lame and libopus (sudo apt install ffmpeg), python3, and
# pdflatex with pgfplots for the figure. The codec must be built (cmake --build build).
# Takes a few minutes.
#
# How the other codecs are measured (see the notes written in the results):
#   - ffmpeg encodes each WAV with its default settings for the codec and bitrate, into
#     the usual file format: .mp3, .m4a (AAC) and .opus (Ogg). These formats store the
#     encoder delay, so ffmpeg removes it when decoding and the output is aligned with
#     the original. The script checks this once per codec and stops if it is not.
#   - The decoded file is cut (or padded) to the exact length of the original, at the
#     original sample rate and sample format (Opus always decodes at 48 kHz and is
#     resampled to the original rate; the error of resampling alone is measured too).
#   - The bitrate and the ratio use the real size of the compressed file, container
#     headers included. It can differ from the target (Opus is variable-bitrate).
#   - Time and memory include starting the program: for ffmpeg that is a large program,
#     so a run without any codec (WAV -> WAV) is also measured, as a reference.
#   - Only mono and stereo files are compared (MP3 has at most 2 channels).

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"   # the lossy folder
BIN="$ROOT/build"
AUDIO="${1:-$ROOT/../AudioFiles}"
OUT="${2:-$ROOT/results/mpeg}"
read -r -a RATES <<< "${RATES:-32 48 64 96 128 160 192 256 320}"
CODECS=(ours mp3 aac opus)
CHECK_RATE="${RATES[$(( ${#RATES[@]} / 2 ))]}"   # bitrate of the alignment check and time table

[ -x "$BIN/wav_lossy_enc" ] || { echo "Build the codec first: cmake --build build" >&2; exit 1; }
command -v ffmpeg > /dev/null || { echo "ffmpeg not found: sudo apt install ffmpeg" >&2; exit 1; }
ENCODERS=$(ffmpeg -hide_banner -encoders 2> /dev/null)
for enc in libmp3lame aac libopus; do
    grep -q " $enc " <<< "$ENCODERS" || { echo "ffmpeg has no $enc encoder" >&2; exit 1; }
done

shopt -s nullglob
FILES=("$AUDIO"/*.wav "$AUDIO"/*.WAV)
[ ${#FILES[@]} -gt 0 ] || { echo "No .wav files in $AUDIO" >&2; exit 1; }
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Runs a command and prints "<seconds> <peak memory in KB>" (as in bench.sh).
measure() {
    local t0 t1
    t0=$(date +%s%N)
    /usr/bin/time -f "%M" -o "$TMP/mem" "$@" > /dev/null
    t1=$(date +%s%N)
    echo "$(( (t1 - t0) / 1000 )) $(cat "$TMP/mem")" | awk '{printf "%.4f %d\n", $1 / 1e6, $2}'
}

# "frames rate channels bits" of a WAV file
wav_info() {
    python3 -c "import sys, wave; w = wave.open(sys.argv[1]);
print(w.getnframes(), w.getframerate(), w.getnchannels(), 8 * w.getsampwidth())" "$1"
}

# best_lag <original> <decoded>: shift (in samples) that best aligns the two files,
# searched within ±1200 samples (more than the delay of the three codecs) on a part
# of the first channel taken from the middle of the file.
best_lag() {
    python3 - "$1" "$2" << 'EOF'
import array, sys, wave
def channel0(path):
    with wave.open(path) as w:
        n, ch = w.getsampwidth(), w.getnchannels()
        data = w.readframes(w.getnframes())
    step = n * ch
    return [int.from_bytes(data[i:i + n], "little", signed=n > 1) for i in range(0, len(data), step)]
x, y = channel0(sys.argv[1]), channel0(sys.argv[2])
start, size, reach = len(x) // 2, 2048, 1200
ref = x[start:start + size]
best = min(range(-reach, reach + 1),
           key=lambda lag: sum((a - b) ** 2 for a, b in zip(ref, y[start + lag:start + lag + size])))
print(best)
EOF
}

declare -A EXT=([mp3]=mp3 [aac]=m4a [opus]=opus)
declare -A ENCODER=([mp3]=libmp3lame [aac]=aac [opus]=libopus)
declare -A PCM=([8]=pcm_u8 [16]=pcm_s16le [24]=pcm_s24le [32]=pcm_s32le)

for codec in "${CODECS[@]}"; do
    echo "file,step,kbps,ratio,snr_db,enc_s,dec_s,enc_kb,dec_kb" > "$OUT/$codec.csv"
done
# Per file: duration, ffmpeg without codec, and our encoder in one pass (see below).
echo "file,seconds,copy_s,copy_kb,onepass_s,onepass_kb" > "$OUT/reference.csv"

SKIPPED=()
for f in "${FILES[@]}"; do
    name=$(basename "$f")
    read -r frames rate channels bits < <(wav_info "$f")
    if [ "$channels" -gt 2 ]; then SKIPPED+=("$name ($channels channels)"); continue; fi
    pcmBytes=$(( frames * channels * bits / 8 ))
    echo "$name"

    # Reference: ffmpeg without any codec (WAV -> WAV)
    read -r copyS copyKB < <(measure ffmpeg -nostdin -v error -y -i "$f" -c:a "${PCM[$bits]}" "$TMP/copy.wav")
    # Our encoder in one pass: -b runs 16 encodings to find the step; this is the time
    # of a single encoding with the step it found for CHECK_RATE.
    step=$("$BIN/wav_lossy_enc" -v -b "$CHECK_RATE" "$f" "$TMP/c.lsy" 2>&1 | awk '/^Rate control/ {print $NF}')
    read -r oneS oneKB < <(measure "$BIN/wav_lossy_enc" -q "$step" "$f" "$TMP/c.lsy")
    echo "$name,$(awk -v n="$frames" -v r="$rate" 'BEGIN { print n / r }'),$copyS,$copyKB,$oneS,$oneKB" >> "$OUT/reference.csv"

    for r in "${RATES[@]}"; do
        for codec in "${CODECS[@]}"; do
            if [ "$codec" = ours ]; then
                comp="$TMP/c.lsy"
                read -r encS encKB < <(measure "$BIN/wav_lossy_enc" -b "$r" "$f" "$comp")
                read -r decS decKB < <(measure "$BIN/wav_lossy_dec" "$comp" "$TMP/d.wav")
            else
                comp="$TMP/c.${EXT[$codec]}"
                read -r encS encKB < <(measure ffmpeg -nostdin -v error -y -i "$f" \
                    -c:a "${ENCODER[$codec]}" -b:a "${r}k" "$comp")
                read -r decS decKB < <(measure ffmpeg -nostdin -v error -y -i "$comp" \
                    -c:a "${PCM[$bits]}" \
                    -af "aresample=$rate,apad=whole_len=$frames,atrim=end_sample=$frames" "$TMP/d.wav")
                # Once per codec: the decoded file must be aligned with the original.
                if [ "$r" = "$CHECK_RATE" ] && [ "$f" = "${FILES[0]}" ]; then
                    lag=$(best_lag "$f" "$TMP/d.wav")
                    if [ "$lag" != 0 ]; then
                        echo "$codec: decoded file is shifted by $lag samples; the SNR would be wrong" >&2
                        exit 1
                    fi
                    echo "  $codec: alignment checked (shift 0)"
                fi
            fi
            snr=$("$BIN/wav_snr" "$f" "$TMP/d.wav" | awk '/^all/ {print $4}')
            size=$(stat -c %s "$comp")
            awk -v f="$name" -v r="$r" -v size="$size" -v pcm="$pcmBytes" -v secs="$(( frames ))" \
                -v rate="$rate" -v snr="$snr" -v es="$encS" -v ds="$decS" -v ek="$encKB" -v dk="$decKB" \
                'BEGIN { printf "%s,%s,%.1f,%.2f,%s,%s,%s,%s,%s\n",
                         f, r, size * 8 / (secs / rate) / 1000, pcm / size, snr, es, ds, ek, dk }' \
                >> "$OUT/$codec.csv"
        done
    done
done

# Tables (Markdown) and the averaged curves for the figure.
python3 - "$OUT" "$CHECK_RATE" "${SKIPPED[@]+"${SKIPPED[@]}"}" << 'EOF' > "$OUT/tables.md"
import csv, sys
out, check_rate, skipped = sys.argv[1], sys.argv[2], sys.argv[3:]
codecs = ["ours", "mp3", "aac", "opus"]
names = {"ours": "ours (v2)", "mp3": "MP3 (LAME)", "aac": "AAC (FFmpeg)", "opus": "Opus"}
cols = ["kbps", "ratio", "snr_db", "enc_s", "dec_s", "enc_kb", "dec_kb"]

def averages(path):
    rows = {}
    for row in csv.DictReader(open(path)):
        rows.setdefault(row["step"], []).append(row)
    return {r: {c: sum(float(x[c]) for x in v) / len(v) for c in cols} for r, v in rows.items()}

avg = {c: averages(f"{out}/{c}.csv") for c in codecs}
rates = sorted(avg["ours"], key=float)
p = print

p("## SNR (dB) at the same target bitrate (higher is better)\n")
p("| target bitrate (kbps) | " + " | ".join(names[c] for c in codecs) + " |")
p("|---:|" + "---:|" * len(codecs))
for r in rates:
    p(f"| {r} | " + " | ".join(f"{avg[c][r]['snr_db']:.2f}" for c in codecs) + " |")

p("\n## Real bitrate (kbps) and compression ratio\n")
p("The bitrate is measured from the size of the compressed file (container included).\n")
p("| target bitrate (kbps) | " + " | ".join(f"{names[c]} kbps | ratio" for c in codecs) + " |")
p("|---:|" + "---:|---:|" * len(codecs))
for r in rates:
    p(f"| {r} | " + " | ".join(f"{avg[c][r]['kbps']:.1f} | {avg[c][r]['ratio']:.2f}" for c in codecs) + " |")

ref = list(csv.DictReader(open(f"{out}/reference.csv")))
mean = lambda key: sum(float(x[key]) for x in ref) / len(ref)
p(f"\n## Time and memory at {check_rate} kbps\n")
p(f"Average per file ({mean('seconds'):.1f} s of audio on average). Peak resident memory of the program.\n")
p("| codec | encoding time (s) | decoding time (s) | encoder memory (MB) | decoder memory (MB) |")
p("|---|---:|---:|---:|---:|")
for c in codecs:
    a = avg[c][check_rate]
    p(f"| {names[c]} | {a['enc_s']:.3f} | {a['dec_s']:.3f} | {a['enc_kb'] / 1024:.1f} | {a['dec_kb'] / 1024:.1f} |")
p(f"| ours, one encoding with the step found by `-b` (`-q`) | {mean('onepass_s'):.3f} | — | {mean('onepass_kb') / 1024:.1f} | — |")
p(f"| ffmpeg without codec (WAV → WAV, reference) | {mean('copy_s'):.3f} | — | {mean('copy_kb') / 1024:.1f} | — |")

with open(f"{out}/rd_average.csv", "w") as f:
    f.write("rate," + ",".join(f"{c}_kbps,{c}_snr" for c in codecs) + "\n")
    for r in rates:
        f.write(r + "," + ",".join(f"{avg[c][r]['kbps']:.2f},{avg[c][r]['snr_db']:.3f}" for c in codecs) + "\n")

if skipped:
    p("\nNot compared (more than 2 channels): " + ", ".join(skipped))
EOF

# Error of the resampling alone (original rate -> 48 kHz -> original rate), as done for Opus.
read -r frames rate _ bits < <(wav_info "${FILES[0]}")
ffmpeg -nostdin -v error -y -i "${FILES[0]}" -af aresample=48000 "$TMP/r48.wav"
ffmpeg -nostdin -v error -y -i "$TMP/r48.wav" -c:a "${PCM[$bits]}" \
    -af "aresample=$rate,apad=whole_len=$frames,atrim=end_sample=$frames" "$TMP/r.wav"
RESAMPLE_SNR=$("$BIN/wav_snr" "${FILES[0]}" "$TMP/r.wav" | awk '/^all/ {print $4}')

cd "$OUT"
{
    echo "# Lossy codec vs MP3, AAC and Opus"
    echo
    echo "Files:"
    for f in "${FILES[@]}"; do echo "- $(basename "$f")"; done
    echo
    echo "Every value is the mean over the files. Versions: $(ffmpeg -version | head -1 | cut -d' ' -f1-3)."
    echo
    cat "$OUT/tables.md"
    echo
    echo "## BD-rate against our codec (equal SNR)"
    echo
    echo "Positive: the codec needs more bits than ours for the same SNR."
    echo "Only the SNR range shared with our codec is used."
    echo
    "$ROOT/tools/compare.py" ours.csv mp3.csv aac.csv opus.csv | tail -n +3 |
        sed 's/| compared with | BD-rate (%) | BD-SNR (dB) |/| codec | BD-rate (bits at equal SNR, %) | BD-SNR (SNR at equal bitrate, dB) |/;
             s/| mp3.csv |/| MP3 (LAME) |/; s/| aac.csv |/| AAC (FFmpeg) |/; s/| opus.csv |/| Opus |/'
    echo
    echo "## How it was measured"
    echo
    echo "* MP3, AAC and Opus are encoded by ffmpeg with its default settings for each codec and"
    echo "  bitrate (\`-b:a\`), into the usual file formats: .mp3, .m4a and .opus."
    echo "* These formats store the encoder delay, so ffmpeg removes it when decoding. The script"
    echo "  checks once per codec that the decoded file is aligned with the original (shift 0);"
    echo "  without this the SNR would be meaningless."
    echo "* The decoded file is cut to the exact length of the original (AAC adds padding at the"
    echo "  end), at the original sample rate. Opus always decodes at 48 kHz, so it is resampled"
    echo "  to the original rate. The resampling alone (to 48 kHz and back, $(basename "${FILES[0]}"))"
    echo "  has an SNR of $RESAMPLE_SNR dB, so it does not limit the SNR of Opus."
    echo "* The bitrate and the ratio use the real size of the file, container headers included."
    echo "  The real bitrate can be a few % above the target (Opus is variable-bitrate by default;"
    echo "  .m4a and .opus have container headers; ours has a 30-byte header). The SNR table is at"
    echo "  the target bitrate, which slightly favours the codecs that use more bits; the BD-rate"
    echo "  and the figure use the real bitrates."
    echo "* AAC uses FFmpeg's own encoder; the best AAC encoder (fdk-aac) is not included in"
    echo "  Ubuntu's ffmpeg because of its licence, and would do somewhat better."
    echo "* Time and memory are measured for the whole program. ffmpeg is a large program: the"
    echo "  reference row (WAV → WAV without any codec) shows how much of it is not the codec."
    echo "  Our encoder with \`-b\` runs 16 encodings to find the step (rate control); the row"
    echo "  with \`-q\` is the time of one encoding, comparable to the one-pass MPEG encoders."
    echo "* The SNR is not perceived quality: MP3, AAC and Opus put the noise where it is not"
    echo "  heard (psychoacoustic model) and remove inaudible high frequencies. They are expected"
    echo "  to have a lower SNR than our codec and still sound better at the same bitrate."
} > README.md
rm tables.md

if command -v pdflatex > /dev/null; then
    (cd "$TMP" && pdflatex -interaction=batchmode \
        "\def\csvfile{$OUT/rd_average.csv}\input{$ROOT/tools/compare_mpeg.tex}" > /dev/null) ||
        { cp "$TMP/compare_mpeg.log" "$OUT/"; echo "pdflatex failed, see $OUT/compare_mpeg.log" >&2; exit 1; }
    cp "$TMP/compare_mpeg.pdf" "$OUT/rd_mpeg.pdf"
else
    echo "pdflatex not found: figure not drawn (the data is in rd_average.csv)" >&2
fi

echo "Done. Results in $OUT/README.md"
