#!/bin/sh
# A recoverable snapshot of the source under test (docs/TESTING.md, section 3).
#
# The snapshot holds what a result depends on and nothing that can be rebuilt or is not source: the tracked diff from
# HEAD, every untracked file the source and evidence keep (build directories, toolchains and the personal CrossOver
# installs are excluded), the dependency pins a build reads, and a record of the revision and submodule revisions the
# tree was at. It is a directory, one file per item, so a restore is a copy: a tracked change is `git apply`d and an
# untracked file is put back where it was.
#
# usage: tools/snapshot.sh [DESTINATION]
#   DESTINATION defaults to .agent-obj/snapshots/mission-<revision>-<date>
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
revision=$(git -C "$root" rev-parse HEAD)
branch=$(git -C "$root" rev-parse --abbrev-ref HEAD)
date=$(date +%Y%m%d-%H%M%S)
destination=${1:-"$root/.agent-obj/snapshots/mission-$revision-$date"}

mkdir -p "$destination"

# what the snapshot is of: the revision, the branch, submodules, and what the working tree changed
git -C "$root" rev-parse HEAD >"$destination/HEAD"
git -C "$root" rev-parse --abbrev-ref HEAD >"$destination/branch"
git -C "$root" submodule status --recursive >"$destination/submodules.txt"
git -C "$root" status --porcelain=v1 --untracked-files=all >"$destination/status.txt"
git -C "$root" diff HEAD --binary >"$destination/tracked.patch"
git -C "$root" diff --cached --binary >"$destination/staged.patch"

# every untracked file, relative to the root, that is not a build product, a toolchain or another product's install.
# the list is taken from git, so a file .gitignore hides is not silently dropped from the source identity.
# a snapshot does not contain earlier snapshots: they are recovery points, not source or evidence under test
git -C "$root" ls-files --others --exclude-standard -z -- . ':(exclude).agent-obj/snapshots' >"$destination/untracked.list"
if [ -s "$destination/untracked.list" ]; then tar -C "$root" --null --files-from="$destination/untracked.list" -czf "$destination/untracked.tgz"; else : >"$destination/untracked.tgz"; fi

# the pins a build reads: the toolchain in use, the compilers' releases, and the submodules' revisions
{
  echo "llvm-mingw: $(sed -n 's/.*llvm-mingw-\([0-9]*\)-ucrt.*/\1/p' "$root"/build-win64.txt "$root"/build-win32.txt 2>/dev/null | head -1)"
  echo "wine: $("$root"/toolchains/wine/bin/wine --version 2>/dev/null || echo absent)"
  grep -E '^(DXC|SDK) *=' "$root"/tests/compilers.py || true
  git -C "$root" submodule status --recursive
} >"$destination/pins.txt"

# every immutable attempt directory the runner made: a build tree ignores its own logs, so the manifests and per-test
# logs are taken here explicitly rather than being left behind when the tree is rebuilt
find "$root" -maxdepth 1 -type d -name 'build*' -exec sh -c 'test -d "$1/meson-logs/runs" && printf "%s\n" "$1/meson-logs/runs"' _ {} \; >"$destination/runner-runs.list" 2>/dev/null || true
if [ -s "$destination/runner-runs.list" ]; then
  tar -C "$root" -czf "$destination/runner-runs.tgz" --files-from="$destination/runner-runs.list" 2>/dev/null || true
else
  : >"$destination/runner-runs.tgz"
fi

# the evidence a result relies on, by hash and size: the logs and manifests under .agent-obj that exist now
if [ -d "$root/.agent-obj" ]; then
  find "$root/.agent-obj" -type f \( -name '*.log' -o -name 'manifest.json' \) -print0 |
    xargs -0 shasum -a 256 >"$destination/evidence.sha256" 2>/dev/null || true
fi

printf '%s\n' "$destination"
