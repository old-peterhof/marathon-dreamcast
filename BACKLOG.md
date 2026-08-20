# Backlog

Ordered roughly by appetite rather than difficulty. Completed sections are kept
rather than deleted, because the reasoning in them is usually the part worth
having later — and because it is useful to see which predictions held.

**Still open:** rumble pack (blocked on hardware), the PowerVR renderer (works,
unmeasured on a console), native keyboard and mouse (present, never tested), and
shrinking a save below 10 blocks (marginal).

**Done:** the controller-native UI, the console trim, Falco's flags, and health
and air on the VMU.

## Rumble pack support

Force feedback for gun shots and for taking hits. Wanted explicitly.

Notes for whoever picks it up:

- KOS has a driver at `dc/maple/purupuru.h`. It is left **enabled** in this
  port precisely so this stays possible — see `dc/dc_maple.c`, where it would
  have been the blunter fix for the boot hang and was deliberately not used.
- **The boot hang is fixed, confirmed on hardware in b68.** It was KOS's
  `maple_wait_scan()` waiting forever for all four ports while the pack made one
  never report. `dc/dc_maple.c` now emits KOS's init-flag list with that one
  entry dropped and waits on the same condition with a deadline instead. See
  BUGS.md for why the earlier "not available to us" conclusion was wrong.
- **So this is unblocked.** Trying an official pack is
  still worth doing, but for a different reason than before: b68 stops the hang,
  it does not make a third-party pack answer enumeration, so rumble may still
  not work on that unit even with a booting game.
- The natural hook points are the same places the game already makes noise:
  weapon fire in `weapons.cpp` and damage in `player.cpp`.

## Native keyboard and mouse

**Nothing was stripped.** That is the point of this entry: it started as "we
should not have removed this" and the answer is that nobody did, so the work is
to test what is already compiled in rather than to restore it.

What is in the b67 elf, checked with `nm`:

- KOS's keyboard driver — `kbd_init`, `kbd_drv`, `kbd_attach`, `kbd_periodic`
- KOS's mouse driver
- SDL's Dreamcast event pump, `SDL_dcevents.c`

That event pump polls `MAPLE_FUNC_MOUSE` and `MAPLE_FUNC_KEYBOARD` **and nothing
else**, which is the reason `dc/dc_input.c` exists at all: SDL never reads
`MAPLE_FUNC_CONTROLLER`, so the pad had to be built on top of a driver that
already understood the two peripherals nobody has.

So a real keyboard or mouse should already produce SDL events and drive the
dialogs, which were written for exactly that. "Should" is carrying weight there —
neither has ever been plugged in.

**Why it is worth more than a curiosity.** A keyboard drives `w_text_entry`, the
one widget a pad cannot work, and naming a save is the single thing the four-slot
generated-name design exists to work around. Generated names stay the default,
because almost nobody has a Dreamcast keyboard, but a keyboard on the bus at the
save screen could offer a typed name.

Order of work:

1. **Plug both in and look.** No code first. This may already work.
2. If the keyboard works, offer a typed save name when one is present, leaving the
   generated name as the default and the pad path untouched.
3. **Check for interference.** `dc_input.c` injects synthetic keys through
   `SDL_PrivateKeyboard`, and `dc_input_set_ingame()` releases every key on a
   context switch — which would stomp a real key held across a level change.
   Harmless in theory, unverified in practice.
4. **The mouse is the awkward one.** The analog stick already feeds
   `delta_yaw`/`delta_pitch` through `mouse_sdl.cpp`'s analog path. A real mouse
   arriving as SDL motion events may add to that or fight it.

One caution before any of it: a third-party rumble pack hangs the boot because
KOS's maple scan waits forever for all four ports (see BUGS.md). A keyboard or
mouse that answers enumeration badly could do the same. Test one device at a
time, and know that pulling it out releases the hang.

## Controller-native UI — DONE, b58-b61

Built over five phases. `UI-HANDOFF.md` is the design, `MENU-TREE.md` is the
audit it was built against, and README.DC.md's "The interface" section is the
summary.

The question this section asked -- reskin the existing widgets, or a parallel
controller-first set -- was answered in the middle, and the middle turned out to
be right. The widget set was kept and extended by four widgets (`w_pad_key`,
`w_explain`, `w_save_slot`, `w_pad_grid`), while the main menu, which was the one
part genuinely built around a pointer, was replaced outright.

Each of the five problems listed here was real and each was fixed:

- **2D layout driven by a 1D list.** The menu is now drawn text over a plate, so
  the list *is* the layout and cannot disagree with it.
- **Lists trap focus.** Still true of `w_list`, and the reason the new save
  screens are built from buttons instead. `w_pad_grid` releases focus at its
  edges rather than swallowing everything, which is the pattern to copy.
- **Inconsistent navigation.** Left/Right adjust in place everywhere, which
  turned out to be behaviour `w_select` and `w_slider` already had.
- **No consistent focus indicator.** Selection now reads three ways at once -- a
  bar, amber text, and a caret -- because one cue is not enough at 15fps on a
  composite television.
- **The look is data-driven.** Taken advantage of: the palette went in as theme
  colours, and the plate is baked from the design prototype's own CSS by
  `tools/bake-plate.py`.

The artwork objection at the end of the next section -- that the menu is a fixed
bitmap with the buttons drawn into it -- is what the plate removed, and is why
the trimming below could finally happen.

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

## Which preferences and menu items to strip on console — DONE, b58-b59

Discussed and actioned. Everything on the candidate list went: both network
items, all three film items, Quit, the whole Environment screen, four of six
graphics settings, Channels, the player name and both colours, and CONFIGURE
KEYBOARD. The main menu went from ten items to five and Preferences from five
buttons to three rows.

Nothing was deleted from the engine to do it -- `iManageSaves` is appended to the
enum so no id shifts, and the rectangle table is untouched, so non-DC builds are
unchanged. `MENU-TREE.md` records what went and why, including the reasoning for
the items that stayed.

The objection recorded here was correct and was the blocker: the menu was a fixed
bitmap with the buttons drawn into it. Replacing it with a drawn menu is what
made this possible, which is why the two were done together.

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

## Player health and air on the VMU — DONE, b61

The VMU shows MARATHON 2 / Build N / FPS / HP150 A100.

Built the cheap way this section predicted: the render loop copies health and
oxygen into two plain integers once a frame and the profiler reads those, so
nothing reaches into engine state from the profiler's own thread.

Two details the note did not anticipate. Health is shown raw rather than out of
100, because Marathon's own scale is the meaningful one -- 100 is a full normal
suit and the 150 ceiling is what canisters add. And the readout has to be cleared
when the main menu is drawn: `render_screen()` is what feeds it and does not run
there, so it would otherwise keep showing whatever the player's condition was
when they left the level, which reads as a live number and is not one.
