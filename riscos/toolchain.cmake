# CMake toolchain for mupen64plus on RISC OS 5 (ARMv7 with VFPv3/NEON: Pi 2 and later; tuned for
# the Pi 4): riscos-crossdev's toolchain file, plus the devkit's SDL2 description.
include($ENV{RCD_DIR}/cmake/riscos.cmake)
set(FPU neon)
set(CMAKE_C_FLAGS_INIT "-mfpu=neon -mfloat-abi=hard -mtune=cortex-a72 -fstack-clash-protection -mno-unaligned-access")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT}")
set(SDL2_DIR ${CMAKE_CURRENT_LIST_DIR}/cmake/SDL2 CACHE PATH "")
