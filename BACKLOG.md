# Backlog

Wanted, not yet started. Ordered roughly by appetite rather than difficulty.

## Rumble pack support

Force feedback for gun shots and for taking hits. Wanted explicitly.

Notes for whoever picks it up:

- KOS has a driver at `dc/maple/purupuru.h`. It is left **enabled** in this
  port precisely so this stays possible — see `dc/dc_maple.c`, where it would
  have been the blunter fix for the boot hang and was deliberately not used.
- A rumble pack currently prevents the game booting at all (see BUGS.md), and
  the cause is now known: KOS's `maple_wait_scan()` waits forever for all four
  ports to report, and the pack makes one never report. Pulling it out while
  hung releases the wait and the boot continues. The rumble driver itself is
  exonerated — b21 ran with `INIT_PURUPURU` cleared and still hung.
- That has to be solved first, and it is a prerequisite rather than a detour: a
  device that will not enumerate cannot be driven either. Two routes, both real
  work: patch KOS's maple to bound the wait, or clear `INIT_MAPLE_ALL` and take
  over maple init ourselves so we control the timeout. Before either, try an
  official pack — this was a third-party unit and may just answer enumeration
  badly.
- The natural hook points are the same places the game already makes noise:
  weapon fire in `weapons.cpp` and damage in `player.cpp`.

## Controller-native UI

Rebuild the dialogs, save/load windows, preference screens and the main menu to
be designed for a controller, while still looking like Marathon. The present UI
is a mouse interface with keyboard navigation bolted on, and every controller
problem so far has been a symptom of that rather than a bug in the pad code.

What is already known, so this does not start from scratch:

- **The main menu is a 2D layout driven by a 1D list.** `shell_sdl.cpp` carries
  a hardcoded `menus[]` array (BERO's) in a fixed order, and UP/DOWN walk that
  array. It does not follow what the eye sees: the screen has two columns, so
  "down" sometimes jumps across the screen. A controller wants the traversal to
  match the layout.
- **Lists trap focus.** `w_list_base::event` swallows UP and DOWN by design --
  "Prevent selection of previous/next widget" -- so a focused list cannot be
  left with a D-pad. We work around it by sending TAB from the triggers, which
  works but is not discoverable: nothing on screen says so.
- **Dialog navigation is inconsistent.** In `dialog::event`, UP *and* LEFT both
  mean "previous widget" while DOWN and RIGHT mean "next", except when the
  focused widget consumes them first -- `w_slider` eats LEFT/RIGHT to adjust,
  `w_list` eats UP/DOWN to scroll. So which key does what depends on what is
  selected, which is fine with a mouse and confusing with a pad.
- **There is no consistent focus indicator.** The main menu highlights the
  selected button (BERO added that), but dialog widgets rely on subtler cues
  that were designed to be clicked rather than cursored to.
- **The look is data-driven, which helps.** Themes live in
  `disc-AlephOne/Themes/Default` as MML plus bitmaps, so a lot of restyling is
  data rather than code. `sdl_dialogs.cpp` and `sdl_widgets.cpp` hold the
  layout and behaviour.

Worth deciding early whether this is a reskin of the existing widget set or a
parallel controller-first set of screens that reuses the theme art. The second
is more work but avoids fighting a widget system built around a pointer.

## PowerVR / hardware-accelerated renderer

Status after b34: **it builds, links, initialises a real GL context on the
PowerVR, loads a level, and dies of memory exhaustion on the first frame.**

    gl: requested=1 obtained=1
    gl: PowerVR2 CLX2 100mHz / 1.2 (partial) - GLdc 1.1

The old note here said this was blocked on clip planes and needed a renderer
rewrite. That was wrong. All six glClipPlane calls are inside RenderModelSetup(),
reached only when rectangle_definition::ModelPtr is set -- the external-3D-model
path, which stock Marathon 2 never enters. See dc/dc_gl_compat.h.

Fixed along the way:

- 17 missing GL entry points, via dc/dc_gl_compat.h, force-included under GL=1
- display lists, in both OGL_RenderText and FontHandler's 256-glyph table
- gluScaleImage and gluBuild2DMipmaps, in dc/dc_glu.c
- GL_LUMINANCE_ALPHA font textures, which GLdc refuses ("Couldn't find stride
  for format: 0x190a") -- expanded to RGBA
- SDL_OPENGLBLIT, which the Dreamcast backend has no notion of, to SDL_OPENGL
- a second SDL_SetVideoMode at level entry tearing the PowerVR down underneath
  GLdc, which tripped "Assertion pvr_state.valid failed" in pvr_wait_ready

### What is left: memory

A Dreamcast has 16MB. Measured heap, GL build:

| stage | heap |
|---|---|
| after initialize_marathon | 812 KB |
| OGL_StartRun entry | 8,760 KB |
| after OGL_StartTextures | 9,148 KB |
| after screen font | 9,532 KB |
| after map fonts | 10,684 KB |
| at first render | 12,084 KB |
| next request | +2,048 KB, refused |

The interesting line is the second: 8MB goes in during level loading, before a
single GL call. GL setup adds about 3.3MB on top of that, and the last 2MB
request is what falls off the end.

So the work is not in the renderer, it is in the 8MB baseline. Options, roughly
in order of promise:

1. Find what the level load actually allocates. It is the same load the software
   build survives, so the question is how much headroom software has -- measure
   `heap: at first render` in a software build and subtract.
2. Texture settings are already at half resolution, 16-bit colour, no mipmaps,
   and verified applied (`txtr[0]: res=1 fmt=8056 far=2601`). Quarter resolution
   is available if needed.
3. GLdc's initial OP/TR/PT vertex list capacities are configurable through
   glKosInitConfig; the SDL backend calls plain glKosInit and takes the defaults.

## Get a saved game below 10 blocks

Done in b29, most of the way: folding a save against its map level took it from
163 blocks to 23. What is left, if it needs to be smaller still, is the stock
physics models -- MNpx, PRpx, WPpx, FXpx, PXpx, about 11.7K raw -- which the map
file does not carry, so there is nothing to fold them against. They are only in
a save at all because a physics file *could* have been loaded.

The fix would be to record "physics were stock" in the header, drop those five
chunks, and splice them back in on restore. The awkward part is that the stock
bytes come from the engine's own tables at runtime rather than from a file, so
the restore path would have to run after those tables exist rather than at boot,
where it runs now.

Worth maybe 6 blocks of the 23. Not obviously worth the complexity unless a card
is very full. Doom 64 manages 10 blocks, but it has far less state to keep.

## Which preferences and menu items to strip on console

For discussion with Max, not to be actioned unilaterally.

The UI still offers a lot that means nothing on a Dreamcast, and every item is
one more thing to navigate past with a d-pad. Candidates, roughly in order of how
obviously they should go:

- Network: GATHER NETWORK GAME and JOIN NETWORK GAME. `network_dummy.cpp` is
  linked, so these cannot work at all; they currently blink and return.
- Films: REPLAY SAVED FILM, REPLAY LAST FILM, SAVE LAST FILM. Recordings are
  written to the ramdisk and die at power-off, so a film cannot outlive the
  session that made it.
- QUIT. A console has no desktop to return to.
- Preferences that describe hardware we know: resolution, colour depth, fullscreen
  toggle, OpenGL options while the GL path is unbuilt.
- Keyboard-only preferences: key bindings, mouse sensitivity as distinct from the
  stick sensitivity already added.

Against stripping: the main menu is a fixed 1990s bitmap with the buttons drawn
into it, so removing an item means either editing artwork or leaving a dead
region on screen. That argues for doing this as part of the controller-native UI
work rather than before it.

## Rebuild KallistiOS and its ports with heavier optimisation

Falco Girgis (KallistiOS) suggested this after seeing the port run: enable -O3,
-ffast-math, -mfsca, -mfsrra and -flto in the KOS environ.sh, re-source it, then
rebuild KOS, every dependency and the project so the flags reach all translation
units.

Half of it is done. `FAST=1` puts -O3 -ffast-math -flto on Aleph One's own
objects and the link, and b35 was built and verified both ways: the picture is
identical, 12388 of 12800 pixels drawn against 12400 for the ordinary build, so
-ffast-math does not disturb the software renderer. -mfsca and -mfsrra were
already coming from KOS_CFLAGS.

What is left is the larger half: KOS itself, SDL, GLdc and zlib were all built
with whatever environ.sh specified at the time, and the renderer spends most of
its life in code this project did not compile. That means editing
/opt/toolchains/dc/kos/environ.sh, re-sourcing, and rebuilding kos and kos-ports
from scratch -- an hour or so, and it invalidates every object here, which the
config stamp will notice.

No measurement yet. Flycast holds at 30fps because that is the engine's tick
rate, not the renderer's limit, so the difference can only be seen on hardware.
