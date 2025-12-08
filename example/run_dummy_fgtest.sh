#!/bin/bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
fgtest_bin="$script_dir/../build/bin/fgtest"
target_bin="$script_dir/dummy"
input_file="$script_dir/dummy_input.bin"
output_dir="$script_dir/out"

if [[ ! -x "$fgtest_bin" ]]; then
  echo "error: fgtest binary not found at $fgtest_bin" >&2
  exit 1
fi

if [[ ! -x "$target_bin" ]]; then
  echo "error: dummy binary not found at $target_bin" >&2
  exit 1
fi

mkdir -p "$output_dir"

if [[ ! -f "$input_file" ]]; then
  printf '\x00' > "$input_file"
fi

export TAINT_OPTIONS="taint_file=stdin output_dir=$output_dir debug=1"

exec "$fgtest_bin" "$target_bin" "$input_file"
