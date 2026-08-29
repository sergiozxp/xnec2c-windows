#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_dir="$script_dir/launcher"
output="${1:-$script_dir/xnec2c-launcher.exe}"
compiler="${CC:-gcc}"
resource_compiler="${WINDRES:-windres}"
resource_object="$(mktemp "${TMPDIR:-/tmp}/xnec2c-launcher.XXXXXX.o")"

cleanup() {
  rm -f -- "$resource_object"
}
trap cleanup EXIT

if [[ "${MSYSTEM:-}" != "UCRT64" || "${MINGW_PREFIX:-}" != "/ucrt64" ]]; then
  echo "error: build the launcher from an MSYS2 UCRT64 shell" >&2
  exit 1
fi

mkdir -p -- "$(dirname -- "$output")"

(
  cd "$source_dir"
  "$resource_compiler" -O coff -i xnec2c-launcher.rc -o "$resource_object"
  "$compiler" \
    -std=c11 -Os -Wall -Wextra -Wpedantic -Werror \
    -ffunction-sections -fdata-sections -fno-ident \
    -D_WIN32_WINNT=0x0A00 -municode -mwindows -static-libgcc \
    -Wl,--gc-sections -Wl,--no-insert-timestamp \
    -Wl,--dynamicbase -Wl,--high-entropy-va -Wl,--nxcompat \
    -s -o "$output" xnec2c-launcher.c "$resource_object" -luser32
)

echo "Native launcher created at: $output"
