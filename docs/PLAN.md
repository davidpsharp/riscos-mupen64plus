# riscos-n64: plan for porting mupen64plus to RISC OS on RISC3D

Written 22 Nov 2026 (decisions in section 8) from: the riscos-kb, RoMac's `docs/risc3d.md`, Andy Youll's RISC3D
(`rol-run/build-r3dkit`, release 0.10/0.11), the riscos-quake3 / riscos-pcem / riscos-mgba ports,
and the mupen64plus sources (core `daae68e`, 10 Nov 2026; GLideN64 master).

## 1. Goal and targets

One build of mupen64plus (one ARMv7 hard-float `!RunImage`) that draws through riscos-mesa's
EGL and RISC3D, and runs on:

| # | Platform | GPU route | CPU | Status of the route |
|---|---|---|---|---|
| 1 | **RoMac** (RISC OS on Linux's image on macOS, Tarmac) | RISC3D -> Linux3D -> `romac-3d` -> Mac OpenGL | ARM32 translated by Tarmac to AArch64 | works: TORCS 125 fps, ClassiCube 62 fps. Contexts: GL 2.1 compat, GL 3.x via 4.1 core; **no GLES 3** |
| 2 | **RISC OS on Linux** on **pi5-octagon** (Pi 5, Debian 13) | RISC3D -> Linux3D -> `rol-3d` -> Linux Mesa | native AArch32 | works on Pi 4 and the Dev Kit; **Pi 5 (V3D 7.1) not yet tested by Andy**: Linux's Mesa drives it, so it should work. We would be the first to try it |
| 3 | **Native RISC OS 5.30, Pi 4** | RISC3D -> V3DRender (program must link `libMesaGPU`) | Cortex-A72, one core | works: OpenLara 50 fps, ClassiCube 31 fps |

The same binary serves all three: RISC3D picks the driver, the program never names one.
Native Pi 4 only uses the GPU if the program is linked with `-Wl,-u,riscos_mesa_gpu -lMesaGPU`
(static build, ~15 MB more), so we always link it.

## 2. What mupen64plus is, and what each part needs

mupen64plus is a core library plus plugins loaded with `dlopen`, driven by a front end.

| Part | Choice | What it needs on RISC OS |
|---|---|---|
| Front end | `mupen64plus-ui-console` | loads core + plugins by `dlopen` -> replaced by a static table (see 3.1); later a desktop wrapper |
| Core | `mupen64plus-core` | SDL2 for its video extension (GL context, window) ; zlib, libpng; optional minizip; CPU modes: pure interpreter, **cached interpreter** (portable C), **new_dynarec** (has an ARMv7 back end, `new_dynarec/arm/`) |
| RSP | `mupen64plus-rsp-hle` | plain C; nothing special |
| Audio | `mupen64plus-audio-sdl` | SDL2 audio (SharedSound under SDL's RISC OS driver) |
| Input | `mupen64plus-input-sdl` | SDL2 keyboard/joystick |
| Video, phase 1 | **`mupen64plus-video-rice`** | OpenGL 1.x/2.x fixed function, or GLES 2 (`USE_GLES=1`). Fits every RISC3D route, including RoMac's GL 2.1 |
| Video, phase 2 | **GLideN64** (C++17) | desktop build asks for **GL 3.3 core**; GLES builds ask for ES 2/3/3.1. RISC3D tops out at **GL 3.1 / ES 3.1**, and RoMac has no ES 3. So: GLES 3.x build on Pi 4 and ROL; RoMac needs ES 3 contexts added to `romac-3d` first (see 3.6) |
| Video, not planned | angrylion (software LLE, far too slow), parallel-rdp (Vulkan: no route on RISC OS), glide64mk2 (no gain over Rice + GLideN64) | |

Facts checked in the source:
- new_dynarec's 32 MB translation cache is a static, page-aligned array inside `g_dev`
  (`r4300_core.h:184`), made executable with `mprotect` and flushed with `__clear_cache`. On
  RISC OS application memory is already executable and `__clear_cache` maps to
  `OS_SynchroniseCodeAreas`, so the dynarec's memory model fits RISC OS; `mprotect` becomes a no-op.
  The only `mmap(MAP_FIXED)` is the 64-bit `ROM_COPY` hack, unused on 32-bit.
- The Makefile picks ARM + new_dynarec from `uname -m` (`armv7*`); we set `HOST_CPU=armv7l`
  explicitly, or drive it from our own CMake.
- ioquake3's ARM JIT already runs on RISC OS (riscos-quake3), and RoMac invalidates Tarmac's
  translations on the `cacheflush` syscall that `OS_SynchroniseCodeAreas` makes under ROL. So a
  JIT inside RISC OS inside RoMac is expected to *work*; how fast is an open question (5.4).

## 3. Work streams

### 3.1 One static binary (no dlopen)
The KB rule is static builds (`-static`, no LTO). Every plugin exports the same names
(`PluginStartup`, `PluginGetVersion`, `RomOpen`, ...) and some share global helper names, so:
- build each plugin as a partial link (`ld -r`), keep only its API symbols global
  (`objcopy --keep-global-symbols`), and prefix them (`--prefix-symbols=rice_` or `--redefine-syms`);
- add `osal_dynamiclib_static.c` to core and ui-console: `osal_dynlib_open("rice")` returns a
  handle to a table, `osal_dynlib_getproc` looks the name up in it;
- ui-console links the core directly (it already can, via its function pointer table).
Fallback if this gets ugly: GCCSDK shared libraries (`ld-riscos/so/2` in !SharedLibs). Not preferred.

### 3.2 OS layer
- `osal/files_riscos.c`: config and save folders in `<Choices$Write>.Mupen64Plus`, shared data
  (`mupen64plus.ini`, fonts) in `<Mupen64Plus$Dir>`, ROMs from anywhere (UnixLib path names).
- `fenv` and FPU rounding: the core's FPU emulation sets host rounding modes. Check UnixLib's
  `fesetround` on VFP, and the flush-to-zero lesson (`lessons/2026-11-13-vfp-flush-to-zero...`).
- Threads: keep emulation on the main thread; ship `PThrTicker` for SDL audio.
- Memory: 32 MB dynarec cache + 8 MB RDRAM + ROM (up to 64 MB) + textures. Heap in a dynamic
  area (`__dynamic_da_name`, 128 MB cap per DA); the static dynarec buffer is in the app slot.
- Alignment: build `-mno-unaligned-access`; run the QEMU armtest (riscos-mgba `docker/armtest`)
  that traps unaligned access like RISC OS.

### 3.3 Video through RISC3D
- The core's video extension opens an SDL2 GL window. Under SDL's RISC OS driver that goes to
  EGL and so to RISC3D (`SDL_RISCOS_GL_EGL` default). To check early: that the driver honours
  `SDL_GL_CONTEXT_PROFILE_ES` and version attributes, double buffering, swap interval, and window
  resize/full screen. This is the single biggest unknown; it is spike S1.
- Link order: `-lEGL ... -lOSMesa -Wl,-u,riscos_mesa_gpu -lMesaGPU`; riscos-mesa >= 20.3.5-22.
  If the crossdev devkit's Mesa is older, build riscos-mesa from source as riscos-quake3 does.
- Rice first (GLES 2 build and GL build; pick whichever runs on all three; GL 2.1 is the common
  ground with RoMac).

### 3.4 Desktop app `!Mupen64Plus`
Follow `app-conventions.md` section A unless David decides otherwise (questions in 7):
icon bar icon (Info/Help/Quit, from the patched SDL2 in riscos-pcem), drag a ROM (`.z64 .n64
.v64`) onto it or the window, game in a desktop window by default with a full-screen toggle,
settings in Choices, Info box with version and build date, `!Help` with requirements.
Per-frame Wimp polling comes from SDL's event pump, which the core calls every VI.

### 3.5 Speed
- CPU mode order: cached interpreter first (portable, debuggable), then new_dynarec ARMv7.
- Pi 4 native: dynarec is the big win; RISC OS uses one core, which suits mupen64plus (mostly
  single threaded; leave GLideN64's threaded GL off).
- Profile with `arcelite-port`'s `OS_CallEvery` sampler (`ro_prof.c` + `prof.py`).
- Frame skip and VI-rate limiter settings from the start, so slow targets still play sound right.

### 3.6 RoMac work (in `~/code/rol-run`: requests only, never edited from here)
This project does not change RoMac. Each need is written up in `docs/romac-requests.md` (in ARM,
Tarmac and Linux terms, with measurements and a test ROM or program that shows it) and handed
to the RoMac/rol-run agent with SendMessage. That agent decides how and when.
- Expected request 1, needed only for GLideN64: ES 3.0 contexts in `romac-3d` (GLSL ES 3.00 -> GLSL 4.10 core is
  mostly a header rewrite; the GL 3 path already runs on 4.1 core). Coordinate with Andy's
  planned split of platform code out of rol-3d.
- If the dynarec is slow under Tarmac (5.4), the likely causes are known from OpenTS: code cache
  thrash and invalidation storms from small `cacheflush` calls. That would be request 2, with
  Runway's own "tarmac per s" and "slow paths" log lines as evidence.

## 4. Phases and exit criteria

| Phase | Work | Done when |
|---|---|---|
| **0. Set-up** (small) | Vendor mupen64plus repos + GLideN64 at pinned commits (`PROVENANCE.md`); copy `sdk.sh`/`build.sh`/`toolchain.cmake` from riscos-quake3/pcem; CMake build of core + plugins | Mac build of the tree from our CMake |
| **1. Static binary on the Mac** | 3.1 shim; Mac SDL2 build with dummy video/audio; add `M64P_SYNCLOG` (hash of RDRAM + VI count every N frames) | one Mac binary boots a test ROM headless and writes a hash log; interpreter and cached interpreter agree |
| **2. RISC OS, no graphics** | Cross-build; dummy video; cached interpreter; 3.2 OS layer | test ROMs (CPU tests) pass on `pi4-plato` and RoMac, hash logs match the Mac; VIs/s measured |
| **S1 spike (can run during 1-2)** | Tiny SDL2 program: open GL 2.1, GLES 2 and GLES 3 contexts through the core's vidext path; draw; swap | know which contexts each of the three targets gives, through SDL's driver |
| **3. Pictures with Rice** | Rice through RISC3D | a homebrew ROM draws correctly on RoMac, ROL and Pi 4 (screenshots via Rorth/RUNWAY_SHOTDIR) |
| **4. Playable app** | audio-sdl, input-sdl (keyboard map + pads), `!Mupen64Plus` | a game plays with sound from the icon bar on all three |
| **5. Speed** | new_dynarec on Pi 4 native, then ROL, then RoMac; profiling; frame skip | dynarec hash logs match the interpreter; fps table per target and per CPU mode |
| **6. GLideN64** | GLES 3 build on Pi 4/ROL; ES 3 in romac-3d (3.6) | GLideN64 selectable; Rice stays the fallback |
| **7. Release** | VERSION, package, zip, `!Help`, GPL source offer | David's go-ahead |

## 5. Testing

1. **Mac first** for everything platform-neutral (shim, OS layer, hash logs), with
   `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy nice -n 19`; never open game windows on David's Mac.
2. **ARM32 Linux in QEMU** (riscos-mgba armtest) for the ARMv7 dynarec and alignment traps
   before touching a RISC OS machine.
3. **Pi 4 native** through Rorth: `pi4-plato` is up (and `pi4-socrates` when it answers).
4. **RoMac**: Rorth installed on a throwaway RoMac data folder (approved), registered as a
   Rorth target, with the risc3d player and !RISC3D 0.05+, !MesaLibs 20.3.5-22+, !SharedLibs.
   `runway-headless` with `RUNWAY_EVENTS`/`RUNWAY_SHOTDIR` for scripted runs.
5. **RISC OS on Linux: pi5-octagon** (Debian 13, kernel 6.18.50 rpi, Mesa 26.2.6, render node
   present; reached over ssh). Checked 22 Nov 2026: **16 KB-page kernel, ROL not installed.**
   ROL native needs a 4 KB-page kernel (`kernel=kernel8.img`; see
   `riscos-kb/lessons/2026-11-09-riscos-on-linux-native-vs-qemu.md`). The KVM route keeps the
   16 KB kernel but has no GPU in the VM (llvmpipe only), so it is no good for RISC3D. Plan:
   install ROL (riscos-support + riscos-risc3d) and switch the Pi to `kernel8.img`, **with
   David's OK** (question 6); then put Rorth on the ROL disc. For smoke tests before that, ROL in
   Docker with `RISC3D$Force` exercises Linux3D's command stream on llvmpipe.
6. **Determinism**: `M64P_SYNCLOG` hashes compared Mac vs Pi vs RoMac, and interpreter vs dynarec,
   find CPU-emulation bugs without looking at pictures.
7. **ROMs**: public test ROMs and homebrew (e.g. n64-systemtest, krom's CPU/RDP tests) in CI-style
   runs; David's own cartridge dumps for game testing, kept out of git.

### Speed expectations (guesses until phase 5 measures them)

| Target | Cached interpreter | new_dynarec |
|---|---|---|
| Pi 4 native | perhaps 10-25% of full speed | lighter games near full speed (Pi 4 32-bit Linux builds manage this) |
| ROL on pi5-octagon | faster than Pi 4 native (A76 at 2.4 GHz) | likely full speed for most games |
| RoMac (M-series) | depends on Tarmac: possibly 30-60% | unknown: double JIT; may be faster or may thrash Tarmac's cache |

## 6. Risks

| Risk | Effect | Mitigation |
|---|---|---|
| SDL's RISC OS driver doesn't pass context version/profile to EGL | no GLES 3, maybe wrong context | spike S1 early; patch SDL (we already carry SDL patches) |
| GLideN64 wants GL 3.3 core, RISC3D gives at most 3.1 | desktop-GL GLideN64 can't run | GLES 3 build; Rice meanwhile |
| RoMac has no GLES 3 | no GLideN64 on the Mac | 3.6 in romac-3d |
| Dynarec under Tarmac slow or wrong | RoMac stuck on interpreter speed | measure; Tarmac requests; cached interpreter default on RoMac if needed |
| Plugin symbol clashes in one static binary | link errors, wrong functions called | partial link + localise per plugin (3.1) |
| UnixLib gaps (fenv, `mprotect`, `getpwuid`, XDG paths) | build or subtle FPU errors | OS layer; hash logs vs Mac |
| Licences | mupen64plus and GLideN64 are GPLv2, RISC3D and riscos-mesa MIT, V3DRender GPL-2.0+ | GPL binary: ship source with releases; no ROMs ever |

## 7. Open questions for David

6. **pi5-octagon**: OK to install RISC OS on Linux (riscos-support, riscos-risc3d) and switch it
   to the 4 KB-page kernel (`kernel=kernel8.img` in `config.txt`)? It's the only way to give ROL
   the GPU there. Rorth-on-ROL comes after. (Runway's own Pi 5 tests ran on both kernels.)

## 8. Decisions (David, 22 Nov 2026)

1. **Desktop window by default**, with a full-screen toggle.
2. **Rice first**, GLideN64 later.
3. **pi5-octagon** (Pi 5, Linux) is the RISC OS on Linux machine.
4. **Rorth on a throwaway RoMac disc**: yes.
5. **RoMac changes are requests** handed to the RoMac/rol-run agent (3.6); this project never
   edits `~/code/rol-run`.

## 9. Progress

**22 Nov 2026: phases 0-2 done for RoMac; phase 3 started.**
- One static program (core, ui-console, rsp-hle, audio-sdl, input-sdl, Rice) builds for the Mac and
  for RISC OS (riscos-crossdev 1.11, riscos-mesa devkit 20.3.5-22, libMesaGPU linked; crossdev's
  checks pass: static, stack probes, VFPv3, UnixLib).
- On a throwaway RoMac (0.1.220 risc3d build, RISC3D 0.05, Linux3D on the GPU), six PeterLemon test
  ROMs (CP1 ADD/CVT/ROUND/SQRT, COP0 registers, Mandelbrot) give **the same RDRAM and framebuffer
  hashes as the Mac** at VI 300, cached interpreter; the CP1 tests show PASS.
- Speed on RoMac (M3 Pro), cached interpreter, these simple ROMs: about 30 VI/s with dummy video,
  26.5 with Rice's window open (full speed is 60).
- Rice opens a 640x480 desktop window through SDL and EGL: `Using OpenGL: Apple M3 Pro (RISC3D,
  Linux GPU) - 2.1 RISC3D (2.1 Metal)`. Still to see: a picture drawn by Rice, which needs a ROM with
  ordinary display lists (the test ROMs draw with the CPU, which Rice doesn't show).

**22 Nov 2026 (later): games draw on RoMac through RISC3D.** Super Mario 64, Zelda: Ocarina of
Time and GoldenEye 007 (David's own dumps, kept in `work/`, never in git) run in a desktop window with
Rice on the M3 Pro's GPU, through their intros (screenshots in `work/*-r3d*.png`, `zelda-*.png`,
`ge-*.png`).
- Fix needed: riscos-mesa's EGL gives a single-buffered context (`GL_DOUBLEBUFFER` 0), so Rice drew
  to the front buffer while RISC3D shows the back buffer at each swap: a black window, though
  `glReadBuffer(GL_FRONT)` readback had the picture and software Mesa showed it. Rice now calls
  `glDrawBuffer(GL_BACK)` on RISC OS (falling back to GL_FRONT if refused).
- Saves and caches: test runs set `XDG_DATA_HOME`/`XDG_CACHE_HOME` to `user` (no more `$HOME` error).
- Speed (cached interpreter, average from boot): Mario 600 VIs in 88 s (about 7 VI/s, slower in 3D
  scenes); Zelda 1080 in 118 s (9); GoldenEye 1500 in 89 s (17). Full speed is 60: the dynarec next.

**22 Nov 2026 (speed): the ARMv7 dynarec runs on RoMac, and games run above full speed.**
- new_dynarec (ARM backend) builds for RISC OS and is now the default there (`N64_DYNAREC`, forced
  on). Its cache is the static 32 MB buffer in `g_dev`; `__clear_cache` is OS_SynchroniseCodeAreas;
  `/proc/cpuinfo` is missing, so it divides in software. Same RDRAM hashes as the interpreter on all
  six test ROMs.
- Limiter off, Rice through RISC3D, with the Mac otherwise quiet: Mario about 130-160 VI/s, Zelda
  about 100, GoldenEye about 84 on average (39 in its heaviest intro scene). Full speed is 60.
- With the limiter on, Mario holds 55-60, with dips to the mid-40s after long frames (stalls of
  80 ms or more, which the core's limiter doesn't make up).
- On RISC OS, `SDL_GetTicks` and UnixLib's clocks count centiseconds, and `SDL_Delay` rounds up to
  the next one, sometimes two. `src/riscos_time.c` gives the limiter a microsecond clock from HAL
  counter 0 where there is one. RoMac's HAL has no counter, so there it falls back to `SDL_Delay`.
- Profile (`N64_PROFILE`), Mario 1800 VIs: the RSP (HLE plus Rice's display lists and GL calls)
  52%, CPU emulation 43%, the SDL event pump 4-6%. Skipping Rice's triangle draws saves only 10%,
  so the GL path isn't the bottleneck.
- Timings on the Mac vary widely while other emulators run (other projects' RoMacs, rpcemu, a VM).
  Compare only runs made close together.

**22 Nov 2026 (sound):** audio-sdl plays through SDL's RISC OS driver (SharedSoundBuffer and
StreamManager, John Duffell's freeware, which the program needs loaded: `RMEnsure` them; David's
RoMac disc has them in `!System.350.Modules`, and the throwaway disc now gets copies).
- The device takes 43,800-43,900 sample frames/s at 44.1 kHz; audio-sdl's sync then paces the game.
  Sound costs about 8% of the speed. Underruns ("crackles") come when the game falls below full
  speed: on RoMac, long frames of 80-130 ms argue for a larger PRIMARY_BUFFER_TARGET in the app.
- Trap: `SDL_AUDIODRIVER` is a system variable, so a `dummy` set once (the old test script) stays
  set for every later program, and the dummy driver's sleep-based pacing looked like a slow device
  (27,900/s). Not yet heard by a person: David to listen.

**22 Nov 2026 (pi4-plato, native RISC OS 5.30, first try):**
- The same binary runs. Interpreter and dynarec give the same RDRAM hashes as the Mac and RoMac on
  the test ROMs. Mario with dummy video does 1200 VIs in 1.6 s (about 740 VI/s) on the dynarec.
- Through the GPU (V3DRender 0.16, "V3D 4.2.14.0 - 2.1 Mesa 26.1.6") it hits the bug riscos-quake3
  and riscos-doom3 hit: "MESA: error: Failed to allocate device memory for BO", then a segfault
  (dump D3208). Every GPU buffer of 64 KB or more is refused on 5.30 (riscos-doom3
  `docs/risc3d-issues.md`, bug 2, for Andy).
- With `LIBGL_ALWAYS_SOFTWARE 1` Rice draws Mario in a desktop window at about 12-15 VI/s; 93% of
  the time is the RSP plus Rice plus software Mesa.
- No SharedSoundBuffer or StreamManager on the Pi: it runs silent.
- Once, a run straight after the GPU crash died at start with "EMT trap, recursion in the abort
  handler of ARMEABISupport"; the next run was fine.

**22 Nov 2026 (pi4-plato on the GPU, the riscos-doom3 fix):** the 64 KB+ BO failures are
RISC OS 5.30's 128 MB cap on a dynamic area: V3DRender's 256 MB buffer area is made at boot
under it (riscos-doom3 `docs/risc3d-issues.md` item 2; KB lesson
2026-11-22-da-cap-and-specific-page-claims). With DAClamp run, then RISC3D and the stock
V3DRender 0.16 reloaded, Rice draws Mario on the GPU ("V3D 4.2.14.0 - 2.1 Mesa 26.1.6"):
154 VI/s unthrottled (software Mesa: 12-15), 0 BO errors, 0 resets, 0 MMU errors. Lasts until
the Pi restarts. The real fix is Andy's (V3DRender making its area at a size the OS allows, or
!RISC3D lifting the cap before loading it); riscos-doom3 has reported it.

**23 Nov 2026 (pi4-plato: no patches, no reload, just !MesaLibs at boot):** with riscos-mesa's
!MesaLibs (devkit 20.3.5-22) in `!Boot.Resources`, a clean restart gives Mario on the GPU at
154 VI/s, 0 BO errors, with the stock !RISC3D 0.05 / V3DRender 0.16. !MesaLibs' `!Boot` runs
DAClamp, and Resources are booted in name order, so !MesaLibs lifts the 128 MB cap before
!RISC3D loads V3DRender and makes its 256 MB area. Control: the same restart with !MesaLibs moved
out: "Failed to allocate device memory for BO", then a crash (D3219). doom3's other two
V3DRender patches (16k handles, no specific-page claims) aren't needed for Mario. plato is left
with !MesaLibs installed. (A clean restart doesn't load ARMEABISupport; run-test now does.)

**26 Nov 2026 (0.2.0: riscos-mesa 20.3.5-23, RISC3D 0.22):** devkit -23 (plain rebuild). RoMac:
Mario (fb hash at VI 120 unchanged) and megatextures through the app; Info shows 0.2.0. Pi 4
(socrates) with only !RISC3D 0.22 in Resources, no !MesaLibs: GPU, 164.5 VI/s, 256 MB buffer
area, no errors, so the boot-order dependency is gone. 64K colour windows still garbled (-24).
App ships `Licences` (devkit's LICENCES.txt + libpng). Release zips renamed for PackMan:
`Mupen64Plus-riscos-<v>.zip` (app only) and `Mupen64Plus-tests-<v>.zip`; tag `riscos-v<v>`.

**23 Nov 2026 (RISC3D 0.17 / V3DRender 0.26, pi4-socrates):** Mario on the GPU at 163-169 VI/s,
0 BO errors, 0 resets. The 128 MB cap is unchanged: without !MesaLibs in Resources, the same BO
errors and crash as 0.16. New: **a 64K colour desktop garbles every GL window** (GPU, software
Mesa and RoMac; 0.16 too): the frame's 32-bit pixels land unconverted as pairs of 16-bit pixels,
the left half at double width. Shown with `riscos/probe/gl16.c`; it's riscos-mesa's SDL/EGL
window code. Workaround: 16M colours (in !Help). Write-up for Andy: docs/RISC3D-0.17-report.md.

**22 Nov 2026 (input):** keyboard control works on RoMac through input-sdl's default mapping
(Return = Start, Shift = A, Ctrl = B, Z, arrows = stick; `data/InputAutoCfg.ini` is needed).
Driven by `ro-run key`, Mario 64 goes through file select into the game and runs and jumps. SDL's
RISC OS driver scans the keyboard (OS_Byte 121), so keys must really be held (Rorth's taps last
10 cs), and the window needs the input focus.
- Joypad (David's Xbox One controller): RoMac request 3. rol-run has proposed to David: macOS
  GameController -> a Linux joydev device -> a small Joystick module in !RoMac giving Joystick_Read
  (classic and 16-bit forms). Waiting on David; don't build against it until rol-run sends the
  layout. Then riscos-n64 reads it and offers an SDL virtual joystick to input-sdl.
- RoMac: use `~/code/rol-run/build-mac/RoMac.app` (0.1.229+, RISC3D merged; 0.1.230+ restarts
  headless runs on a reset, so `ro-run reboot` works).

**22 Nov 2026 (the app):** `!Mupen64Plus` (`riscos/app`, `riscos/launcher`, `riscos/package.sh`).
- `!RunImage` is a small launcher (riscos-doom3's Wimp code: icon, menu with Info, Help and Quit,
  Info window, message window). Drop a ROM (.z64/.v64/.n64 by its first bytes, or a zip) on the
  icon: it runs `Game` with the ROM's Unix-form name, which runs `Mupen64` (the emulator) in a
  desktop window titled "Mupen64Plus: <game>". A click offers the last ROM again. While a game
  runs, the launcher's icon gives way to SDL's (which has Quit), and comes back when it ends.
- Settings, saves and the emulator's log are in `<Choices$Write>.Mupen64Plus` (the default
  `mupen64plus/cfg` is copied there the first time: plugins, paths as UnixLib `/<Var>/...` names,
  screenshot key Insert into `!Mupen64Plus.Screenshots`, sound buffer target 4096).
- Tested on RoMac by dragging from a Filer window: Mario and Zelda start, Insert and F5 write a
  PNG and a save state, Escape or the close icon ends the game.

**22 Nov 2026 (the app, 2):** David asked for a full-screen key that doesn't clash with RoMac, a
Choices window with the cfg still editable, and save/load on the menu.
- Full screen: mupen64plus's own Alt+Enter already works (a mode change and back). RoMac's
  shortcuts are Command keys, so no clash. Alt+Enter also presses Start (Return), as upstream.
- The emulator puts no icon of its own on the icon bar (`src/riscos_desktop.c`: SDL's
  `Wimp_CreateIcon` declined through `--wrap=_kernel_swi` when `Mupen64Plus$LauncherIcon` is
  set), so the launcher's icon stays. Its menu: Info, Help, Choices..., then the game's Save
  state, Load state, Slot (0-9, ticked), Full screen, Pause (ticked), Stop game, then Quit.
  Commands go through `Mupen64Plus$Command`, read once a frame; the game publishes
  `Mupen64Plus$GameState`.
- Choices window: window size (640/960/1280 wide), start full screen, emulation (recompiler,
  cached or pure interpreter), sound on/off, Edit file (opens the cfg in the editor), Cancel,
  Save. It changes only those lines. Saved during a game, they're written after it ends, as
  the emulator writes the file when it quits.
- Load state crashed ("EMT trap / stack overflow"): an unaligned LDRD in `savestates.c`'s
  `GETDATA`, fixed with memcpy (KB lesson 2026-11-22-unaligned-ldrd-reported-as-stack-overflow).
- All tested on RoMac with Mario: save to slots 0 and 3, load, slot 5 kept, pause, full screen
  from the menu, Choices during a game, stop.
- File type: none is allocated for N64 ROMs (only &0D7 Game Boy); IXFS types files by
  extension through MimeMap, so a MimeMap line would make .z64/.v64/.n64 typed, once a type
  is allocated by ROOL. David (22 Nov): not now; on the backlog (section 10).
- Known: full screen at a size with no matching mode (960x720) shows the picture in a corner.

Next: the joypad (waiting on RoMac request 3). (Choices for saves and settings, sound
buffer defaults).

## 10. Backlog

Not now (David's call when to pick them up):
- An N64 ROM file type (David, 22 Nov 2026: not for now). Ask ROOL to allocate one; then
  `!Boot` sets its name and double-click action (as mGBA does for &0D7), and a MimeMap line for
  .z64/.v64/.n64 types the files on RoMac's IXFS. Test with a restart: a live MimeMap change
  didn't reach IXFS. Until then games start by dragging them to the icon.
- Full screen at a size with no matching mode (960x720): centre the picture.
- Alt+Enter also presses Start (Return is Start), as upstream.
- pi5-octagon kernel question; GLideN64.
- ~~Pi 4 GPU: needs !MesaLibs in !Boot.Resources~~ Fixed by RISC3D 0.22 (26 Nov, section 9).
- 64K colour desktops: retest when riscos-mesa 20.3.5-24 is out (expected to fix it), then drop
  the 16M colour note from !Help, README and the web page.
- **Low-level RSP and RDP plugins, for libdragon and Tiny3D homebrew (experiment).** Modern
  homebrew is built with libdragon, which runs its own RSP code (rspq) and sends RDP commands
  directly, and Tiny3D uses its own microcode. HLE RSP and Rice only know Nintendo's
  standard microcode (F3DEX/F3DEX2), so these games never draw. Tried on 23 Nov 2026:
  - Junkrunner 64 (lambertjamesd/n64brew2025 v3.2.1, MIT) and the N64brew Game Jam 2024 ROM,
    which includes Coin Rush (n64brew/N64brew-GameJam2024 1.2.1, MIT).
  - On RISC OS, the dynarec stops with "Compile at bogus memory address: 8e0003c0". The
    cached interpreter hung on RoMac.
  - The Mac host build's interpreter makes no progress either (no VIs in 40 s), so it isn't
    the ARM port.
  - Tiny3D's examples (e.g. `12_uv_gen`, spherical UVs) have no prebuilt ROMs.

  Candidates:
  - RSP: mupen64plus-rsp-cxd4 (interpreter), or parallel-rsp (recompiler, x86/ARM64 lightning:
    check ARMv7).
  - RDP: angrylion-rdp-plus (software, exact, slow), or paraLLEl-RDP (Vulkan compute: no
    Vulkan on RISC OS, so probably out).

  Expect the software route to be far below full speed on a Pi 4; worth measuring.
  - **Test ROMs:** libdragon's examples, Tiny3D's examples, Junkrunner 64 and the 2024 jam ROM.
  - **Fix first:** the dynarec's 8e0003c0 failure, which is libdragon's IPL3/boot.
  - **Known to work with Rice:** libultra/F3DEX2 homebrew such as megatextures
    (lambertjamesd/n64brew2023).
