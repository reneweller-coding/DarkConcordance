#!/usr/bin/env bash
# Builds DARK CONCORDANCE for Linux (Release, GCC 12) and packs dist/DarkConcordance-<version>-linux-x86_64.tar.gz:
# the program, its assets (without assets/raw, the textures' sources), the system's dav1d in lib/ (the program's
# rpath looks there first), the player's readme, the manual, the licence and the credits. Built on Ubuntu 22.04 it
# runs on glibc 2.35 and newer, SteamOS included.
#
#   bash Deploy/build_linux.sh [BUILD_DIR]      (default ~/build/spaceopera-release; nice -n 15, four jobs)
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${1:-$HOME/build/spaceopera-release}"
version="$(sed -n 's/^project( SpaceOpera VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"

# raylib's source from the Windows build where there is one (no download), Ninja where there is one.
extra=()
[ -d "$root/build/msvc/_deps/raylib-src" ] && extra+=(-DFETCHCONTENT_SOURCE_DIR_RAYLIB="$root/build/msvc/_deps/raylib-src")
command -v ninja > /dev/null && extra+=(-G Ninja)
CC="${CC:-gcc-12}" CXX="${CXX:-g++-12}" cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release "${extra[@]}"
nice -n 15 cmake --build "$build" --parallel 4

name="DarkConcordance-$version-linux-x86_64"
stage="$build/release/$name"
rm -rf "$build/release"
mkdir -p "$stage/lib" "$stage/assets"
cp "$build/SpaceOpera" "$stage/DarkConcordance"
(cd "$root/assets" && tar --exclude=./raw -cf - .) | (cd "$stage/assets" && tar -xf -)
dav1d="$(ldd "$build/SpaceOpera" | awk '/libdav1d/ { print $3 }')"
cp -L "$dav1d" "$stage/lib/$(basename "$dav1d")"
cp "$root/docs/README-player.txt" "$stage/README.txt"
cp "$root/LICENSE" "$root/docs/CREDITS.txt" "$root/res/darkconcordance.png" "$stage/"
[ -f "$root/docs/manual/DarkConcordance-Handbuch.pdf" ] && cp "$root/docs/manual/DarkConcordance-Handbuch.pdf" "$stage/Handbuch.pdf"
[ -f "$root/docs/manual/DarkConcordance-Manual.pdf" ] && cp "$root/docs/manual/DarkConcordance-Manual.pdf" "$stage/Manual.pdf"
mkdir -p "$root/dist"
tar -C "$build/release" -czf "$root/dist/$name.tar.gz" "$name"
echo "$root/dist/$name.tar.gz"
