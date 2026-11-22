# mupen64plus as one static program: core, ui-console, rsp-hle, audio-sdl, input-sdl and
# video-rice, linked together (src/static_dynlib.c stands in for dlopen).
#
# Mac (host tests):  cmake -S . -B build/mac -G Ninja && ninja -C build/mac
# RISC OS:           riscos/sdk.sh riscos/build.sh
cmake_minimum_required(VERSION 3.16)
project(mupen64plus-riscos C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 11)

set(M64P ${CMAKE_SOURCE_DIR}/mupen64plus)
set(GEN ${CMAKE_SOURCE_DIR}/src/gen)
option(N64_DYNAREC "new_dynarec (ARMv7, RISC OS builds only)" OFF)
option(N64_RICE "link the Rice video plugin (needs OpenGL)" ON)

if(RISCOS OR CMAKE_SYSTEM_NAME STREQUAL "RISCOS" OR CMAKE_C_COMPILER MATCHES "riscos")
  set(N64_RISCOS ON)
  set(N64_DYNAREC ON CACHE BOOL "" FORCE)   # ARM dynarec: about 4x the cached interpreter on RoMac
else()
  set(N64_RISCOS OFF)
endif()

# ---- dependencies
find_package(ZLIB REQUIRED)
if(N64_RISCOS)
  # the devkit has no libpng: build ours (third_party/libpng)
  set(PNG_SHARED OFF CACHE BOOL "" FORCE)
  set(PNG_STATIC ON CACHE BOOL "" FORCE)
  set(PNG_TESTS OFF CACHE BOOL "" FORCE)
  set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
  set(PNG_EXECUTABLES OFF CACHE BOOL "" FORCE)
  set(PNG_HARDWARE_OPTIMIZATIONS OFF CACHE BOOL "" FORCE)
  set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
  add_subdirectory(third_party/libpng EXCLUDE_FROM_ALL)
  add_library(PNG::PNG ALIAS png_static)
  target_include_directories(png_static PUBLIC ${CMAKE_SOURCE_DIR}/third_party/libpng
    ${CMAKE_BINARY_DIR}/third_party/libpng)
else()
  find_package(PNG REQUIRED)
endif()
if(N64_RISCOS)
  find_package(SDL2 REQUIRED CONFIG)
  set(SDL_TARGET SDL2::SDL2)
  set(GL_LIBS "")                     # EGL/OSMesa come with the devkit's SDL2
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  set(SDL_TARGET PkgConfig::SDL2)
  find_package(OpenGL REQUIRED)
  set(GL_LIBS OpenGL::GL)
endif()

set(COMMON_DEFS NDEBUG M64P_STATIC_PLUGINS)
if(N64_RISCOS)
  add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:SHELL:-include ${CMAKE_SOURCE_DIR}/src/riscos_compat.h>")
  set(RISCOS_SRC ${CMAKE_SOURCE_DIR}/src/riscos_compat.c ${CMAKE_SOURCE_DIR}/src/riscos_time.c)
endif()
if(APPLE)
  add_compile_options(-Wno-deprecated-declarations -DGL_SILENCE_DEPRECATION)
endif()

# ---- core
set(C ${M64P}/core/src)
set(CORE_SRC
  api/callbacks.c api/common.c api/config.c api/debugger.c api/frontend.c api/vidext.c
  backends/api/video_capture_backend.c backends/plugins_compat/audio_plugin_compat.c
  backends/plugins_compat/input_plugin_compat.c backends/clock_ctime_plus_delta.c
  backends/dummy_video_capture.c backends/file_storage.c
  device/cart/cart.c device/cart/af_rtc.c device/cart/cart_rom.c device/cart/eeprom.c
  device/cart/flashram.c device/cart/is_viewer.c device/cart/sram.c
  device/controllers/game_controller.c device/controllers/vru_controller.c
  device/controllers/paks/biopak.c device/controllers/paks/mempak.c
  device/controllers/paks/rumblepak.c device/controllers/paks/transferpak.c
  device/dd/dd_controller.c device/dd/disk.c device/device.c
  device/gb/gb_cart.c device/gb/mbc3_rtc.c device/gb/m64282fp.c
  device/memory/memory.c device/pif/bootrom_hle.c device/pif/cic.c
  device/pif/n64_cic_nus_6105.c device/pif/pif.c
  device/r4300/cached_interp.c device/r4300/cp0.c device/r4300/cp1.c device/r4300/cp2.c
  device/r4300/idec.c device/r4300/interrupt.c device/r4300/pure_interp.c
  device/r4300/r4300_core.c device/r4300/tlb.c
  device/rcp/ai/ai_controller.c device/rcp/mi/mi_controller.c device/rcp/pi/pi_controller.c
  device/rcp/rdp/fb.c device/rcp/rdp/rdp_core.c device/rcp/ri/ri_controller.c
  device/rcp/rsp/rsp_core.c device/rcp/si/si_controller.c device/rcp/vi/vi_controller.c
  device/rdram/rdram.c
  # main/workqueue.c only with M64P_PARALLEL (worker threads for savestates): left out
  main/main.c main/util.c main/cheat.c main/eventloop.c main/rom.c main/savestates.c
  main/screenshot.c main/sdl_key_converter.c
  plugin/plugin.c plugin/dummy_video.c plugin/dummy_audio.c plugin/dummy_input.c
  plugin/dummy_rsp.c)
