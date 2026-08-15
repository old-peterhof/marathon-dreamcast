# Aleph One 0.12.0 on Dreamcast, modern toolchain

Marathon 2: Durandal running on a Sega Dreamcast, built with a 2026 KallistiOS
toolchain instead of the 2002 one the port was written against.

This is BERO's `AlephOne-0.12.0-dc-1` port (2002, sdl-dc.sourceforge.net) applied
to a pristine Aleph One 0.12.0 tarball, plus the work needed to make it build,
render and play on a modern KOS. The game itself was not rewritten.

Current state: boots, renders the full retail campaign, draws the HUD, plays with
either a keyboard or a Dreamcast controller, and keeps its preferences on a VMU.

## Build

You need KallistiOS at `/opt/toolchains/dc/kos`, with `kos-ports` providing SDL
1.2 and libGL, plus `mkdcdisc`.

```sh
./tools/fetch-data.sh     # full Marathon 2 retail data (once, ~29MB download)
./build.sh -j8            # -> alephone.elf
./build.sh flycast        # build a test image and run it
./build.sh cdi            # padded image for hardware or GDEMU
```

Always go through `build.sh`; it sources `environ.sh`, which is where `KOS_BASE`,
`KOS_PORTS`, `KOS_CFLAGS`, `KOS_LDFLAGS` and `KOS_LIBS` come from. Calling
`make -f Makefile.dc` directly fails on purpose.

### Targets

| Target      | Result                                                     |
|-------------|------------------------------------------------------------|
| *(default)* | `alephone.elf`                                             |
| `disc`      | stage `disc/AlephOne` from the skeleton plus the game data  |
| `test`      | unpadded `.cdi`, ~59 MB — **emulator only**, carries test markers |
| `cdi`       | padded `.cdi` — this is the one that boots on hardware      |
| `gdi`       | GDEMU's native format                                       |
| `flycast`   | build the test image and launch it                          |
| `clean`     | objects and the elf                                         |
| `distclean` | also disc images and the staged disc tree                   |

**Do not burn `alephone-test.cdi`.** `-N` drops the data-track padding to keep
the image small for emulator runs, and unpadded images do not boot on a real
console. It also carries the test markers described below. Use `cdi`.

### Game data

Aleph One ships no game data. `tools/fetch-data.sh` pulls the full retail
Marathon 2 from Aleph One's GitHub releases — Bungie made the trilogy freely
available in 2005 and granted the project a distribution license in 2021. Free
for noncommercial use; Bungie keeps the copyright.

The modern package names its files `Map.sceA`, `Shapes.shpA`, `Sounds.sndA` and
`Images.imgA`. Aleph One 0.12.0 predates those extensions and wants bare names,
so the script renames them. `Plugins/`, `Scripts/` and `Physics Models/` are
deliberately skipped: they target Aleph One 1.x and an MML dialect 0.12.0 cannot
parse.

## Controls

The analog stick drives aim rather than movement, because Marathon's aiming
benefits most from analog precision while walk speed is effectively fixed. The
D-pad shares a thumb with the stick, so nothing time-critical during combat
lives there — only stationary actions.

| Control | In game |
|---|---|
| Analog stick | turn left/right, look up/down (continuous) |
| Y | move forward |
| A | move backward |
| X | strafe left |
| B | strafe right |
| R trigger | primary fire |
| L trigger | alt fire |
| D-pad Up | action / use — switches, terminals, pickups |
| D-pad Down | toggle overhead map |
| D-pad Left | cycle weapon forward |
| D-pad Right | spare |
| Start | pause |

In menus the D-pad and stick navigate and **A** confirms, because BERO's menu
handler only understands UP, DOWN and RETURN. `shell_sdl.cpp` switches binding
tables from the game state, releasing every key in the outgoing table so nothing
sticks down across the change.

A keyboard also works throughout. The default key layout is the arrow-key one
(`_left_handed_keyboard_setup`), not Aleph One's usual keypad layout — a
Dreamcast keyboard may have no keypad at all.

**Turn Sensitivity** and **Look Sensitivity** are sliders in Preferences →
CONTROLS. They cap at 100% deliberately: `physics.cpp` quantises `delta_yaw`
into a bounded field (`MAXIMUM_ABSOLUTE_YAW`), so the turn rate saturates near
67°/sec however large a value is fed in. The sliders' useful work is *reducing*
sensitivity for finer aim.

## Saves

