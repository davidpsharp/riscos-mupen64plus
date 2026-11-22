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
Status: planning. The plan, phases and open questions are in `docs/PLAN.md`.

Rules (David, 2026-11-22):
- Never edit `~/code/rol-run` (RoMac). Write what RoMac needs in `docs/romac-requests.md` and hand
  it to the RoMac/rol-run agent with SendMessage; that agent decides.
- Rorth may go on a throwaway RoMac data folder, never David's own.
- The app opens in a desktop window by default; Rice video first, GLideN64 later.
