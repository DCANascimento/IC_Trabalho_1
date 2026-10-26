#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
codec_input="${1:-$project_dir/build/MyLosslessCodec}"
if [[ "$codec_input" == /* ]]; then
    codec="$codec_input"
else
    codec="$(cd -- "$(dirname -- "$codec_input")" && pwd -P)/$(basename -- "$codec_input")"
fi
if [[ ! -x "$codec" ]]; then
    printf 'Executable not found: %s\nBuild the project with CMake first.\n' "$codec" >&2
    exit 1
fi

data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
base_dir="$data_home/losslesscodec"
runner="$base_dir/run-action"
mkdir -p "$base_dir"
printf '%s\n' "$codec" > "$base_dir/codec-path"

cat > "$runner" <<'RUNNER'
#!/usr/bin/env bash
set -uo pipefail

base_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ ! -r "$base_dir/codec-path" ]]; then
    printf 'The configured codec executable could not be found.\n' >&2
    exit 1
fi
IFS= read -r codec < "$base_dir/codec-path"

mode="${1:-}"
if [[ "$mode" != "-c" && "$mode" != "-d" ]]; then
    printf 'Invalid mode for the file manager action.\n' >&2
    exit 2
fi
shift
if (($# == 0)); then
    printf 'No local files were selected.\n' >&2
    exit 2
fi

cache_home="${XDG_CACHE_HOME:-$HOME/.cache}"
log_dir="$cache_home/losslesscodec"
mkdir -p "$log_dir"
successes=0
failures=()

for file in "$@"; do
    lower_file="${file,,}"
    if [[ "$mode" == "-c" && "$lower_file" != *.wav ]]; then
        failures+=("$file (expected a .wav file)")
        continue
    fi
    if [[ "$mode" == "-d" && "$lower_file" != *.clss ]]; then
        failures+=("$file (expected a .clss file)")
        continue
    fi

    log_file="$(mktemp "$log_dir/execucao-XXXXXX.log")"
    if "$codec" "$mode" "$file" >"$log_file" 2>&1; then
        successes=$((successes + 1))
        rm -f -- "$log_file"
    else
        failures+=("$file (details in $log_file)")
    fi
done

if (("${#failures[@]}" > 0)); then
    summary="Completed: $successes; failed: ${#failures[@]}."
    details="$(printf '%s\n' "${failures[@]}")"
    message="$summary"$'\n\n'"$details"
    if command -v zenity >/dev/null 2>&1; then
        zenity --error --title='Lossless Codec' --text="$message" 2>/dev/null || true
    elif command -v kdialog >/dev/null 2>&1; then
        kdialog --error "$message" --title='Lossless Codec' 2>/dev/null || true
    elif command -v notify-send >/dev/null 2>&1; then
        notify-send 'Lossless Codec: error' "$summary" 2>/dev/null || true
    else
        printf '%s\n' "$message" >&2
    fi
elif command -v notify-send >/dev/null 2>&1; then
    notify-send 'Lossless Codec' "Operation completed for $successes file(s)." 2>/dev/null || true
elif command -v zenity >/dev/null 2>&1; then
    zenity --info --title='Lossless Codec' --text="Operation completed for $successes file(s)." 2>/dev/null || true
fi
RUNNER
chmod 755 "$runner"

write_file_manager_script() {
    local destination="$1"
    local action_name="$2"
    local mode="$3"
    local runner_quoted mode_quoted

    mkdir -p "$destination"
    printf -v runner_quoted '%q' "$runner"
    printf -v mode_quoted '%q' "$mode"
    printf '#!/usr/bin/env bash\nexec %s %s "$@"\n' \
        "$runner_quoted" "$mode_quoted" > "$destination/$action_name"
    chmod 755 "$destination/$action_name"
}

# Nautilus e Nemo apresentam estes scripts no submenu Scripts do botão direito.
for scripts_dir in "$data_home/nautilus/scripts" "$data_home/nemo/scripts"; do
    write_file_manager_script "$scripts_dir" 'LosslessCodec - Compress to CLSS' '-c'
    write_file_manager_script "$scripts_dir" 'LosslessCodec - Decompress to WAV' '-d'
done

# Regista a extensão CLSS para o menu de serviços do Dolphin.
mime_package_dir="$data_home/mime/packages"
mkdir -p "$mime_package_dir"
cat > "$mime_package_dir/losslesscodec-clss.xml" <<'MIME'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/x-losslesscodec-clss">
    <comment>Lossless Codec compressed audio</comment>
    <glob pattern="*.clss"/>
    <glob pattern="*.CLSS"/>
  </mime-type>
</mime-info>
MIME
if command -v update-mime-database >/dev/null 2>&1; then
    update-mime-database "$data_home/mime"
else
    printf 'Warning: update-mime-database was not found; the Dolphin CLSS action may not appear until the MIME database is updated.\n' >&2
fi

desktop_quote() {
    local value="$1"
    local tick char output='"' index
    printf -v tick '%b' '\140'
    for ((index = 0; index < ${#value}; ++index)); do
        char="${value:index:1}"
        case "$char" in
            '\'|'"'|'$'|"$tick") output+="\\$char" ;;
            %) output+="%%" ;;
            *) output+="$char" ;;
        esac
    done
    output+='"'
    printf '%s' "$output"
}
runner_desktop="$(desktop_quote "$runner")"
service_dir="$data_home/kio/servicemenus"
mkdir -p "$service_dir"

cat > "$service_dir/losslesscodec-compress.desktop" <<DESKTOP
[Desktop Entry]
Type=Service
MimeType=audio/wav;audio/x-wav;audio/vnd.wave;audio/x-pn-wav;
Actions=compress;
X-KDE-Priority=TopLevel

[Desktop Action compress]
Name=Compress to CLSS
Icon=audio-x-generic
Exec=$runner_desktop -c %F
DESKTOP

cat > "$service_dir/losslesscodec-decompress.desktop" <<DESKTOP
[Desktop Entry]
Type=Service
MimeType=application/x-losslesscodec-clss;
Actions=decompress;
X-KDE-Priority=TopLevel

[Desktop Action decompress]
Name=Decompress to WAV
Icon=audio-x-generic
Exec=$runner_desktop -d %F
DESKTOP
chmod 755 "$service_dir/losslesscodec-compress.desktop" "$service_dir/losslesscodec-decompress.desktop"

printf 'Actions installed for this user.\n'
# Thunar stores user custom actions in an XML file. Merge only our own IDs.
if command -v thunar >/dev/null 2>&1; then
    if command -v python3 >/dev/null 2>&1; then
        config_home="\${XDG_CONFIG_HOME:-$HOME/.config}"
        uca_file="$config_home/Thunar/uca.xml"
        mkdir -p "$(dirname -- "$uca_file")"
        python3 - "$uca_file" "$runner" <<'PYTHON'
import os
import re
import shlex
import sys
import xml.etree.ElementTree as ET

filename, runner = sys.argv[1:3]
if os.path.exists(filename):
    with open(filename, "r", encoding="utf-8") as source:
        contents = source.read()
else:
    contents = '<?xml version="1.0" encoding="UTF-8"?>\n<actions>\n</actions>\n'

def remove_codec_actions(text):
    for identifier in ("losslesscodec-compress", "losslesscodec-decompress"):
        pattern = re.compile(
            r"<action(?:\s[^>]*)?>(?:(?!</action>).)*?"
            r"<unique-id>\s*" + re.escape(identifier) +
            r"\s*</unique-id>(?:(?!</action>).)*?</action>\s*",
            re.DOTALL,
        )
        text = pattern.sub("", text)
    return text

contents = remove_codec_actions(contents)
close_tag = contents.rfind("</actions>")
if close_tag < 0:
    raise SystemExit("Thunar uca.xml has no closing </actions> element.")

quoted_runner = shlex.quote(runner)
actions = []
for identifier, label, mode, pattern, file_kind in (
    ("losslesscodec-compress", "LosslessCodec - Compress to CLSS",
     "-c", "*.wav;*.WAV", "audio-files"),
    ("losslesscodec-decompress", "LosslessCodec - Decompress to WAV",
     "-d", "*.clss;*.CLSS", "other-files"),
):
    action = ET.Element("action")
    for tag, value in (
        ("icon", "audio-x-generic"),
        ("name", label),
        ("submenu", ""),
        ("unique-id", identifier),
        ("command", f'{quoted_runner} {mode} %F'),
        ("description", label),
        ("range", ""),
        ("patterns", pattern),
    ):
        ET.SubElement(action, tag).text = value
    ET.SubElement(action, file_kind)
    actions.append(ET.tostring(action, encoding="unicode"))

contents = contents[:close_tag] + "\n" + "\n".join(actions) + "\n" + contents[close_tag:]
with open(filename, "w", encoding="utf-8", newline="\n") as target:
    target.write(contents)
PYTHON
    else
        printf 'Warning: Thunar was detected, but Python 3 was not; Thunar actions were not installed.\n' >&2
    fi
fi

printf 'Nautilus/Nemo: Scripts submenu. Dolphin and Thunar: context menu actions.\n'
printf 'If your file manager was already open, close it and reopen it.\n'