if(APPLE)
  list(APPEND CORE_SRC osal/files_macos.c)
else()
  list(APPEND CORE_SRC osal/files_unix.c)
endif()
list(TRANSFORM CORE_SRC PREPEND ${C}/)
list(APPEND CORE_SRC
  ${M64P}/core/subprojects/md5/md5.c
  ${M64P}/core/subprojects/minizip/ioapi.c
  ${M64P}/core/subprojects/minizip/zip.c
  ${M64P}/core/subprojects/minizip/unzip.c)
list(APPEND CORE_SRC ${CMAKE_SOURCE_DIR}/src/n64_check.c)   # test hooks (VI)
set(CORE_DEFS NO_ASM)
if(N64_DYNAREC)
  # Ari64's new_dynarec, ARMv7 backend (RISC OS only). Its assembly needs struct offsets, which
  # are read from a compiled asm_defines.o (as the Makefile does, tools/gen_asm_script.sh)
  set(ND ${C}/device/r4300/new_dynarec)
  set(ASMDEF ${CMAKE_BINARY_DIR}/asm_defines)
  add_library(m64p_asmdefs OBJECT ${C}/asm_defines/asm_defines.c)
  target_include_directories(m64p_asmdefs PRIVATE ${C} ${C}/api)
  target_compile_definitions(m64p_asmdefs PRIVATE ${COMMON_DEFS} M64P_CORE_PROTOTYPES DYNAREC
    NEW_DYNAREC=3)
  target_compile_options(m64p_asmdefs PRIVATE -fno-lto)
  add_custom_command(OUTPUT ${ASMDEF}/asm_defines_gas.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ASMDEF}
    COMMAND bash ${M64P}/core/tools/gen_asm_script.sh ${ASMDEF} $<TARGET_OBJECTS:m64p_asmdefs>
    DEPENDS m64p_asmdefs $<TARGET_OBJECTS:m64p_asmdefs> COMMAND_EXPAND_LISTS)
  set_source_files_properties(${ND}/arm/linkage_arm.S PROPERTIES
    OBJECT_DEPENDS ${ASMDEF}/asm_defines_gas.h)
  list(APPEND CORE_SRC ${ND}/new_dynarec.c ${ND}/arm/linkage_arm.S ${ND}/arm/arm_cpu_features.c)
  set(CORE_DEFS DYNAREC NEW_DYNAREC=3)
endif()
add_library(m64p_core STATIC ${CORE_SRC})
target_include_directories(m64p_core PUBLIC ${C} ${C}/api
  PRIVATE ${M64P}/core/subprojects/md5 ${M64P}/core/subprojects/xxhash ${M64P}/core/subprojects)
if(N64_DYNAREC)
  target_include_directories(m64p_core PRIVATE ${ASMDEF})
endif()
target_compile_definitions(m64p_core PRIVATE ${COMMON_DEFS} M64P_CORE_PROTOTYPES NOCRYPT NOUNCRYPT
  IOAPI_NO_64 ${CORE_DEFS})
target_link_libraries(m64p_core PUBLIC ${SDL_TARGET} PNG::PNG ZLIB::ZLIB)

