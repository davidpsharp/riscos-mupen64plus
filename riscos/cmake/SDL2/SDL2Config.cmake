# The devkit's SDL 2.26, linked statically, with OpenGL through EGL and OSMesa (RISC3D
# where a driver is loaded), and libMesaGPU so a native Pi 4 draws on its GPU too.
# After riscos-quake3's (risc3d branch).
set(_devkit $ENV{RISCOS_DEVKIT})
set(SDL2_VERSION 2.26.0)
set(SDL2_INCLUDE_DIRS ${_devkit}/include/SDL2 ${_devkit}/include)
if(NOT TARGET SDL2::SDL2)
  add_library(SDL2::SDL2 STATIC IMPORTED)
  set_target_properties(SDL2::SDL2 PROPERTIES
    IMPORTED_LOCATION ${_devkit}/lib/libSDL2.a
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
    INTERFACE_COMPILE_DEFINITIONS _REENTRANT
    INTERFACE_LINK_LIBRARIES "${_devkit}/lib/libGLU.a;${_devkit}/lib/libEGL.a;${_devkit}/lib/libOSMesa.a;-Wl,-u,riscos_mesa_gpu;${_devkit}/lib/libMesaGPU.a;stdc++;${_devkit}/lib/libz.a;m;dl;pthread")
endif()
set(SDL2_FOUND TRUE)
