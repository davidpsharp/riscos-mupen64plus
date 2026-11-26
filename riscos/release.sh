#!/bin/sh
# Makes the release zips on the Mac (after riscos/sdk.sh riscos/build.sh and
# riscos/sdk.sh riscos/package.sh). They're RISC OS zips: file types travel in each entry's ARC0
# extra field (tools/riscos_zip.py), so SparkFS, !SparkPlug or Info-ZIP's UnZip on RISC OS
# restore them.
#   build/Mupen64Plus-riscos-<version>.zip      !Mupen64Plus alone: the download, and what
#                                               website-davidsharp-com's packman/publish.py packages
#   build/Mupen64Plus-tests-<version>.zip       Tests/gl16, RunGL16, ReadMe: the GL probe from
#                                               docs/RISC3D-0.17-report.md (16 bpp desktops)
set -e
cd "$(dirname "$0")/.."
V=$(cat VERSION)
OUT=build/Mupen64Plus-riscos-$V.zip
TESTS=build/Mupen64Plus-tests-$V.zip
M=build/release-manifest.txt
MT=build/release-tests-manifest.txt
APP=build/riscos/app/!Mupen64Plus
test -f "$APP/Mupen64,ff8" || { echo "no $APP: run riscos/package.sh first" >&2; exit 1; }
test -f build/riscos/gl16,ff8 || { echo "no build/riscos/gl16,ff8: run riscos/build.sh first" >&2; exit 1; }

: > "$M"; : > "$MT"
# name,xxx -> "xxx  zip/name  source"; names with no suffix are data (ffd)
add() {   # add <source> <name in the zip> [manifest]
    case "$1" in
    *,???) t=${1##*,}; n=${2%,???} ;;
    *) t=ffd; n=$2 ;;
    esac
    printf '%s  %s  %s\n' "$t" "$n" "$1" >> "${3:-$M}"
}
find "$APP" -type f | sort | while read -r f; do
    add "$f" "!Mupen64Plus/${f#$APP/}"
done
add build/riscos/gl16,ff8 Tests/gl16,ff8 "$MT"
add riscos/release/RunGL16,feb Tests/RunGL16,feb "$MT"
add riscos/release/ReadMe,fff Tests/ReadMe,fff "$MT"
rm -f "$OUT" "$TESTS"
python3 -I tools/riscos_zip.py "$OUT" "$M"
python3 -I tools/riscos_zip.py "$TESTS" "$MT"
