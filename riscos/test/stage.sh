#!/bin/sh
# Builds work/stage/n64: what riscos/test/run-test runs, ready for `ro-run sync`.
set -e
cd "$(dirname "$0")/../.."
s=work/stage/n64
mkdir -p $s/data $s/roms $s/cfg $s/out $s/user
cp build/riscos/mupen64plus,ff8 $s/mupen64plus,ff8
cp mupen64plus/core/data/mupen64plus.ini mupen64plus/video-rice/data/RiceVideoLinux.ini $s/data/
for f in work/roms/*.N64 work/roms/*.z64 work/roms/*.v64 work/romzip/*; do [ -f "$f" ] && cp -f "$f" $s/roms/; done
cp 'riscos/test/run-test,feb' $s/
touch $s/cfg/.keep $s/out/.keep $s/user/.keep
echo "staged $s"
