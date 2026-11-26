# Cross-build environment for mupen64plus on RISC OS 5 (ARMv7, hard float).
# riscos-crossdev 1.11 (GCCSDK GCC 10.2, UnixLib) with riscos-mesa's 20.3.5-22 devkit in
# place of the one it carries: EGL/OSMesa with RISC3D, and libMesaGPU for a native Pi 4.
# Same recipe as riscos-quake3's risc3d branch.
#   docker build --platform linux/amd64 -t n64-riscos riscos/docker
# RISC OS port of Mupen64Plus. Copyright (C) 2026 David Sharp. GPL-2.0.
FROM ghcr.io/adyoull/riscos-crossdev:1.11
ARG MESA_DEVKIT=20.3.5-23
ARG MESA_DEVKIT_SHA256=fc8e190e4870bedf82c085bca3267f197117c07a4ab2c3f57def8a8404403609
RUN curl -fsSL -o /tmp/devkit.tgz \
      https://github.com/adyoull/riscos-mesa/releases/download/v$MESA_DEVKIT/riscos-mesa-devkit-$MESA_DEVKIT.tgz \
 && echo "$MESA_DEVKIT_SHA256  /tmp/devkit.tgz" | sha256sum -c - \
 && rm -rf /opt/riscos-crossdev/devkit/env \
 && mkdir /opt/riscos-crossdev/devkit/env \
 && tar xzf /tmp/devkit.tgz -C /opt/riscos-crossdev/devkit/env --strip-components=1 \
 && rm /tmp/devkit.tgz \
 && grep -q "$MESA_DEVKIT" /opt/riscos-crossdev/devkit/env/VERSION \
 && test -f /opt/riscos-crossdev/devkit/env/lib/libMesaGPU.a
WORKDIR /work
