# Aleph One 0.12.0 on Dreamcast, modern toolchain

Marathon 2: Durandal running on a Sega Dreamcast, built with a 2026 KallistiOS
toolchain instead of the 2002 one the port was written against.

This is BERO's `AlephOne-0.12.0-dc-1` port (2002, sdl-dc.sourceforge.net) applied
to a pristine Aleph One 0.12.0 tarball, plus the work needed to make it build,
render and play on a modern KOS. The game itself was not rewritten.

Current state: boots on real hardware, renders the full retail campaign, draws
the HUD, plays with a Dreamcast controller, keeps preferences *and saved games*
on a VMU, and has an interface built for a pad rather than a mouse. Roughly 15-20
fps on a console; the software renderer is the one that ships.

A hardware-accelerated PowerVR renderer exists and works -- it draws textured
walls, sprites and the HUD through GLdc -- but it is not in the shipping build
and is not finished. See "The PowerVR renderer" below.

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
| `play`      | unpadded image, autostart + traces, no synthetic stick: for driving by hand in Flycast |
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
| D-pad Left | run / swim |
| D-pad Right | cycle weapon forward |
| Start | bound to Escape, which gameplay ignores -- see Known gaps |

In menus the D-pad and stick navigate and **A** confirms, because BERO's menu
handler only understands UP, DOWN and RETURN. `shell_sdl.cpp` switches binding
tables from the game state, releasing every key in the outgoing table so nothing
sticks down across the change.

A keyboard also works throughout. The default key layout is the arrow-key one
(`_left_handed_keyboard_setup`), not Aleph One's usual keypad layout — a
Dreamcast keyboard may have no keypad at all.

That table is the binding, not the key name. `game_bindings` in `dc/dc_input.c`
sends `SDLK_QUOTE` for D-pad Right and `SDLK_LCTRL` for D-pad Left, which are
the left-handed table's next-weapon and run keys; the two paragraphs below say
why neither key is the one its name suggests.

**Turn Sensitivity** and **Look Sensitivity** are sliders in Preferences →
CONTROLS. They cap at 100% deliberately: `physics.cpp` quantises `delta_yaw`
into a bounded field (`MAXIMUM_ABSOLUTE_YAW`), so the turn rate saturates near
67°/sec however large a value is fed in. The sliders' useful work is *reducing*
sensitivity for finer aim.

D-pad right cycles weapons forward and D-pad left swims. Neither key is the one
its name suggests.

Next weapon is `SDLK_QUOTE`. Aleph One keeps three key tables in
`key_definitions.h`, and this port's bindings match the **left-handed** one
throughout — arrows to move, `z`/`x` to sidestep, tab for action, `m` for the map
— and that table binds `_cycle_weapons_forward` to the quote key. The standard
table uses keypad 9; binding that here breaks weapon cycling.

Swim is `SDLK_LCTRL`, the run key, which is the same in every table. There is no
separate swim action: `player.cpp` converts run into swim when the player's head
is under liquid, so the same button runs on land.

## The interface

Marathon 2's interface is a *mouse* interface with keyboard navigation bolted on,
and most of it could not be driven from a Dreamcast pad at all. Since b58-b60 it
is a pad interface. `MENU-TREE.md` is the full audit and `UI-HANDOFF.md` is the
design it implements; the short version:

**The main menu is no longer artwork.** It was two full-screen PICTs with the
buttons painted in, highlighted by clipping to one of eighteen hardcoded
rectangles, and four files had to agree on item order through
`rect = item - 1 + _new_game_button_rect`. It is now a static plate with text
drawn over it (`dc_mainmenu.cpp`), so changing which items exist costs an array
entry rather than a paint program. Five items: New Game, Continue Game, Manage
Saves, Preferences, Credits.

Nothing was deleted from the engine to do that. `iManageSaves` is appended to the
enum so every other id keeps its value, and the rectangle table is untouched, so
non-DC builds behave exactly as before.

**The plate is baked from the design prototype**, not hand-drawn:
`tools/bake-plate.py` renders `mockups/prototype` in headless Chrome and writes a
24-bit BMP, so the artwork cannot drift from the design. BMP because
`SDL_LoadBMP` is core SDL; PNG would mean linking libpng, which this port does
not.

**Start opens a pause menu** -- Resume, Save Game, Preferences, Quit to Main
Menu. Every in-game command in this engine is an Alt+key chord, which a pad
cannot produce, so before this there was no way to pause, no way to save away
from a terminal, and no way off a level short of resetting the console. The
dialog *is* the pause: dialogs run their own event loop, so the world stops while
one is open.

**Start always means back out**, in every context, and is not bindable. So are X
and Y, which are the secondary and tertiary actions -- delete on the saves
screen, page-flip and defaults on the binding screen. The one destructive action
in the interface must not be something a player can rebind away, and the binding
screen's own escape hatches cannot depend on the bindings being edited on it.

