#!/bin/sh
# The only supported way to regenerate the fixed-function shader headers. The generator is rebuilt from its current
# source by ninja before it runs, so a shader edit can never be compiled into the library from stale headers. The
# script also stamps every generated header with the generator source's SHA-256; tools/check-fixed-function-shaders.sh
# fails if a header was produced from a different source.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

build=${1:-build64-2311}
prefix=${2:-toolchains/tests-prefix}
export PATH="$root/toolchains/llvm-mingw-20261104-ucrt-macos-universal/bin:$PATH"

# Build the generator from the current source first. This is deliberately one target, not a general compile that
# could compile the library against the headers being replaced.
ninja -C "$build" tests/d3d9/fixed_function_shaders.exe

WINEPREFIX="$root/$prefix" WINEDEBUG=-all \
  "$root/toolchains/wine/bin/wine" "$root/$build/tests/d3d9/fixed_function_shaders.exe" "$root/src/d3d9"

sha=$(shasum -a 256 tests/d3d9/fixed_function_shaders.cpp | awk '{print $1}')
for file in "$root"/src/d3d9/*shaders.hpp; do
  tmp=$(mktemp "${file}.XXXXXX")
  {
    printf '// generator-source-sha256: %s\n' "$sha"
    grep -v '^// generator-source-sha256:' "$file" || true
  } >"$tmp"
  mv "$tmp" "$file"
done

printf 'regenerated fixed-function shader headers from %s\n' "$sha"
