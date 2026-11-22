#!/bin/sh
# Cross-builds mupen64plus for RISC OS (inside the image: riscos/sdk.sh riscos/build.sh).
# Leaves build/riscos/mupen64plus (ELF, with symbols) and mupen64plus,ff8 (AIF).
set -e
cd "$(dirname "$0")/.."
cmake -S . -B ${BUILD_DIR:-build/riscos} -G Ninja -DCMAKE_TOOLCHAIN_FILE=riscos/toolchain.cmake \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE:-RelWithDebInfo} "$@"
ninja -C ${BUILD_DIR:-build/riscos}