**Every settings row explains itself** in a line of plain English, driven by a
focus callback in `dialog::activate_widget`. Someone across a room with a pad
cannot hover, read a manual, or try a setting and put it back in two seconds.

Two things a screen here has to respect, both learned the hard way. Nothing
readable may sit within 40px of any edge, because a television eats it -- traces
drawn 8px in were completely unreadable on a real set. And **no 1px horizontal
lines**: the output is 480i, so a one-pixel row exists on one field only and
buzzes at 30Hz. Vertical lines are fine. `UI-BRIEF.md` has the measurements.

## Saves

Preferences are mirrored to a VMU. The game writes them to the KOS ramdisk —
the only writable filesystem here, since the game runs from `/cd` — and that is
wiped at power-off, so `dc/dc_vmu.c` restores the VMU copy before preferences
are read and writes it back after they are saved. Aleph One is unchanged apart
from two calls in `wad_prefs.cpp` and still only ever touches `/ram`.

Saved games **are** mirrored, and getting them to fit took some doing. A
Marathon 2 save is 214629 bytes. A VMU does not hold the 128K on its box: 200 of
its 256 blocks are available to files, so about 102400 bytes, less whatever else
is on the card. The first attempt refused the write and said so -- `card full,
need 421 blocks, 194 free` -- which is how the real figure was measured rather
than guessed.

94% of a save is a verbatim copy of the map already pressed on the disc. So
before compressing, `dc/dc_wad.c` XORs the save chunk by chunk against the same
chunk of the same level read back off `/cd`. Bytes that did not change become
zero and deflate swallows runs of zeroes: 82495 bytes down to 10834, 163 blocks
down to 23.

XOR rather than a cleverer record-level diff because it is its own inverse and
needs to understand nothing about the data -- no table of which fields of
`side_data` a switch may alter. Restore runs the identical call. The geometry
could not simply be dropped instead: sides come back 1.5% changed and polygons
1.3%, which is switches and platforms, and a reverted switch is a real bug.

### Four slots

Since b59 the player sees four save slots rather than a directory. Stock Aleph
One asks for a typed filename, which is why every save this port made before then
was called "Untitled Game"; the slots generate their own names, so nothing has to
be typed.

The card header is versioned. v1 was 64 bytes and full -- the name field ran to
byte 63 with nothing spare -- so v2 is a longer header under a different magic
(`A1S2`), carrying the level name, an RTC timestamp, a sequence counter, elapsed
play time and difficulty. That is what lets the slot screen list four saves from
four 128-byte reads instead of decompressing and unfolding each one to find out
what it is. **Bytes 4 to 14 keep their v1 meaning exactly**, because that range
holds the level number the payload was folded against, and unfolding against the
wrong level produces garbage that the wad reader accepts without complaint.

Both versions still read, so a card written by an older build keeps working and
simply lists as "Level 7" with no name or date.

The slot model is a table, not a card layout. The player sees four; the card is
scanned to eight, and each table entry remembers which card and which card slot
it actually came from. A save an older build put in card slot 7 becomes the
player's slot 2 with nothing moved -- renumbering would mean rewriting a player's
data for cosmetic tidiness.

Ordering is on the sequence counter and not the clock, because a Dreamcast with a
flat battery reports a fixed date, and Continue Game opening the wrong save is
worse than showing the wrong date beside it.

### Preferences written by one build can confuse another

Adding a field to any preference struct changes the size of a stored chunk, and
the file lives on the card rather than the disc, so it outlives every image you
burn. `dc/dc_vmu.c` stamps the card copy with a number derived from the sizes of
the preference structs the build was compiled against; a card that does not match
is ignored and Aleph One writes fresh defaults over it.

Treat this as a guard rather than a diagnosis. Deleting the preferences file
demonstrably fixes a console that will not start a level, but the mechanism has
not been reproduced -- see BUGS.md.

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
itself. Marker files on the disc, staged by the test targets and never by `cdi`
or `gdi`:

| Marker | Effect |
|---|---|
| `AUTOSTART` | selects "Begin New Game" a few seconds after the menu appears |
| `AUTOSTART` containing `controls` | opens Preferences -> CONTROLS instead |
| `AUTOSTART` containing `load` | selects "Continue Game" instead |
| `AUTOSTART` containing `saves` | opens MANAGE SAVES instead |
| `AUTOSTART` containing `binds` | opens CONFIGURE CONTROLLER instead |
| `PADTEST` | synthesises a held stick deflection, to exercise the controller path without a pad |
| `DEBUG` | enables `dc_trace()` over serial and to the framebuffer |
| `PROFILE` | starts the VMU Profiler, so a framerate can be read off the LCD on hardware |