# ---- plugins: each compiled with its prefix header (tools/gen_static.py)
function(m64p_plugin name dir)
  set(srcs ${ARGN})
  list(TRANSFORM srcs PREPEND ${M64P}/${dir}/src/)
  add_library(${name} STATIC ${srcs})
  target_include_directories(${name} PRIVATE ${M64P}/${dir}/src ${C}/api)
  target_compile_definitions(${name} PRIVATE ${COMMON_DEFS} M64P_PLUGIN_PROTOTYPES)
  target_compile_options(${name} PRIVATE "SHELL:-include ${GEN}/prefix_${name}.h")
  target_link_libraries(${name} PRIVATE ${SDL_TARGET})
endfunction()

m64p_plugin(rsphle rsp-hle
  alist.c alist_audio.c alist_naudio.c alist_nead.c audio.c cicx105.c hle.c hvqm.c jpeg.c
  memory.c mp3.c musyx.c re2.c plugin.c)
m64p_plugin(audsdl audio-sdl
  circular_buffer.c main.c sdl_backend.c resamplers/resamplers.c resamplers/trivial.c)
m64p_plugin(inpsdl input-sdl plugin.c autoconfig.c config.c sdl_key_converter.c)
if(N64_RICE)
  m64p_plugin(rice video-rice
    liblinux/BMGImage.c liblinux/BMGUtils.c liblinux/bmp.c liblinux/pngrw.c
    Blender.cpp Combiner.cpp Config.cpp ConvertImage.cpp ConvertImage16.cpp Debugger.cpp
    DeviceBuilder.cpp FrameBuffer.cpp GraphicsContext.cpp OGLCombiner.cpp OGLExtensions.cpp
    OGLGraphicsContext.cpp OGLRender.cpp OGLRenderExt.cpp OGLTexture.cpp Render.cpp
    RenderBase.cpp RenderExt.cpp RenderTexture.cpp RSP_Parser.cpp RSP_S2DEX.cpp Texture.cpp
    TextureFilters.cpp TextureFilters_2xsai.cpp TextureFilters_hq2x.cpp
    TextureFilters_hq4x.cpp TextureManager.cpp VectorMath.cpp Video.cpp osal_files_unix.c)
  target_link_libraries(rice PRIVATE PNG::PNG ${GL_LIBS})
  target_compile_definitions(rice PRIVATE NO_ASM)
  if(N64_RISCOS)
    # GL 2.0 functions straight from OSMesa (its dispatch reaches RISC3D)
    target_compile_definitions(rice PRIVATE GL_GLEXT_PROTOTYPES)
  endif()
endif()

# ---- the program: ui-console + everything
set(U ${M64P}/ui-console/src)
add_executable(mupen64plus
  ${U}/cheat.c ${U}/compare_core.c ${U}/core_interface.c ${U}/debugger.c ${U}/main.c ${U}/plugin.c
  src/static_dynlib.c ${GEN}/static_tables.c ${RISCOS_SRC})
target_include_directories(mupen64plus PRIVATE ${U} src ${C}/api)
target_compile_definitions(mupen64plus PRIVATE ${COMMON_DEFS})
set_source_files_properties(${U}/cheat.c ${U}/compare_core.c ${U}/core_interface.c ${U}/debugger.c
  ${U}/main.c ${U}/plugin.c PROPERTIES COMPILE_FLAGS "-include ${GEN}/prefix_ui.h")
set(PLUGIN_LIBS rsphle audsdl inpsdl)
if(N64_RICE)
  list(APPEND PLUGIN_LIBS rice)
else()
  target_compile_definitions(mupen64plus PRIVATE N64_NO_RICE)
endif()
target_link_libraries(mupen64plus PRIVATE m64p_core ${PLUGIN_LIBS} ${SDL_TARGET} ${GL_LIBS} m)
if(APPLE)
  target_link_libraries(mupen64plus PRIVATE "-framework CoreFoundation")
endif()
if(N64_RISCOS)
  include(RiscOSBuild)
  riscos_checks(mupen64plus)
  riscos_aif(mupen64plus ${CMAKE_BINARY_DIR}/mupen64plus,ff8)
  set_target_properties(mupen64plus-aif PROPERTIES EXCLUDE_FROM_ALL FALSE)
endif()

# ---- probes (RISC OS): small programs that measure the platform
if(N64_RISCOS)
  add_executable(delayprobe riscos/probe/delay.c)
  target_link_libraries(delayprobe PRIVATE ${SDL_TARGET})
  riscos_aif(delayprobe ${CMAKE_BINARY_DIR}/delayprobe,ff8)
endif()
