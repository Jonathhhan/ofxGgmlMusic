#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
runner="$script_dir/../tools/musicgen_hf_runner.py"

if [ "${OFXGGML_MUSIC_PYTHON:-}" != "" ]; then
	exec "$OFXGGML_MUSIC_PYTHON" "$runner" "$@"
fi

if command -v python3 >/dev/null 2>&1; then
	exec python3 "$runner" "$@"
fi

if command -v python >/dev/null 2>&1; then
	exec python "$runner" "$@"
fi

printf '%s\n' "Could not find python or python3."
printf '%s\n' "Set OFXGGML_MUSIC_PYTHON to a Python executable with torch, transformers, and numpy installed."
exit 1
