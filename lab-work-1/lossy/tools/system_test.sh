#!/usr/bin/env bash
# System tests of the lossy codec: builds it (also with AddressSanitizer and
# UndefinedBehaviorSanitizer), then runs encoder + decoder end to end on real and
# synthetic WAV files and checks the results. Prints PASS/FAIL per test.
#
# Usage:   tools/system_test.sh [audio_folder | file.wav]
# Default: ../AudioFiles   (relative to the lossy folder)
# Groups 1-5 use these files, groups 6-10 synthetic files made by the script.
#
# The tests run with the sanitizer build, so a memory error or undefined behaviour
# also makes a test fail. Needs cmake, a C++17 compiler (g++ or clang++) and python3.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"   # the lossy folder
AUDIO="${1:-$ROOT/../AudioFiles}"

# The real files: one file, or the .wav/.WAV files of a folder (not its subfolders).
shopt -s nullglob
if [ -f "$AUDIO" ]; then
    FILES=("$AUDIO")
elif [ -d "$AUDIO" ]; then
    FILES=("$AUDIO"/*.wav "$AUDIO"/*.WAV)
else
    echo "Not found: $AUDIO (give a WAV file or a folder with WAV files)" >&2
    exit 1
fi
if [ ${#FILES[@]} -eq 0 ]; then
    echo "No .wav files in $AUDIO (subfolders are not searched)" >&2
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# A sanitizer error exits with 99, so it is told apart from a normal error (1).
export ASAN_OPTIONS="exitcode=99:detect_leaks=1"
export UBSAN_OPTIONS="halt_on_error=1:exitcode=99:print_stacktrace=1"

PASSED=0
FAILED=0
pass() { PASSED=$((PASSED + 1)); printf '  PASS  %s\n' "$1"; }
fail() { FAILED=$((FAILED + 1)); printf '  FAIL  %s\n' "$1"; [ -n "${2:-}" ] && printf '        %s\n' "$2"; }
check() { if eval "$2"; then pass "$1"; else fail "$1" "${3:-}"; fi; }

# ---------------------------------------------------------------- build
echo "== Build"
if cmake -S "$ROOT" -B "$ROOT/build" > /dev/null && cmake --build "$ROOT/build" --clean-first > "$TMP/build.log" 2>&1; then
    warnings=$(grep -c "warning:" "$TMP/build.log")
    check "release build without warnings" '[ "$warnings" -eq 0 ]' "$warnings warnings, see the build output"
else
    fail "release build" "$(tail -5 "$TMP/build.log")"
    exit 1
fi

SAN="-fsanitize=address,undefined -fno-omit-frame-pointer"
if cmake -S "$ROOT" -B "$TMP/san" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="$SAN -O1" \
         -DCMAKE_EXE_LINKER_FLAGS="$SAN" > /dev/null && cmake --build "$TMP/san" > /dev/null 2>&1; then
    pass "sanitizer build (ASan + UBSan)"
else
    fail "sanitizer build" "the tests use the release build instead"
fi
BIN="$TMP/san"
[ -x "$BIN/wav_lossy_enc" ] || BIN="$ROOT/build"
ENC="$BIN/wav_lossy_enc"
DEC="$BIN/wav_lossy_dec"
SNR="$BIN/wav_snr"

# ---------------------------------------------------------------- helpers
# enc <options...> <in.wav> <out.lsy>; dec <in.lsy> <out.wav>   (stderr kept in $TMP/err)
enc() { "$ENC" "$@" 2> "$TMP/err"; }
dec() { "$DEC" "$@" 2> "$TMP/err"; }
# snr <a.wav> <b.wav>: "SNR maxError" of all channels ("" if the files do not match)
snr() { "$SNR" "$1" "$2" 2>/dev/null | awk '/^all/ {print $4, $3}'; }
# roundtrip <wav> <options...>: encodes and decodes into $TMP/rt.lsy / $TMP/rt.wav
roundtrip() {
    local wav="$1"; shift
    enc "$@" "$wav" "$TMP/rt.lsy" && dec "$TMP/rt.lsy" "$TMP/rt.wav"
}
kbps() { awk -v b="$(stat -c %s "$1")" -v s="$2" 'BEGIN { printf "%.2f", b * 8 / s / 1000 }'; }
# Bits per sample of a WAV decoded by us (44-byte header: bits at offset 34).
bits_of() { od -An -t u2 -j 34 -N 2 "$1" | tr -d ' '; }

# noise_check <original.wav> <decoded.wav> <step> <rounding>
# The SNR of a file depends on its loudness, but the noise added by the codec depends
# only on the step q (see tools/README.md). The worst interval of the quantizer is the
# deadzone, of width 2(1 - r)q, so the mean squared error is at most (1 - r)²q²/3
# (q²/12 for plain rounding), plus 1/12 LSB² from rounding the output to integers.
# Errors are compared in 16-bit units, as the codec measures q.
# Sets NOISE to a description; returns true if the error is within the limit.
noise_check() {
    local mse snr bits
    read -r mse snr < <("$SNR" "$1" "$2" 2> /dev/null | awk '/^all/ {print $2, $4}')
    if [ -z "$mse" ]; then NOISE="different format or length"; return 1; fi
    bits=$(bits_of "$2")
    NOISE=$(awk -v m="$mse" -v b="$bits" -v q="$3" -v r="$4" -v s="$snr" 'BEGIN {
        u = 4 ^ (16 - b)
        printf "MSE %.3f q², limit %.3f q², SNR %s dB", m * u / q^2, ((1 - r)^2 / 3 * q^2 + u / 12) / q^2, s }')
    awk -v m="$mse" -v b="$bits" -v q="$3" -v r="$4" \
        'BEGIN { u = 4 ^ (16 - b); exit !(m * u <= (1 - r)^2 / 3 * q^2 + u / 12) }'
}
# float comparison: cmp "<a> <op> <b>"
cmp_f() { awk "BEGIN { exit !($1) }"; }

# Synthetic files: gen <file> <bits> <channels> <rate> <frames> <signal>
# signal: music (sum of tones + noise), silence, square (full scale), same (L = R), noise
gen() {
    python3 - "$@" << 'EOF'
import math, random, struct, sys, wave
path, bits, ch, rate, frames, signal = sys.argv[1], *map(int, sys.argv[2:6]), sys.argv[6]
random.seed(1)
peak = 2 ** (bits - 1) - 1
out = bytearray()
for i in range(frames):
    t = i / rate
    base = (0.4 * math.sin(2 * math.pi * 440 * t) + 0.2 * math.sin(2 * math.pi * 1250 * t)
            + 0.05 * random.uniform(-1, 1))
    for c in range(ch):
        if signal == "silence":
            x = 0.0
        elif signal == "square":
            x = 1.0 if (i // 50) % 2 else -1.0
        elif signal == "noise":
            x = random.uniform(-0.9, 0.9)
        elif signal == "same":
            x = base
        else:
            x = base * (1 - 0.3 * c) + 0.1 * math.sin(2 * math.pi * (300 + 200 * c) * t)
        v = max(-peak - 1, min(peak, round(x * peak)))
        if bits == 8:
            out += bytes([v + 128])                  # 8-bit WAV is unsigned
        else:
            out += struct.pack("<i", v)[: bits // 8]
with wave.open(path, "wb") as w:
    w.setnchannels(ch); w.setsampwidth(bits // 8); w.setframerate(rate)
    w.writeframes(bytes(out))
EOF
}

# ---------------------------------------------------------------- 1. real files
# Not checked: the SNR (it depends on the loudness of the file) and the ratio
# (an 8-bit file coded with step 16 can grow). Both are printed.
echo "== 1. Real files ($AUDIO), default options (step 16)"
SKIPPED=0
REF=""   # first file the codec accepts: used by groups 2-5
for f in "${FILES[@]}"; do
    name=$(basename "$f")
    if ! enc "$f" "$TMP/rt.lsy"; then
        if grep -q "supported" "$TMP/err"; then   # e.g. float WAV: a known limitation
            SKIPPED=$((SKIPPED + 1)); printf '  SKIP  %s: %s\n' "$name" "$(head -1 "$TMP/err")"
        else
            fail "$name: encoding" "$(cat "$TMP/err")"
        fi
        continue
    fi
    if ! dec "$TMP/rt.lsy" "$TMP/rt.wav"; then
        fail "$name: decoding" "$(cat "$TMP/err")"
        continue
    fi
    [ -z "$REF" ] && REF="$f"
    ratio=$(awk -v a="$(stat -c %s "$f")" -v b="$(stat -c %s "$TMP/rt.lsy")" 'BEGIN { printf "%.1f", a / b }')
    noise_check "$f" "$TMP/rt.wav" 16 0.35; ok=$?
    check "$name: same format and length, error within the step ($NOISE, ratio $ratio)" '[ $ok -eq 0 ]'
done

if [ -z "$REF" ]; then
    echo "== 2-5 not run: no file could be coded"
else
    # ------------------------------------------------------------ 2. rate-distortion
    echo "== 2. Larger step -> smaller file and lower SNR ($(basename "$REF"))"
    prev_size=999999999; prev_snr=999; ok=1; line=""
    for q in 1 4 16 64 256 1024 4096; do
        roundtrip "$REF" -q "$q" || { ok=0; break; }
        size=$(stat -c %s "$TMP/rt.lsy"); read -r s _ < <(snr "$REF" "$TMP/rt.wav")
        line+="q$q: ${size}B ${s}dB  "
        { [ "$size" -lt "$prev_size" ] && cmp_f "$s < $prev_snr"; } || ok=0
        prev_size=$size; prev_snr=$s
    done
    check "size and SNR decrease monotonically" '[ $ok -eq 1 ]' "$line"

    # ------------------------------------------------------------ 3. determinism
    echo "== 3. Determinism"
    enc "$REF" "$TMP/a.lsy"; enc "$REF" "$TMP/b.lsy"
    check "two encodings give identical files" 'cmp -s "$TMP/a.lsy" "$TMP/b.lsy"'
    dec "$TMP/a.lsy" "$TMP/a.wav"; dec "$TMP/a.lsy" "$TMP/b.wav"
    check "two decodings give identical files" 'cmp -s "$TMP/a.wav" "$TMP/b.wav"'

    # ------------------------------------------------------------ 4. options
    echo "== 4. Encoder options ($(basename "$REF"))"
    for opts in "-n 16" "-n 256" "-n 512" "-n 2048" "-n 4096" "-t dct" "-t dct -n 256" \
                "-z 0" "-z 0.5" "--no-centroid" "--no-ms" "-z 0.5 --no-centroid --no-ms" "-v"; do
        if roundtrip "$REF" $opts; then
            r=0.35
            [[ " $opts " =~ " -z "([0-9.]+) ]] && r="${BASH_REMATCH[1]}"
            noise_check "$REF" "$TMP/rt.wav" 16 "$r"; ok=$?
            check "$opts ($NOISE)" '[ $ok -eq 0 ]'
        else
            fail "$opts" "$(cat "$TMP/err")"
        fi
    done
    enc -v "$REF" "$TMP/rt.lsy"
    check "-v prints statistics" 'grep -q "Mid/Side" "$TMP/err" && grep -q "Output" "$TMP/err"'
    enc "$REF" "$TMP/ms.lsy"; enc --no-ms "$REF" "$TMP/lr.lsy"
    check "Mid/Side never makes the file noticeably larger" \
          '[ "$(stat -c %s "$TMP/ms.lsy")" -le $(( $(stat -c %s "$TMP/lr.lsy") * 1001 / 1000 )) ]'

    # ------------------------------------------------------------ 5. rate control
    echo "== 5. Rate control (-b)"
    secs=$(python3 -c "import wave,sys; w=wave.open(sys.argv[1]); print(w.getnframes()/w.getframerate())" "$REF")
    for target in 32 64 128 256; do
        if enc -b "$target" "$REF" "$TMP/rt.lsy"; then
            k=$(kbps "$TMP/rt.lsy" "$secs")
            check "-b $target -> $k kbps (between 97 % and 100.5 % of the target)" \
                  'cmp_f "$k <= $target * 1.005 && $k >= $target * 0.97"'
        else
            fail "-b $target" "$(cat "$TMP/err")"
        fi
    done
    enc -b 100000 "$REF" "$TMP/rt.lsy"; code=$?
    check "-b higher than possible: uses the smallest step" '[ $code -eq 0 ]'
    enc -b 0.001 "$REF" "$TMP/rt.lsy"; code=$?
    check "-b lower than possible: uses the largest step" '[ $code -eq 0 ]'
fi

# ---------------------------------------------------------------- 6. formats
echo "== 6. Sample formats, channels, rates (synthetic, 1 s)"
for spec in "8 1 8000" "8 2 22050" "16 1 44100" "16 2 44100" "16 3 48000" "16 6 48000" \
            "24 2 96000" "32 2 44100"; do
    read -r bits ch rate <<< "$spec"
    w="$TMP/f_${bits}_${ch}_${rate}.wav"
    gen "$w" "$bits" "$ch" "$rate" "$rate" music
    if roundtrip "$w"; then
        read -r s _ < <(snr "$w" "$TMP/rt.wav")
        check "$bits bits, $ch ch, $rate Hz: header kept, SNR $s dB" '[ -n "$s" ]'
    else
        fail "$bits bits, $ch ch, $rate Hz" "$(cat "$TMP/err")"
    fi
done

echo "== 7. Smallest step: (almost) lossless"
for spec in "8 1" "16 1" "16 2" "16 3"; do
    read -r bits ch <<< "$spec"
    w="$TMP/l_${bits}_${ch}.wav"; gen "$w" "$bits" "$ch" 44100 44100 music
    roundtrip "$w" -q 0.01; read -r s e < <(snr "$w" "$TMP/rt.wav")
    check "$bits bits, $ch ch, -q 0.01: max error 0 (got $e)" '[ "$e" = "0" ]'
done
w="$TMP/l_24.wav"; gen "$w" 24 2 44100 44100 music
roundtrip "$w" -q 0.01; read -r s e < <(snr "$w" "$TMP/rt.wav")
check "24 bits, -q 0.01: max error <= 8 LSB, step 0.01 > 1 LSB (got $e, SNR $s)" '[ -n "$e" ] && [ "$e" -le 8 ]'

# ---------------------------------------------------------------- 8. edge cases
echo "== 8. Lengths and signals"
for frames in 0 1 2 1023 1024 1025 2049; do
    w="$TMP/len_$frames.wav"; gen "$w" 16 2 44100 "$frames" music
    if roundtrip "$w"; then
        check "$frames frames: decoded length is the same" '"$SNR" "$w" "$TMP/rt.wav" > /dev/null 2>&1'
    else
        fail "$frames frames" "$(cat "$TMP/err")"
    fi
done
w="$TMP/silence.wav"; gen "$w" 16 2 44100 44100 silence
roundtrip "$w"; read -r s e < <(snr "$w" "$TMP/rt.wav")
check "silence: decoded is silence, file is tiny ($(stat -c %s "$TMP/rt.lsy") bytes)" \
      '[ "$e" = "0" ] && [ "$(stat -c %s "$TMP/rt.lsy")" -lt 2000 ]'
w="$TMP/square.wav"; gen "$w" 16 2 44100 44100 square
roundtrip "$w" -q 64; read -r s e < <(snr "$w" "$TMP/rt.wav")
check "full-scale square: no wrap-around when clipping (SNR $s dB)" '[ -n "$s" ] && cmp_f "$s > 20"'
w="$TMP/noise.wav"; gen "$w" 16 2 44100 44100 noise
roundtrip "$w" -q 1; read -r s e < <(snr "$w" "$TMP/rt.wav")
check "white noise (incompressible), -q 1: works (SNR $s dB)" '[ -n "$s" ] && cmp_f "$s > 40"'
w="$TMP/same.wav"; gen "$w" 16 2 44100 44100 same
enc "$w" "$TMP/ms.lsy"; enc --no-ms "$w" "$TMP/lr.lsy"
check "L = R: Mid/Side almost halves the file ($(stat -c %s "$TMP/ms.lsy") vs $(stat -c %s "$TMP/lr.lsy") bytes)" \
      '[ "$(stat -c %s "$TMP/ms.lsy")" -lt $(( $(stat -c %s "$TMP/lr.lsy") * 6 / 10 )) ]'

# ---------------------------------------------------------------- 9. errors
echo "== 9. Invalid input: error message and exit code 1, no crash"
w="$TMP/len_1024.wav"
gen "$TMP/ok.wav" 16 2 44100 44100 music
enc "$TMP/ok.wav" "$TMP/ok.lsy"
expect_error() {   # expect_error <description> <command...>
    local d="$1"; shift
    "$@" > /dev/null 2> "$TMP/err"; local code=$?
    check "$d (exit $code)" '[ $code -eq 1 ] && [ -s "$TMP/err" ]' "$(head -3 "$TMP/err")"
}
expect_error "encoder without arguments"     "$ENC"
expect_error "decoder without arguments"     "$DEC"
expect_error "-q 0"                          "$ENC" -q 0 "$w" "$TMP/x.lsy"
expect_error "-q -5"                         "$ENC" -q -5 "$w" "$TMP/x.lsy"
expect_error "-q abc"                        "$ENC" -q abc "$w" "$TMP/x.lsy"
expect_error "-b 0"                          "$ENC" -b 0 "$w" "$TMP/x.lsy"
expect_error "-z 0.7"                        "$ENC" -z 0.7 "$w" "$TMP/x.lsy"
expect_error "-n 1000 (not a power of two)"  "$ENC" -n 1000 "$w" "$TMP/x.lsy"
expect_error "-n 8192 (too large)"           "$ENC" -n 8192 "$w" "$TMP/x.lsy"
expect_error "-t wavelet"                    "$ENC" -t wavelet "$w" "$TMP/x.lsy"
expect_error "unknown option"                "$ENC" --fast "$w" "$TMP/x.lsy"
expect_error "option without value"          "$ENC" -q
expect_error "input file does not exist"     "$ENC" "$TMP/none.wav" "$TMP/x.lsy"
echo "hello" > "$TMP/text.wav"
expect_error "input is not a WAV file"       "$ENC" "$TMP/text.wav" "$TMP/x.lsy"
python3 -c "
import struct,sys
d = struct.pack('<ffff', 0.1, -0.1, 0.2, -0.2)
fmt = struct.pack('<HHIIHH', 3, 2, 44100, 44100*8, 8, 32)
sys.stdout.buffer.write(b'RIFF' + struct.pack('<I', 36+len(d)) + b'WAVEfmt ' + struct.pack('<I',16) + fmt + b'data' + struct.pack('<I',len(d)) + d)" > "$TMP/float.wav"
expect_error "float WAV (not integer PCM)"   "$ENC" "$TMP/float.wav" "$TMP/x.lsy"
expect_error "output folder does not exist"  "$ENC" "$w" "$TMP/no/dir/x.lsy"
expect_error "compressed file does not exist" "$DEC" "$TMP/none.lsy" "$TMP/x.wav"
expect_error "compressed file is a WAV"      "$DEC" "$w" "$TMP/x.wav"
: > "$TMP/empty.lsy"
expect_error "compressed file is empty"      "$DEC" "$TMP/empty.lsy" "$TMP/x.wav"
size=$(stat -c %s "$TMP/ok.lsy")
for cut in 4 20 32 $((size / 2)) $((size - 1)); do
    head -c "$cut" "$TMP/ok.lsy" > "$TMP/cut.lsy"
    expect_error "compressed file cut at $cut of $size bytes" "$DEC" "$TMP/cut.lsy" "$TMP/x.wav"
done

echo "== 10. Corrupted compressed files (200 random byte changes): no crash"
crashes=0
for i in $(seq 1 200); do
    python3 -c "
import random,sys
random.seed(int(sys.argv[3]))
d = bytearray(open(sys.argv[1],'rb').read())
for _ in range(random.randint(1, 4)):
    p = random.randrange(len(d)); d[p] = random.randrange(256)
open(sys.argv[2],'wb').write(d)" "$TMP/ok.lsy" "$TMP/bad.lsy" "$i"
    timeout 20 "$DEC" "$TMP/bad.lsy" "$TMP/bad.wav" > /dev/null 2> "$TMP/err"
    code=$?
    if [ $code -ne 0 ] && [ $code -ne 1 ]; then
        crashes=$((crashes + 1)); cp "$TMP/bad.lsy" "$ROOT/results/crash_$i.lsy" 2> /dev/null
    fi
done
check "decoder returns 0 or 1 for every corrupted file" '[ $crashes -eq 0 ]' \
      "$crashes crashes/timeouts (copied to results/crash_*.lsy)"

# ---------------------------------------------------------------- summary
echo
echo "Passed: $PASSED   Failed: $FAILED   Skipped: $SKIPPED"
[ "$FAILED" -eq 0 ]
