#!/usr/bin/env bash
set -euo pipefail

data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
base_dir="$data_home/losslesscodec"
rm -f -- "$data_home/nautilus/scripts/LosslessCodec - Comprimir para CLSS" \
          "$data_home/nautilus/scripts/LosslessCodec - Descomprimir para WAV" \
          "$data_home/nemo/scripts/LosslessCodec - Comprimir para CLSS" \
          "$data_home/nemo/scripts/LosslessCodec - Descomprimir para WAV" \
          "$data_home/nautilus/scripts/LosslessCodec - Compress to CLSS" \
          "$data_home/nautilus/scripts/LosslessCodec - Decompress to WAV" \
          "$data_home/nemo/scripts/LosslessCodec - Compress to CLSS" \
          "$data_home/nemo/scripts/LosslessCodec - Decompress to WAV" \
          "$data_home/kio/servicemenus/losslesscodec-compress.desktop" \
          "$data_home/kio/servicemenus/losslesscodec-decompress.desktop" \
          "$data_home/mime/packages/losslesscodec-clss.xml" \
          "$base_dir/run-action" \
          "$base_dir/codec-path"
rmdir "$base_dir" 2>/dev/null || true

if command -v update-mime-database >/dev/null 2>&1 && [[ -d "$data_home/mime" ]]; then
    update-mime-database "$data_home/mime"
fi

# Remove just the two custom actions owned by this codec from Thunar's config.
config_home="\${XDG_CONFIG_HOME:-$HOME/.config}"
uca_file="$config_home/Thunar/uca.xml"
if [[ -f "$uca_file" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        python3 - "$uca_file" <<'PYTHON'
import re
import sys

filename = sys.argv[1]
with open(filename, "r", encoding="utf-8") as source:
    contents = source.read()
for identifier in ("losslesscodec-compress", "losslesscodec-decompress"):
    pattern = re.compile(
        r"<action(?:\s[^>]*)?>(?:(?!</action>).)*?"
        r"<unique-id>\s*" + re.escape(identifier) +
        r"\s*</unique-id>(?:(?!</action>).)*?</action>\s*",
        re.DOTALL,
    )
    contents = pattern.sub("", contents)
with open(filename, "w", encoding="utf-8", newline="\n") as target:
    target.write(contents)
PYTHON
    else
        printf 'Warning: Python 3 was not found; remove the LosslessCodec actions from Thunar manually.\n' >&2
    fi
fi

printf 'Lossless Codec actions removed for this user.\n'