Preferences are mirrored to a VMU. The game writes them to the KOS ramdisk —
the only writable filesystem here, since the game runs from `/cd` — and that is
wiped at power-off, so `dc/dc_vmu.c` restores the VMU copy before preferences
are read and writes it back after they are saved. Aleph One is unchanged apart
from two calls in `wad_prefs.cpp` and still only ever touches `/ram`.

Saved games are **not** mirrored; they far exceed a VMU's 128K.

## Identifying a build

Every image is named `alephone-b<N>-<slug>.cdi`, the same `b<N> <slug>` is drawn
at the bottom-left of the main menu, and it is burned into the disc volume label
so it survives a rename. `BUILDS.md` maps each tag to its commit and what
changed, so feedback like "b17 turns too fast at the rim" identifies exactly one
binary.

Start a new one with `tools/new-build.sh <slug> "what changed"`, which bumps the
number, sets the name and appends the row.

## Testing aids

Synthesised keystrokes into Flycast proved unreliable, so the port can drive
itself. Four marker files on the disc, all staged by `make test` and never by
`cdi` or `gdi`:

| Marker | Effect |
|---|---|
| `AUTOSTART` | selects "Begin New Game" a few seconds after the menu appears |
| `AUTOSTART` containing `controls` | opens Preferences → CONTROLS instead |
| `PADTEST` | synthesises a held stick deflection, to exercise the controller path without a pad |
| `DEBUG` | enables `dc_trace()` output over serial and to the framebuffer |

Without `DEBUG` the build is silent; the traces stay in the source rather than
being deleted.

`tools/run-flycast.sh <disc> <log>` launches an image and retries past Flycast's
startup bug. Flycast on macOS fails to initialise perhaps half the time with a
`Verify Failed ... driver.cpp:349` assertion — it depends on ASLR, not on the
disc image, and `Dynarec.Enabled` makes no difference. The script checks
liveness by PID rather than `pgrep -f Flycast`, which also matches the launching
shell and reports success for a process that already died.

## What had to change, and why

### Building at all

**A missing `typename`.** `sdl_widgets.h` declared
`virtual void draw_item(vector<T>::const_iterator i, ...) const = 0;`. That is a
dependent type; gcc 2.95 accepted it bare, gcc 15 parses the parameter as `int`.
Every override then mismatched the pure virtual, so four widget subclasses
stayed abstract and five translation units failed.

**`struct dirent` lost its `size`.** `FileHandler_SDL.cpp` read `de->size` and
treated a negative value as "directory" — a KOS 1.1.7 extension. Modern KOS uses
standard newlib `dirent`, but its iso9660 driver implements `stat()`, so the
generic path works.

**One surviving `try`/`catch`.** The port builds `-fno-exceptions`, which is why
BERO rewrote `try`/`throw` as `goto` elsewhere. He missed `wad_prefs.cpp`.

**A gcc 15 SH4 codegen bug.** `weapons.cpp` at `-O2` emits
`mov.w r0,@(10,macl)` after a `mul.l`, using the multiply-accumulate register as
an address base without an `sts macl,rN` first. The assembler rejects it. That
file is pinned to `-O1`.

**The link line.** SDL 1.2's DC video backend calls into KOS's GL, so `-lGL` is
required even though the software renderer is used; `-nostdlib` means
`-lstdc++ -lsupc++` must be named; and the explicit `startup.o` BERO linked by
hand is now supplied by libkallisti.

**A name collision.** Pfhortran's `stack_top` collided with KallistiOS's. Now
`static`.

**Hidden inputs in the Makefile.** Aleph One selects its platform layer by
`#include`ing one `.cpp` from another, so `shell_sdl.cpp`, `preferences_sdl.cpp`,
`shapes_sdl.cpp` and six others never get objects of their own. Without explicit
rules, editing them silently produces an unchanged binary. BERO's Makefile
carried the `shell.o` line for this reason; the rest were missing there too.

### Running

**MacBinary I.** The retail `Map` is wrapped in a MacBinary container, so the wad
header sits at offset 128. Aleph One strips MacBinary already, but
`is_macbinary()` required bytes 122/123 to be ≥ 0x81 — the MacBinary **II**
version stamp — and the retail data is MacBinary **I** with zeroes there. Reading
offset 0 gave `version=3, data_version=0x4D61` (the "Ma" of "Map") and the
"error 4" dialog. `is_macbinary()` now accepts both, applying the CRC test only
when the II stamp is present and otherwise sanity-checking the fork sizes.

