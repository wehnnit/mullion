#!/bin/sh
# Fails when a generated fixed-function header was not produced from the current generator source. Regenerate with
# tools/regen-fixed-function-shaders.sh, which rebuilds the generator before running it.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

sha=$(shasum -a 256 tests/d3d9/fixed_function_shaders.cpp | awk '{print $1}')
stale=0
for file in "$root"/src/d3d9/*shaders.hpp; do
  if ! grep -qx "// generator-source-sha256: $sha" "$file"; then
    printf 'stale fixed-function shader header: %s\n' "${file#"$root/"}" >&2
    stale=1
  fi
done

if [ "$stale" -ne 0 ]; then
  printf 'run tools/regen-fixed-function-shaders.sh (it rebuilds the generator first)\n' >&2
  exit 1
fi
printf 'fixed-function shader headers are current (%s)\n' "$sha"
