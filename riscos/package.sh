#!/bin/sh
# Makes build/riscos/app/!Mupen64Plus from the RISC OS build, in the n64-riscos image:
#   riscos/sdk.sh riscos/build.sh && riscos/sdk.sh riscos/package.sh
# !RunImage is the launcher (icon bar; drop a ROM on it); Mupen64 is the emulator, started by
# Game. The unstripped ELFs stay in build/riscos for ro-run dump --elf.
# RISC OS port of Mupen64Plus. Copyright (C) 2026 David Sharp. GPL-2.0.
set -e
cd /work
A=build/riscos/app/!Mupen64Plus
rm -rf "$A"
mkdir -p "$A/Data"
cp -r riscos/app/!Mupen64Plus/. "$A"/
arm-riscos-gnueabihf-strip -o build/riscos/mupen64plus.stripped build/riscos/mupen64plus
elf2aif -e build/riscos/mupen64plus.stripped "$A/Mupen64,ff8" >/dev/null
rm build/riscos/mupen64plus.stripped
cp build/riscos/launcher,ff8 "$A/!RunImage,ff8"
cp "$RISCOS_DEVKIT/riscos/PThrTicker,ffa" "$A/"
cp mupen64plus/core/data/mupen64plus.ini "$A/Data/mupen64plus.ini,fff"
cp mupen64plus/video-rice/data/RiceVideoLinux.ini "$A/Data/RiceVideoLinux.ini,fff"
cp mupen64plus/input-sdl/data/InputAutoCfg.ini "$A/Data/InputAutoCfg.ini,fff"
cp mupen64plus/core/doc/gpl-license "$A/Licence,fff"
# The libraries built into Mupen64: riscos-mesa's devkit (Mesa, SDL2, zlib, UnixLib, ...) and libpng
{ cat "$RISCOS_DEVKIT/LICENCES.txt"
  printf '\n===========================================================================\n\nlibpng\n\n'
  cat third_party/libpng/LICENSE; } > "$A/Licences,fff"
ls -l "$A"
