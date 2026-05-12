#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if command -v pwsh >/dev/null 2>&1; then
	pwsh -NoProfile -ExecutionPolicy Bypass -File "$SCRIPT_DIR/generate-procedural-music.ps1" "$@"
elif command -v powershell >/dev/null 2>&1; then
	powershell -NoProfile -ExecutionPolicy Bypass -File "$SCRIPT_DIR/generate-procedural-music.ps1" "$@"
else
	echo "PowerShell or pwsh is required for this helper script." >&2
	exit 1
fi
