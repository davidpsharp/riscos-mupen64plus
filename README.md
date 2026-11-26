# riscos-mupen64plus

Mupen64Plus, the Nintendo 64 emulator, ported to RISC OS 5. It draws with OpenGL through
riscos-mesa and Andy Youll's RISC3D, so it uses the GPU where RISC3D has one.

- **Runs on:**
  - a native Raspberry Pi 4 (V3DRender);
  - RoMac (RISC OS on a Mac, RISC3D on the Mac's GPU);
  - RISC OS on Linux is planned.
- **Speed:** Super Mario 64 runs at well over full speed on a Pi 4's GPU (about 165 VI/s with
  the limiter off; full speed is 60).
- **The app:** `!Mupen64Plus` lives on the icon bar; drop a ROM on it to play in a desktop
  window.
  - Alt+Enter switches to full screen.
  - The icon bar menu saves and loads states and pauses.
  - Choices sets the window size, emulation core and sound.
  - The keyboard is the controller for now.

Status, plans and measurements: [docs/PLAN.md](docs/PLAN.md). How it behaves on RISC3D 0.17, for
Andy Youll: [docs/RISC3D-0.17-report.md](docs/RISC3D-0.17-report.md).

## Download

Home page: <https://davidsharp.com/riscos/n64>. Questions and bug reports: raise an
[issue](https://github.com/davidpsharp/riscos-mupen64plus/issues) or use the
[contact page](https://davidsharp.com/contact/).

- **PackMan:** add the source `https://davidsharp.com/riscos/pkg`, then install Mupen64Plus
  from the Emulation section. PackMan installs SharedUnixLibrary and ARMEABISupport too, and
  tells you about updates.
- **Zip:** the [v0.2.0 release](https://github.com/davidpsharp/riscos-mupen64plus/releases/tag/riscos-v0.2.0)
  has `Mupen64Plus-riscos-0.2.0.zip`, a RISC OS zip that keeps file types. Unpack it with
  SparkFS, !SparkPlug or Info-ZIP's UnZip on RISC OS. `Mupen64Plus-tests-0.2.0.zip` holds the
  gl16 test program from the RISC3D report.

It needs:
- A Raspberry Pi 2 or later with RISC OS 5, RISC OS on Linux, or RoMac.
- !RISC3D in `!Boot.Resources`, from
  [RISC3D's releases](https://github.com/adyoull/riscos-risc3d/releases). On a Pi 4 use 0.22 or
  later; with older ones !MesaLibs must be in `!Boot.Resources` too (see the report, item 2).
  !MesaLibs isn't otherwise needed: the OpenGL libraries are built in.
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
riscos/release.sh                   # build/Mupen64Plus-riscos-<version>.zip (+ tests zip)
```

The host build, for headless tests on the Mac, is
`cmake -S . -B build/mac -G Ninja && ninja -C build/mac`.

## Licence

GPL-2.0, as Mupen64Plus ([COPYING](COPYING)). The RISC OS port (everything outside
`mupen64plus/` and `third_party/`, and the `__riscos__` changes inside `mupen64plus/`) is
Copyright (C) 2026 David Sharp. The upstream sources are vendored at pinned commits, with their
own copyright notices; see [PROVENANCE.md](PROVENANCE.md).
