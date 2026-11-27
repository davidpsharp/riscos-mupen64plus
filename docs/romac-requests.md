# Requests to RoMac (rol-run)

This project never edits `~/code/rol-run`. Each request goes here first, then to the RoMac/rol-run
agent by SendMessage. Write it in ARM, Tarmac and Linux terms, with measurements and a program or
test ROM that shows the need. The RoMac agent (and David) decide.

| # | Request | Why | Status |
|---|---|---|---|
| 1 | OpenGL ES 3.0 contexts in `romac-3d` | GLideN64's GLES 3 build (phase 6) | not sent: needed from phase 6 |
| 2 | (if measured) Tarmac behaviour under new_dynarec: cache thrash or `cacheflush` storms | dynarec speed on RoMac (phase 5) | not needed so far: the dynarec runs games above full speed |
| 3 | Mac game controllers as RISC OS joysticks (`Joystick_Read`), details below | David's Xbox One controller (USB-C or Bluetooth) for mupen64plus on RoMac | sent 2026-11-22; superseded 2026-11-27 (see below) |

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
