#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
run_ps1="$script_dir/test-musicgen-hf-smoke.ps1"

if command -v pwsh >/dev/null 2>&1; then
	exec pwsh -NoProfile -ExecutionPolicy Bypass -File "$run_ps1" "$@"
fi

exec powershell -NoProfile -ExecutionPolicy Bypass -File "$run_ps1" "$@"
