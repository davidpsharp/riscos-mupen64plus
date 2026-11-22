# riscos-n64: RISC OS project

Shared RISC OS knowledge is in `~/code/riscos-kb` (read, not imported, to keep context small):
read `INDEX.md` first; `app-conventions.md` before designing any app; `testing.md` before running
on RISC OS machines/emulators (tool: Rorth, see `~/code/RORTH/docs/using-rorth.md`); and check
`reuse-catalogue.md` and `projects.md` for prior art before writing something new. The `riscos`
skill does the same. Add anything reusable you learn as a new file in `~/code/riscos-kb/lessons/`.

## This project
Port mupen64plus to RISC OS 5 (ARMv7, one static binary) drawing through riscos-mesa's EGL and
Andy Youll's RISC3D. Targets, in order: RoMac (`~/code/rol-run`, RISC3D on the Mac's GPU),
RISC OS on Linux (on pi5-octagon, Pi 5 running Debian), native RISC OS on a Pi 4 (V3DRender; link `libMesaGPU`).
Status and progress: `docs/PLAN.md` (section 9). The plan, phases and open questions are there too.

Build and test:
- Mac (host tests): `cmake -S . -B build/mac -G Ninja && ninja -C build/mac`; run headless with
  `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` and `--gfx dummy` (never open windows on the Mac).
- RISC OS: `docker build --platform linux/amd64 -t n64-riscos riscos/docker` once, then
  `riscos/sdk.sh riscos/build.sh` -> `build/riscos/mupen64plus,ff8` (ELF with symbols beside it).
- Test hooks (`src/n64_check.c`, env/system variables): `N64_HASHLOG`, `N64_SHOT`, `N64_QUITVI`,
  `N64_NOLIMIT`. Compare `vi N rdram H fb H` lines between platforms.
- Test ROMs: PeterLemon/N64 (Unlicense) in `work/roms` (not in git).
- `ro-run` is `~/code/RORTH/tools/ro-run` (the `/usr/local/bin` one is broken).
- RoMac: `tools/romac-target.sh setup|start|stop` (ro-run target `romac-n64`, no window); then
  `riscos/test/stage.sh`, `ro-run -t romac-n64 sync work/stage/n64 n64`, and
  `ro-run -t romac-n64 cmd 'Obey n64.run-test CP1ADD.N64 tag 1 300 dummy'` (ROM names in Unix form).
  Desktop runs: `launch 'Obey IXFS:$.riscos.HardDisc4.Work.n64.run-test ...'` in a 16M-colour mode.
- Rorth's `crashdumps` must be off on RoMac with Rorth 0.36.1 or older (setup does it; fixed in 0.36.2,
  Rorth#28, then drop the "off"): with it on, every GCC 10 program dies
  with "EMT trap" at its first stack growth.

Rules (David, 2026-11-22):
- Never edit `~/code/rol-run` (RoMac). Write what RoMac needs in `docs/romac-requests.md` and hand
  it to the RoMac/rol-run agent with SendMessage; that agent decides.
- Rorth may go on a throwaway RoMac data folder, never David's own.
- The app opens in a desktop window by default; Rice video first, GLideN64 later.