Without `DEBUG` the build is silent and the traces stay in the source rather than
being deleted.

Traces draw 40 pixels in from the corner and wipe their row first. Both matter on
a real set: at 8 pixels overscan eats them, and without the wipe a short line
leaves the tail of a longer one behind it, which reads as gibberish.

`tools/run-flycast.sh <disc> <log>` launches an image and retries past Flycast's
startup bug. Flycast on macOS fails to initialise perhaps half the time with a
`Verify Failed ... driver.cpp:349` assertion -- it depends on ASLR, not on the
disc image, and `Dynarec.Enabled` makes no difference. The script checks liveness
by PID rather than `pgrep -f Flycast`, which also matches the launching shell and
reports success for a process that already died.

`tools/verify-image.sh` mounts the *currently staged* tree and refuses anything
carrying a test marker. `tools/check-image.py` reads the markers out of an image
that already exists, which verify-image cannot do -- it is blind to a target that
stages a marker and removes it again, as `cdi-debug` and `cdi-profile` both do.

Flycast reproduces load timings to the millisecond run to run, which makes
single-variable experiments trustworthy. It does **not** reproduce at least one
hardware-only failure; see BUGS.md before trusting it for anything else.

Two further aids exist on the `measurements-b31` branch rather than here, because
they were built on a baseline that fails on hardware: a `SLOWTRACE` marker that
holds each trace ~800ms so a sequence can be read on a television, with a
`cdi-slow` target, and an `AUTOKEY` marker that injects a keypress into a dialog
with a `loadtest` target. Both are worth lifting across when there is a
hardware-confirmed baseline to lift them onto.

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

**The video surface is video RAM, and `SDL_UpdateRect` does nothing.** This port
never calls `SDL_DC_SetVideoDriver`, so it gets the default `SDL_DC_DMA_VIDEO`,
which is not one of the textured drivers; it does not ask for `SDL_DOUBLEBUF`
either. `DC_SetVideoMode` therefore takes its plain path and sets
`current->pixels = vram_l`, and `DC_UpdateRects` has an empty body except in
textured mode. Two consequences worth knowing before optimising anything that
draws:

- Calls to `SDL_UpdateRect` and `SDL_UpdateRects` are free, and removing them
  gains nothing. A diagnosis that blamed one for being slow was wrong.
- **Every read back from the surface is an uncached VRAM access.** Writes are
  cheap and can go through the store queues; reads are not. Anything shaped like
  read-modify-write over a large area -- a blend, a tint, a fade -- costs far
  more than the same loop in main memory, and should be done against
  `world_pixels` on the way to the screen rather than against the screen
  afterwards. Both the pause-menu scrim and the damage flash were originally
  written the expensive way round.

### The 2002 DC glue is gone

`dc/syscalls.c` and `dc/fs_mem.c` could not be reused: the first duplicates
newlib symbols modern KOS provides, the second reimplements what is now
`fs_ramdisk`. `dc/dc_compat.c` supplies the only two things still missing,
`fs_mem_init()` and `access()`.

### Things BERO warned about that no longer apply

His README says to raise `MAX_ISO_FILES` from 8 and rebuild KOS. The modern
iso9660 driver uses dynamic handles and has no such constant.

## Load times

A New Game takes about 46 seconds under Flycast, and a good deal longer on
hardware. Measured stage by stage:

| stage | ms |
|---|---|
| menu fade out | 503 |
| chapter screen | 22553 |
| `goto_level` / `load_level_from_map` | 1603 |
| `load_collections` | 7663 |
| `load_all_monster_sounds` | 13700 |
| `start_game` | 512 |
| **total** | **46605** |

Neither of the two dominant items is reading the level.

About ten seconds of the chapter screen was `wait_for_click_or_keypress`, which
is meant to be skippable and was not: the loop in `csmisc_sdl.cpp` only polled
SDL's event queue, and nothing puts a Dreamcast controller into that queue except
`dc_input_poll()`. It now does, so a button skips it.

Loading monster sounds is 13.7 seconds, one seek and read per sound. The **More
Sounds** preference makes each sound load all of its permutations rather than
just the first; turning it off halves that stage, 13700ms to 6240ms. Left as a
preference because it is the player's choice and Preferences already exposes it.

Do not try to fix this with a bigger stdio buffer. It was tried. KOS's ISO9660
driver does have a bulk-read fast path for sector-aligned requests, but
`wad.cpp` reads a chunk by seeking to its offset and then reading a few hundred
bytes, so a 64KB buffer means every seek discards it and re-reads 64KB to serve a
short read. That build failed to load a level at all on hardware.

## The PowerVR renderer

