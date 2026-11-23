#!/bin/sh
# Makes the release zip, build/riscos-mupen64plus-<version>.zip, on the Mac (after
# riscos/sdk.sh riscos/build.sh and riscos/sdk.sh riscos/package.sh). It's a RISC OS zip: file
# types travel in each entry's ARC0 extra field (tools/riscos_zip.py), so SparkFS, !SparkPlug or
# Info-ZIP's UnZip on RISC OS restore them. Contents:
#   !Mupen64Plus            the app (build/riscos/app)
#   Tests/gl16, Tests/RunGL16, Tests/ReadMe
#                           the GL probe from docs/RISC3D-0.17-report.md (16 bpp desktops)
set -e
cd "$(dirname "$0")/.."
V=$(cat VERSION)
OUT=build/riscos-mupen64plus-$V.zip
M=build/release-manifest.txt
APP=build/riscos/app/!Mupen64Plus
test -f "$APP/Mupen64,ff8" || { echo "no $APP: run riscos/package.sh first" >&2; exit 1; }
test -f build/riscos/gl16,ff8 || { echo "no build/riscos/gl16,ff8: run riscos/build.sh first" >&2; exit 1; }

: > "$M"
# name,xxx -> "xxx  zip/name  source"; names with no suffix are data (ffd)
add() {   # add <source> <name in the zip>
    case "$1" in
    *,???) t=${1##*,}; n=${2%,???} ;;
    *) t=ffd; n=$2 ;;
    esac
    printf '%s  %s  %s\n' "$t" "$n" "$1" >> "$M"
}
find "$APP" -type f | sort | while read -r f; do
    add "$f" "!Mupen64Plus/${f#$APP/}"
done
add build/riscos/gl16,ff8 Tests/gl16,ff8
add riscos/release/RunGL16,feb Tests/RunGL16,feb
add riscos/release/ReadMe,fff Tests/ReadMe,fff
python3 -I tools/riscos_zip.py "$OUT" "$M"
