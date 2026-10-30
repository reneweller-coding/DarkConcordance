#!/usr/bin/env bash
# Starts a Linux package once (WSL with WSLg, or Linux): unpacked into ~/package_test, muted, Velm for a
# few seconds. It must end by itself, find its own dav1d, draw its picture (not black) and miss no file of its own
# (raylib's "Failed to open"; the people's models ask for a few textures they never had, those are left out).
#
#   bash Deploy/test_package.sh dist/DarkConcordance-<version>-linux-x86_64.tar.gz
set -u
tgz="$1"
dir="$HOME/package_test"
rm -rf "$dir" && mkdir -p "$dir" && tar -C "$dir" -xzf "$tgz" || { echo "unpacking failed"; exit 1; }
cd "$dir"/DarkConcordance-*-linux-x86_64 || exit 1
if ldd ./DarkConcordance | grep -q "not found"; then ldd ./DarkConcordance | grep "not found"; exit 1; fi
timeout 300 nice -n 15 ./DarkConcordance --level velm.txt --hero corvin --autoplay --shot "$dir/test.png" --warm 8 \
    --mute > run.out 2>&1
code=$?
[ $code -eq 0 ] || { echo "it ended with $code"; tail -5 run.out; exit 1; }
[ -f "$dir/test.png" ] && [ "$(stat -c %s "$dir/test.png")" -gt 307200 ] || { echo "no picture (or a black one)"; exit 1; }
missing=$(grep "Failed to open" run.out spaceopera.log 2>/dev/null | grep -v quaternius | head -5)
[ -z "$missing" ] || { echo "$missing"; exit 1; }
echo "the Linux package starts: $dir/test.png"
