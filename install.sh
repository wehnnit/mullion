#!/bin/sh
# installs Mullion from a release archive: install.sh <wine> [prefix ...], or install.sh --crossover <bottle> [app]
# <wine> is the directory that holds bin/wine, an x86_64 or arm64 Wine. the libraries go where Wine keeps its own, in
# place of its Direct3D, and into each prefix named: a prefix made before them does not know the ones Wine did not have.
# a CrossOver bottle gets a root of its own instead, which its CX_ROOT names: links to the files of CrossOver.app (or
# app), with Mullion as the DXMT the bottle chooses. CrossOver.app stays as it is, so its updates keep this; to undo,
# put back the bottle's cxbottle.conf.before-mullion, and run this again after an update that adds files
set -e
here=$(cd "$(dirname "$0")" && pwd)
if [ "$1" = --crossover ]; then
  bottles="$HOME/Library/Application Support/CrossOver/Bottles"
  conf="$bottles/$2/cxbottle.conf"
  app=$3
  [ -n "$app" ] || for app in /Applications/CrossOver.app "$HOME/Applications/CrossOver.app"; do
    [ ! -d "$app" ] || break
  done
  cx="$app/Contents/SharedSupport/CrossOver"
  [ -n "$2" ] || { echo "usage: $0 --crossover <bottle> [CrossOver.app]" >&2; exit 1; }
  # a name with a space that was not quoted arrives as its first word: the list of bottles shows which was meant
  [ -f "$conf" ] || { echo "no bottle named \"$2\"; the bottles are:" >&2; ls "$bottles" >&2; exit 1; }
  [ -d "$cx/lib/dxmt" ] ||
    { echo "no CrossOver with DXMT${3:+ at $3}: name CrossOver.app after the bottle" >&2; exit 1; }
  root="$HOME/Library/Mullion/Bottles/$2"
  rm -rf "$root"
  mkdir -p "$root/lib/dxmt"
  for entry in "$cx"/* "$cx"/lib/*; do
    case ${entry#"$cx"/} in lib | lib/dxmt) ;; *) ln -s "$entry" "$root/${entry#"$cx"/}" ;; esac
  done
  cp -R "$here"/*-windows "$here"/*-unix "$root/lib/dxmt/"
  [ -f "$conf.before-mullion" ] || cp "$conf" "$conf.before-mullion"
  # the bottle's environment chooses DXMT and names the root, in place of what it said of either
  awk -v root="$root" '
    function ours() { print "\"CX_GRAPHICS_BACKEND\" = \"dxmt\""; print "\"CX_ROOT\" = \"" root "\""; set = 1 }
    /^"(CX_ROOT|CX_GRAPHICS_BACKEND)"/ { next }
    { print }
    $0 == "[EnvironmentVariables]" { ours() }
    END { if (!set) { print "[EnvironmentVariables]"; ours() } }
  ' "$conf" > "$conf.mullion"
  mv "$conf.mullion" "$conf"
  echo "Mullion is installed for the bottle $2; restart it if it runs"
  exit
fi
[ -d "$1/lib/wine" ] || { echo "usage: $0 <wine directory> [wine prefix ...]" >&2; exit 1; }
wine=$1
shift
# the architectures this Wine has: x86_64 for Wine on Rosetta, aarch64 (ARM64X, which its x86_64 programs load as
# ARM64EC) for arm64 Wine
native=
for kind in "$here"/*-windows "$here"/*-unix; do
  [ -d "$wine/lib/wine/$(basename "$kind")" ] || continue
  cp -R "$kind"/. "$wine/lib/wine/$(basename "$kind")/"
  case $(basename "$kind") in
    x86_64-windows) native=${native:-x86_64-windows} ;;
    aarch64-windows) native=aarch64-windows ;;
  esac
done
[ -n "$native" ] || { echo "$wine has neither an x86_64 nor an arm64 Wine in lib/wine" >&2; exit 1; }
# a prefix's system32 has the DLLs of Wine's own architecture, and its syswow64 the 32-bit ones a Wine that runs
# 32-bit programs took above
for prefix; do
  windows="$prefix/drive_c/windows"
  [ -d "$windows" ] || { echo "note: $prefix has no drive_c/windows; skipped" >&2; continue; }
  # system32 gets the Wine's own architecture; syswow64 gets i386 when the prefix is WoW64. the i386 libraries go
  # there only when this Wine has an i386 windows half of its own to load them with
  cp "$here/$native"/*.dll "$windows/system32/"
  if [ -d "$here/i386-windows" ] && [ -d "$windows/syswow64" ] && [ -d "$wine/lib/wine/i386-windows" ]; then
    cp "$here/i386-windows"/*.dll "$windows/syswow64/"
  elif [ -d "$here/i386-windows" ] && [ ! -d "$windows/syswow64" ]; then
    # a 32-bit-only prefix: i386 libraries belong in its system32
    if [ "$native" != "i386-windows" ] && [ "$native" != "x86-windows" ]; then
      echo "note: $prefix is 32-bit; installing i386-windows into system32" >&2
      cp "$here/i386-windows"/*.dll "$windows/system32/"
    fi
  fi
done
echo "Mullion is installed in $wine"
