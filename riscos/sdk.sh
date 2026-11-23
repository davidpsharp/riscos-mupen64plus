#!/bin/sh
# Runs a command in the cross-build image with the repository at /work.
#   docker build --platform linux/amd64 -t n64-riscos riscos/docker
# RISC OS port of Mupen64Plus. Copyright (C) 2026 David Sharp. GPL-2.0.
cd "$(dirname "$0")/.."
exec docker run --rm --platform linux/amd64 -v "$PWD":/work -w /work ${N64_IMAGE:-n64-riscos} "$@"