**`SDL_BlitSurface` silently does nothing.** It returns 0 for success and copies
nothing on this driver, verified by reading the destination VRAM immediately
before and after while the source buffer was ~97% non-zero. `dc_copy_to_screen()`
in `screen_sdl.cpp` copies rows directly instead; both the world view and the HUD
go through it. Not every blit is affected — the menu's title screen goes through
`images_sdl.cpp` and always worked, which is why the menu looked healthy while
gameplay was black.

**No controller support in SDL.** SDL 1.2's DC driver polls only
`MAPLE_FUNC_MOUSE` and `MAPLE_FUNC_KEYBOARD` and ships a dummy joystick backend;
`MAPLE_FUNC_CONTROLLER` is never read. `dc/dc_input.c` reads the pad off the
maple bus and injects through `SDL_PrivateKeyboard`, which updates SDL's key
state array — `SDL_PushEvent` alone would drive menus but leave the player
motionless, since `vbl_sdl.cpp` polls `SDL_GetKeyState`. The stick and triggers
take the analog route through `mouse_sdl.cpp`, which reads them instead of a
pointer; `input_device` defaults to `_mouse_yaw_pitch` because `vbl_sdl.cpp`
skips `test_mouse()` entirely otherwise.

**The cursor smeared.** SDL's software cursor blits without restoring the
background, leaving trails across the menu. It is hidden on this platform.

### The 2002 DC glue is gone

`dc/syscalls.c` and `dc/fs_mem.c` could not be reused: the first duplicates
newlib symbols modern KOS provides, the second reimplements what is now
`fs_ramdisk`. `dc/dc_compat.c` supplies the only two things still missing,
`fs_mem_init()` and `access()`.

### Things BERO warned about that no longer apply

His README says to raise `MAX_ISO_FILES` from 8 and rebuild KOS. The modern
iso9660 driver uses dynamic handles and has no such constant.

## Known gaps

- **Nothing here has run on real hardware.** Everything was developed and tested
  under Flycast. `./build.sh cdi` produces the padded image for a console.
- **Framerate on hardware is unmeasured.** The software renderer holds a steady
  29–30 fps under Flycast, but that is the engine's own tick cap and Flycast's
  SH-4 dynarec does not throttle to 200MHz. This says nothing trustworthy about
  a real Dreamcast.
- **The PowerVR path is a renderer rewrite, not a port.** `GL=1` exists in
  `Makefile.dc` but does not build. Every entry point `OGL_Render.cpp` and
  `FontHandler.cpp` need beyond GLdc's subset is missing, and while most
  substitute trivially, the five clip planes used for portal and liquid-surface
  clipping have no equivalent — emulating them means clipping polygons in
  software before submission. The Makefile comment lists the full set.
- **The controller mapping has not been exercised with a real pad.** Detection is
  confirmed and the whole chain from binding table to the player turning is
  verified via the `PADTEST` self-test, but Flycast would not route host input to
  an emulated controller in any configuration tried.
- **Setting-level persistence is assumed.** The VMU file round trips and parses,
  but that a *changed* slider survives a reboot has not been observed, because
  confirming it needs input into the preferences dialog.
- **Sound is unverified past "it plays".**
- `-fpermissive` and `-fno-strict-aliasing` are load-bearing. The renderer
  type-puns freely and the 2002 code leans on conversions the modern front end
  rejects.

## Working on this code

Two traps worth knowing before you start.

`grep` on the development machine was `ugrep`, which silently skips files it
considers binary — and **61 of the source files are ISO-8859, not UTF-8**,
including `wad.cpp`, `render.cpp` and `map.h`. Searches return nothing at all,
with no warning. That hid a second `errUnknownWadVersion` site and produced a
confidently wrong diagnosis. Search this tree with Python.

Flycast rewrites `emu.cfg` when it quits, so editing the file while it is running
silently reverts your change. Kill it first.

## Provenance

- Aleph One 0.12.0 — `downloads.sourceforge.net/marathon/AlephOne-0.12.0.tar.gz`
- DC port — BERO, 2002, `AlephOne-0.12.0-dc-1`, GPL-2.0
- Marathon 2 retail data — Aleph One release `release-20250829`,
  `Marathon2-20250829-Data.zip`

Marathon is © Bungie. Aleph One is GPL-2.0; see `COPYING`.
