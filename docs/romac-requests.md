# Requests to RoMac (rol-run)

This project never edits `~/code/rol-run`. Each request goes here first, then to the RoMac/rol-run
agent by SendMessage. Write it in ARM, Tarmac and Linux terms, with measurements and a program or
test ROM that shows the need. The RoMac agent (and David) decide.

| # | Request | Why | Status |
|---|---|---|---|
| 1 | OpenGL ES 3.0 contexts in `romac-3d` | GLideN64's GLES 3 build (phase 6) | not sent: needed from phase 6 |
| 2 | (if measured) Tarmac behaviour under new_dynarec: cache thrash or `cacheflush` storms | dynarec speed on RoMac (phase 5) | not needed so far: the dynarec runs games above full speed |
| 3 | Mac game controllers as RISC OS joysticks (`Joystick_Read`), details below | David's Xbox One controller (USB-C or Bluetooth) for mupen64plus on RoMac | sent 2026-11-22 |

## 3. Mac game controllers as RISC OS joysticks

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
