# Requests to RoMac (rol-run)

This project never edits `~/code/rol-run`. Each request goes here first, then to the RoMac/rol-run
agent by SendMessage. Write it in ARM, Tarmac and Linux terms, with measurements and a program or
test ROM that shows the need. The RoMac agent (and David) decide.

| # | Request | Why | Status |
|---|---|---|---|
| 1 | OpenGL ES 3.0 contexts in `romac-3d` | GLideN64's GLES 3 build (phase 6) | sent 2026-11-27 as a joint request with riscos-yabasanshiro (Saturn, ES 3.0), David agreed; write-up in `~/code/riscos-yabasanshiro/ports/saturn/romac-requests.md` |
| 2 | (if measured) Tarmac behaviour under new_dynarec: cache thrash or `cacheflush` storms | dynarec speed on RoMac (phase 5) | not needed so far: the dynarec runs games above full speed |
| 3 | Mac game controllers as RISC OS joysticks (`Joystick_Read`), details below | David's Xbox One controller (USB-C or Bluetooth) for mupen64plus on RoMac | sent 2026-11-22; superseded 2026-11-27 (see below) |
| 4 | A Pi-strict mode: alignment faults, code written without a flush keeps running stale, execute-never, details below | RoMac passed code that crashes on a Pi 4 (riscos-half-life's save games; maybe our Pi-only dynarec crash); David wants RoMac representative enough for efficient testing | written 2026-11-28, David agreed |

## 4. A Pi-strict mode

**Why (David, 28 Nov 2026):** "I'd like romac to be representative of real hardware so it can be
used for efficient testing". RoMac runs RISC OS on Tarmac's Cortex-A72 user-mode profile, which
passes some code that a Pi 4 doesn't:
- **Alignment.** RISC OS 5 on a Pi runs with SCTLR.A=1 (`OS_MMUControl 0` reads `&C53A3F`).
  `tarmac.h` says the user-mode profile has "the MMU off and alignment checking with it". Shown by
  riscos-half-life: `*(int *)` on packed save data worked on RoMac and crashed a Pi 4
  (`riscos-kb/lessons/2026-11-27-romac-does-not-trap-unaligned-access.md`).
- **Code written without a flush.** On a Pi, code must be followed by OS_SynchroniseCodeAreas
  (or `__clear_cache`) before it runs. Tarmac checks every store for translated code and
  retranslates, so a missing or short flush never shows on RoMac. Tarmac already has the strict
  form: `tarmac_set_code_writes_said(cpu, true)` ("a store into translated code the host doesn't
  report leaves that code running as it was"; the interpreters still check). rol-run already
  invalidates on `cacheflush`, which is what OS_SynchroniseCodeAreas becomes.
- **Execute-never.** Running from a page RISC OS 5.30 marks XN faults on a Pi; RoMac doesn't model
  XN. This is Tarmac request T4 (`TARMAC_EXIT_FLAG_NOT_EXEC`, reserved, not built).

**Request:** an opt-in mode (a RoMac setting or environment variable, off by default so normal use
stays fast) with as many of these as fit, in this order:
1. Alignment faults: honour `OS_MMUControl`'s A bit as a Pi does (on unless a program clears it),
   with an unaligned LDR/STR/LDRH/STRH ending in a data abort with `TARMAC_EXIT_FLAG_ALIGNMENT`,
   reported as RISC OS reports it on a Pi.
2. Stale code: `tarmac_set_code_writes_said(true)`, so guest code is retranslated only on
   `cacheflush`/OS_SynchroniseCodeAreas or a mapping change. Stricter than a Pi, whose caches
   sometimes hide a missing flush, which is what a test wants.
3. Execute-never (T4) for the areas RISC OS 5.30 on a Pi 4 maps XN.
4. If cheap: the Pi 4's memory layout for app space and dynamic areas, so a stray pointer lands
   where it would on a Pi.

**What it can't cover** (so riscos-n64 still tests on a Pi before a release): the Pi's GPU driver
(V3D against RISC3D on Metal), timing (RoMac is several times faster), and the kernel's own
privileged paths, for example ARMEABISupport's abort handler failing in SVC mode after a fault.

**Test case:** riscos-n64's dynarec crashes on a Pi 4 within seconds of Zelda's start
(`Obey n64.run-test Zelda.v64 zd 2 3000 dummy`) and runs on RoMac. On 28 Nov a build that
replaced `__clear_cache` with ranged OS_SynchroniseCodeAreas calls still crashed, so a short
flush is probably not its cause, but item 2 would settle it; if items 1-3 reproduce the crash on
RoMac it can be debugged there in minutes instead of a reboot per run on the Pi. riscos-half-life's
save game crash (a build without its alignment fix) is the case for item 1.

## 3. Mac game controllers as RISC OS joysticks

**Superseded, 27 Nov 2026.** RISC OS on Linux has had USB joypads since riscos-support 1.0.0085-1
(25 Nov): its own USBDriver module (USBLinux, over Linux's usbfs) runs RISC OS's USB drivers
unchanged, so Richard Walker's USBJoystick sees pads. David wants ROL reused for RoMac and RoWin,
so rol-run plans to give RoMac a /dev/bus/usb (usbfs) with each Mac controller as a synthetic USB
HID device (USB and Bluetooth pads; the Xbox One over USB isn't HID). Then USBJoystick 0.26 works
unchanged on a Pi, ROL and RoMac. Programs read pads through USBJoystick's slot SWIs; SDL ports
through riscos-quake3's SDL2 joystick driver (patch 10), not the in-app virtual joystick below.
The request below is kept for the record.

David wants to play mupen64plus on RoMac with his Xbox One controller (USB-C or Bluetooth, which
macOS's GameController framework handles). RISC OS has no path for it: the devkit's SDL2 has only
the dummy and virtual joystick drivers, and RoMac doesn't provide the Joystick module.

What riscos-n64 would use: the Joystick module's SWIs (Joystick_Read &43F40), so that any RISC OS
program can use the pad, not just this one. riscos-n64 will read them and present the pad to
mupen64plus's input plugin as an SDL virtual joystick.
- The classic form, `Joystick_Read` with R0 = stick number: R0 out = Y (up positive) | X << 8 |
  buttons << 16, signed bytes. Games such as OpenLara's port already read it. One stick and 8
  buttons isn't enough for an N64 pad (stick, C buttons, A, B, Z, Start, L, R, D-pad).
- So, if RoMac's agent agrees, also a richer form: both sticks and both triggers as 16-bit values,
  and all the buttons (A, B, X, Y, LB, RB, LT/RT as buttons too, Back/View, Start/Menu, both stick
  clicks, D-pad). RISC OS 5's own extension, if there is one, is better than anything new; if not,
  RoMac's agent picks the register layout, and riscos-n64 follows it.
- Several pads: stick numbers 0, 1, ...