`GL=1` builds Aleph One's hardware renderer against GLdc. It compiles, links,
gets a real context -- `PowerVR2 CLX2 100mHz / 1.2 (partial) - GLdc 1.1` -- and
draws textured walls, floors, ceilings, sprites and the HUD. It is not in the
shipping build and is not finished.

The note that used to live here said this was blocked on clip planes and needed a
renderer rewrite. That was wrong. All six `glClipPlane` calls are inside
`RenderModelSetup()`, reached only when `rectangle_definition::ModelPtr` is set --
the external-3D-model path, which stock Marathon 2 never enters.

What actually blocked it was one number. From GLdc's `attributes.c`:

    case GL_DOUBLE:
        return (ATTRIB_LIST.vertex.size == 3) ? _readPosition3d3f
                                              : _readPosition2d3f;

Only a size of 3 is understood for `GL_DOUBLE`; anything else silently falls to
the two-component reader, which sets z = 0. `OGL_Render.cpp` asks for
`glVertexPointer(4, GL_DOUBLE, ...)` on walls and 3 on sprites, so every wall
vertex collapsed onto one plane and the world rendered black while sprites drew
perfectly.

**The GL work was lost and has been restored.** b32 reverted the tree wholesale
to b31 to get back to a known-good hardware baseline, and took b33-b36 with it --
`README.DC.md` went on describing a renderer that was no longer in the branch.
Only `dc/dc_gl_compat.h` survived, because nothing else referenced it. It is
recovered from `f25083d` on the `gl-renderer` branch.

**What blocks it now is `SDL_OPENGLBLIT`, and the number is exact.** SDL keeps a
full-screen shadow surface for 2D drawn over a GL scene, and chooses 16-bit for
it only inside `#ifdef GL_VERSION_1_2` (`SDL_video.c:825`). GLdc's `GL/gl.h` does
not define that macro, so SDL's 16-bit branch is compiled out and the surface is
**32-bit: 640x480x4 = 1,228,800 bytes**. A trace of the mode set says so
directly:

    mode 0: 640x480x16 -> surf 640x480x32

That costs twice. The surface itself is 1.2MB instead of 614KB, and
`SDL_DisplayFormat` matches the display -- so each cached menu plate is 1.2MB
rather than 614KB, and there are two. Roughly 1.8MB of a 16MB machine, spent on
a format nothing wants.

**The fix is one macro, in the SDL port rather than in this tree.** GLdc supports
`GL_UNSIGNED_SHORT_5_6_5` natively -- it is the PowerVR's own texture format,
`texture.c:380` -- so SDL's 16-bit path would work, upload in the hardware's
format instead of converting 32-bit every frame, and leave the video surface at
16 bits, which is what every drawing routine in `dc/` and `Source_Files/Misc/dc_*`
already assumes. Rebuilding the SDL kos-port with `-DGL_VERSION_1_2` is the
experiment; it has not been done, because it patches the toolchain and this port
depends on that install being reproducible.

Meanwhile `dc_plate_release()` frees the menu plates on the way into a level and
suspends further loads until it is left, which is worth up to 1.2MB in GL builds
and is gated to them.

Still open: skies are flat colours because a real one needs a 2MB conversion
buffer, the static effect is gone because it needs `glLogicOp`, and none of it
has been measured on hardware.

## Known gaps

- **Framerate on hardware is roughly 15-20 fps**, software rendered. Flycast
  holds a steady 30, but that is the engine's own tick cap (`TICKS_PER_SECOND`
  is 30 in `map.h`) rather than the renderer's limit, so Flycast says nothing
  about how fast the renderer is. Only the VMU Profiler on a console does.
- **The engine tick is 30Hz and cannot simply be raised.** Physics, monster AI
  and film recording all key off it. Rendering more often would draw duplicate
  frames without view interpolation, which upstream Aleph One added many years
  later.
- **At least one failure reproduces only on hardware.** A console that will not
  start a level is cured by deleting the preferences file from the VMU, and the
  obvious mechanism -- a preference struct changing size -- does not reproduce
  in Flycast in either direction. See BUGS.md.
- **The rumble pack hangs the boot.** KOS's `maple_wait_scan()` waits without a
  timeout for all four ports to report, and a third-party pack never does;
  pulling it out mid-hang lets boot continue. It is bound to `INIT_MAPLE_ALL`
  alongside `maple_init`, so it cannot be disabled without losing the
  controller. Third-party packs are reportedly prone to this.
- **Sound is unverified past "it plays".**
- `-fpermissive` and `-fno-strict-aliasing` are load-bearing. The renderer
  type-puns constantly and this is 2002 C++.
- **gcc 15's SH4 backend miscompiles `weapons.cpp` at -O2**, emitting
  `mov.w r0,@(10,macl)` -- the multiply-accumulate register as an address base --
  which the assembler rejects. That file is pinned to `-O1` in `Makefile.dc`.

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
