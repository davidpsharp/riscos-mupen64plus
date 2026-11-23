# riscos-mupen64plus

Mupen64Plus, the Nintendo 64 emulator, ported to RISC OS 5. It draws with OpenGL through
riscos-mesa and Andy Youll's RISC3D, so it uses the GPU where RISC3D has one.

- **Runs on:**
  - a native Raspberry Pi 4 (V3DRender);
  - RoMac (RISC OS on a Mac, RISC3D on the Mac's GPU);
  - RISC OS on Linux is planned.
- **Speed:** Super Mario 64 runs at well over full speed on a Pi 4's GPU (163-169 VI/s with the
  limiter off; full speed is 60).
- **The app:** `!Mupen64Plus` lives on the icon bar; drop a ROM on it to play in a desktop
  window.
  - Alt+Enter switches to full screen.
  - The icon bar menu saves and loads states and pauses.
  - Choices sets the window size, emulation core and sound.
  - The keyboard is the controller for now.

Status, plans and measurements: [docs/PLAN.md](docs/PLAN.md). How it behaves on RISC3D 0.17, for
Andy Youll: [docs/RISC3D-0.17-report.md](docs/RISC3D-0.17-report.md).

## Download

The [v0.1.0 release](https://github.com/davidpsharp/riscos-mupen64plus/releases/tag/v0.1.0) has
`riscos-mupen64plus-0.1.0.zip`, a RISC OS zip that keeps file types. Unpack it with SparkFS,
!SparkPlug or Info-ZIP's UnZip on RISC OS. It needs:
- !RISC3D 0.05 or later and !MesaLibs (riscos-mesa 20.3.5-22 or later), both in
  `!Boot.Resources`. On a Pi 4, !MesaLibs must be there: see the report, item 2.
- SharedUnixLibrary and ARMEABISupport (PackMan).
- For sound, SharedSoundBuffer and StreamManager.
- A 16M colour desktop: see the report, item 1.

No ROMs are included.

## Build

On a Mac (or Linux) with Docker:

```
docker build --platform linux/amd64 -t n64-riscos riscos/docker
riscos/sdk.sh riscos/build.sh       # build/riscos/mupen64plus,ff8 (+ ELF with symbols)
riscos/sdk.sh riscos/package.sh     # build/riscos/app/!Mupen64Plus
riscos/release.sh                   # build/riscos-mupen64plus-<version>.zip
```

The host build, for headless tests on the Mac, is
`cmake -S . -B build/mac -G Ninja && ninja -C build/mac`.

## Licence

GPL-2.0, as Mupen64Plus ([COPYING](COPYING)). The RISC OS port (everything outside
`mupen64plus/` and `third_party/`, and the `__riscos__` changes inside `mupen64plus/`) is
Copyright (C) 2026 David Sharp. The upstream sources are vendored at pinned commits, with their
own copyright notices; see [PROVENANCE.md](PROVENANCE.md).
