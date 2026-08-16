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

**It renders.** Textured walls, floors, ceilings, sprites and the HUD, on the
PowerVR at 640x480, verified by screenshot in Flycast.

Not measured on hardware yet, and that is the only number that matters -- Flycast
holds at 30fps because that is the engine tick, not the renderer.

### The two things that actually blocked it

**Not clip planes.** Every build note since the first session said this needed a
renderer rewrite because GLdc has no glClipPlane. All six calls are inside
RenderModelSetup(), reached only when ModelPtr is set -- the external-3D-model
path stock Marathon 2 never enters.

**The world was black because of one number.** GLdc's vertex-array reader, from
attributes.c:

    case GL_DOUBLE:
        return (ATTRIB_LIST.vertex.size == 3) ? _readPosition3d3f
                                              : _readPosition2d3f;

Only a size of 3 is understood for GL_DOUBLE. Anything else silently falls to the
two-component reader, which takes x and y and sets z = 0. OGL_Render.cpp asked
for `glVertexPointer(4, GL_DOUBLE, ...)` on walls and 3 on sprites -- so every
wall vertex collapsed onto one plane while sprites drew perfectly. The fourth
component is the homogeneous w of an eye-space point and is 1, so asking for 3
loses nothing.

That took counters rather than reading: walls=1860/sec reaching the renderer,
setupfail=0, vecfail=0, geometry demonstrably submitted, screen still black.

### Still to do

1. **Measure on hardware.** The whole point.
2. Memory is tight. Peak fits now -- flat landscapes, half-resolution 16-bit
   textures, no mipmaps, and reducing each texture buffer as it is built rather
   than building both then shrinking -- but the heap sits around 12.5MB of 16MB.
3. The static/interference effect is gone: it needs glLogicOp and
   glPolygonStipple, neither of which the PowerVR can do.
4. Landscapes are flat colours rather than textured skies, to avoid a 2MB buffer.
   Worth revisiting if memory frees up.
5. The diagnostic counters in OGL_Render.cpp and screen_sdl.cpp can go once this
   is settled.

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

## Falco's optimisation flags -- done, one at a time

All three enabled in /opt/toolchains/dc/kos/environ.sh, with KallistiOS, zlib,
libpng, libGL and SDL rebuilt clean at each step, and each step tested on
hardware before the next was added. KOS_CFLAGS reaches this project's own
compiles and its link too, so Aleph One gets them as well.

| build | flags | text | hardware |
|---|---|---|---|
| b48 | baseline, -O2 | 1272373 | loads, fast |
| b51 | -O3 | 1412357 | loads |
| b54 | + -fbuiltin -ffast-math -ffp-contract=fast | 1411365 | loads, maybe +1fps, nothing wrong |
| b55 | + -flto=auto -ffat-lto-objects | 1397381 | loads, no change in fps |

-mfsca and -mfsrra were already arriving from the sub-architecture config; what
they needed was -ffast-math to let gcc actually use them for sin and cos.

**Conclusion: the flags bought roughly nothing.** About +1fps across all three,
inside what can be told apart by eye, with the framerate readable on the VMU for
the last two. That is a result rather than a failure: the software renderer is
not compiler-bound, so further speed has to come from the renderer itself.

The flags are kept anyway -- they cost nothing, do no harm, and b55 is the
current baseline -- but this direction is closed. Two levers remain untried and
neither looks promising given the above: -freorder-blocks-algorithm=simple and
-fipa-pta, which KOS's environ.sh calls empirically good for release builds.

Where the frames actually are, if they are anywhere: the PowerVR renderer, which
draws the world in hardware instead of on the SH4.

## Player health and air on the VMU

Wanted. The VMU already shows the title, the build number and the framerate; Max
would like health x/100 and air x/100 under them.

The mechanics are already in place -- `dc/dc_profiler.c` adds measurements to the
VMU Profiler and a `use_string` measurement renders its buffer verbatim, so a
line reading `HP  85/100` is a callback away. What it needs is a safe way to read
the player from the profiler's background thread: `players[]` and
`dynamic_world` are engine state, and the profiler polls on its own thread rather
than in the render loop.

The cheap approach is to have the render loop copy health and oxygen into two
plain integers once a frame, and have the profiler read those. A torn read of an
int shows a wrong number for one refresh of a VMU screen, which does not matter.

Note the LCD is 48x32 pixels and fits four lines of about eleven characters, so
the title, build and FPS lines already use three of them. Health and air would
need one line between them, something like `HP 85 O2 60`, or the build line
would have to go once a level is running.
