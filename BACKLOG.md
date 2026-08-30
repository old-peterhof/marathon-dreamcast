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

## GL build: bring it up to parity with b71 — the work list

Max's list, captured 2026-08-29 for a session starting that evening. Ordered by
what is understood versus what needs investigating first.

### 0. The texture pipeline — do this FIRST, it pays for item 1

A proposal Max brought in, spot-checked against the installed GLdc 1.1.1 and
against our own code. The claims hold; where I verified something it is marked.

**Why it comes first:** item 1 wants full-resolution sprites and is constrained
by roughly 2MB of headroom at level start. These two fixes create that headroom
rather than competing with it, so the order is Fix A, then item 1, then Fix B.

**What the pipeline does now** (`Source_Files/Misc/OGL_Textures.cpp`, all
confirmed by reading it):

- `GetOGLTexture:925` — `new uint32[NumPixels]`, expand 8-bit indices through a
  256-entry uint32 CLUT
- `Shrink:1085` — a *second* `new GLuint[NumPixels]`, box-filtered through our own
  `dc/dc_glu.c`
- `PlaceTexture` — `glTexImage2D(..., GL_RGBA, GL_UNSIGNED_BYTE, ...)`, and GLdc
  packs to ARGB4444 internally

Three whole-image passes, two heap allocations, peak 4 bytes/pixel.

**Fix A — one pass, 2 bytes/pixel.** Nothing forces the intermediate to be
32-bit; it exists only because expand, shrink and pack are three separate passes.
Build a second CLUT in `FindColorTables` as 256 x uint16 ARGB4444 beside the
existing uint32 one; make `GetOGLTexture` write uint16 and fold `Shrink`'s box
filter into the same loop (read the 2x2 indices, average in 8-bit or you compound
the quantisation error, pack once); upload with
`GL_UNSIGNED_SHORT_4_4_4_4_REV_TWID_KOS` so GLdc memcpys instead of converting.
Verified: that token exists, `glkos.h:18`. One allocation, one pass, no visual
change.

**A confirmed bug to kill in the same rewrite.** `OGL_Textures.cpp:917` sets
`ColorTable[0] = 0` for transparency, and our `gluScaleImage` (`dc/dc_glu.c:75-78`)
averages R, G and B unweighted across the box — so fully-transparent black texels
drag their neighbours toward black and every sprite edge gets dark fringing.
Weight the average by alpha. Pre-existing since b36, not a regression, but the
rewrite is the moment.

**Fix B — paletted textures, 1 byte/pixel, walls only.** PVR2 does paletted in
hardware and GLdc exposes it: `GL_COLOR_INDEX8_EXT` (`glext.h:132`),
`glColorTableEXT` (`glext.h:149`), `GL_COLOR_INDEX8_TWID_KOS` for the fast
twiddled path — all verified present. Marathon's shape data is already exactly
this shape: one byte per pixel plus a 256-entry table, so rows upload close to
verbatim. 1 byte/pixel in RAM *and* in VRAM, which on 8MB of VRAM matters more
than the transient does. Filtering is not lost — the PVR looks up before it
filters.

The catch is palette banks: `MAX_GLDC_PALETTE_SLOTS` is 4 (verified,
`private.h:26`), and we would need one per (Collection, CTable) — a level uses
well over four. Hence walls and landscape on the paletted path and everything
else on Fix A's 4444 path: walls come from very few collections and are both the
bulk of VRAM and the bulk of screen area, so nearly all the win is there and it
fits the budget. Sprites and HUD are many collections but individually tiny.

If the paletted walls are mipmapped, note GLdc's paletted mip generator averages
via `_glCalculateAverageTexel` over a 1-byte stride (`framebuffer.c:230-251`) --
averaging palette *indices* is meaningless, so either skip mips there or build the
chain yourself against a 15-bit inverse CLUT (32KB per live palette, at most four).
I have not verified how badly that actually looks; check before designing around it.

**DONE (2026-08-29).** Fix A landed in two steps, and the measuring turned up
something that changes what Fix B is for.

*The alpha-bleed bug is fixed and was real.* Compiling the actual
`gluScaleImage` natively for both versions against a synthetic sprite edge
(sprite R220 G40 B40, filter box half inside the silhouette):

| | edge texel |
|---|---|
| before | `R110 G20 B20 A127` |
| after | `R220 G40 B40 A127` |

Exactly half brightness before. Alpha identical, so silhouettes are unchanged.

*The pipeline is one pass.* `GetOGLTexture` now writes the reduced image
directly from the shape's own 8-bit rows; `Shrink` is no longer called. Peak
transient per texture goes from 5 bytes per full-size pixel to 1.

*But the transient was never the constraint, and neither is the heap.* This is
the part worth carrying forward. `dc_heap_used` reads `sbrk(0)`, which only
grows, so it is already a high-water mark -- and it sits at exactly 12676 KB
through texture loading, identical before and after, and identical again with
sprites at full resolution. Uploaded textures do not live in the heap at all:
`glTexImage2D` copies them into the PVR's VRAM and the RAM buffer is freed
immediately.

**So texture resolution is a VRAM budget, not a heap budget.** The "roughly 2MB
of headroom at level start" this item was planned around is main-RAM headroom
and is not what constrains it. GLdc will tell you the real number --
`GL_FREE_TEXTURE_MEMORY_KOS` and `GL_USED_TEXTURE_MEMORY_KOS`, now traced beside
the heap figure on every 25th upload.

Measured on the same level and camera path, at 100 textures uploaded:

| | VRAM used | VRAM free |
|---|---|---|
| everything at half resolution | 2015 KB | 2943 KB |
| sprites and weapon at full | 3645 KB | 1313 KB |

The texture pool is about 4.8MB. Full-resolution sprites cost **1630 KB**, which
is most of the slack, and that is 100 textures from one vantage point in one
level -- a level with more monster variety would load more. It fits and it runs
(28-30fps, no allocation failure), but the margin is thin.

**b73 shipped this and it crashed. Reverted in b74.** Full-resolution sprites
fit at level start but not for long: textures keep loading as play continues,
and free VRAM fell to 270 KB by 125 uploads, at which point the game died to a
black screen -- on hardware after about 1.5 seconds of play, and reproducibly
under Flycast. At half resolution the same soak reached 175 uploads with VRAM
plateauing at 2606 KB free and did not crash.

The cause is established by intervention, not by mechanism: removing the VRAM
pressure removes the crash. Worth knowing that GLdc never reported
GL_OUT_OF_MEMORY (0x505) in any run, including the ones that died -- so the
failure is not simply an upload being refused, and whatever happens inside the
allocator is still unidentified. Do not assume a graceful-degradation guard
around glTexImage2D would have saved it.

**So full-resolution sprites are blocked on Fix B, not merely helped by it.**
Raise them again only after paletted walls have freed the room, and soak before
shipping a disc -- 125 uploads was enough to kill it, and a soak is cheap.

**This is what Fix B is now for.** Not to make room in RAM -- to buy back VRAM
so the full-resolution sprites are affordable. Walls are the bulk of the pool
and paletted walls halve them, which is roughly the 1.6MB the sprites cost.
That is the next piece of item 0, and it should be measured with the same
counter.

*Also found while doing this, both pre-existing:*

- The software build did not link on `gl-forward`. `screen_sdl.cpp` guarded the
  GL polygon counters on `#ifdef DC`, but they are defined in `OGL_Render.cpp`,
  which compiles to nothing without `HAVE_OPENGL`. Fixed to
  `#if defined(DC) && defined(HAVE_OPENGL)`.
- `GetFakeLandscape` and `LoadSubstituteTexture` return full-size buffers and are
  never reduced, but `PlaceTexture` uploads `LoadedWidth x LoadedHeight` from
  them -- so those two paths upload at the wrong stride. Harmless today because
  the landscape is a flat colour, but it is worth knowing before item 2 turns
  the sky back into a texture.

### 1. Per-texture-type resolution — motion tracker blur, sprite sharpness

**This is the well-understood one and should go first.** `OGL_Setup.cpp:146`
loops over every texture type and sets `Resolution = 1` (half) for all of them.
But `TxtrConfigList` is indexed per type — `OGL_Txtr_Wall`,
`OGL_Txtr_Landscape`, `OGL_Txtr_Inhabitant`, `OGL_Txtr_WeaponsInHand` — so the
resolution can differ per type, which is exactly what is wanted: sprites and the
weapon at full resolution, walls left at half.

Watch the memory: half resolution is one of the four things b36 did to make the
GL build fit in 16MB, so raising it costs real space. `dc_heap_trace` reports the
headroom; there was about 2MB free at level start. Raise sprites first, measure,
then decide about the rest.

Note there is no `OGL_Txtr_Interface` in that enum. The motion tracker is drawn
by `HUDRenderer_OGL.cpp` from the interface collection (collection 0, which
`FindColorTables` special-cases to use the CLUT directly), so find out which type
it actually resolves to before assuming this fixes the blur.

### 2. Sky textures are flat colours

`OGL_Flag_FlatLand` is set in the Dreamcast defaults. b36 set it because a real
landscape is a single 1024x512 texture and converting it costs a 2MB
intermediate — the allocation that fell off the end of the machine. So this is a
memory problem, not a rendering one, and the fix is to convert the sky without
the 2MB buffer (stream it, or convert in place) rather than to clear the flag and
hope. `GetFakeLandscape()` is what currently paints the flat colours.

### 3. Terminal screens do not render

Terminals go through the software path: `screen_sdl.cpp` only calls
`update_screen` under GL when `world_view->terminal_mode_active` or the overhead
map is up. That path needs a 2D surface, which the GL configuration does not
have — so this is the same underlying problem as the interface, below, and
probably wants solving once for both.

### 4. Controller-friendly menus and the pause menu

The b57-b67 interface layer. **Blocked on the GL/UI surface tension recorded in
BUGS.md**, not on the porting work: the PowerVR path wants no 2D surface and the
interface needs one. Options are written up there. Do not start this before that
decision is settled, or it will be the magenta hunt again.

Everything else from those builds that does *not* need a surface is already on
`gl-forward` — see the step commits.

### 5. Load times

Builds before b71 cut them substantially. Find the commits
(`git log dc-rebuild --oneline` around the relevant builds) and check what they
touched; if the changes are in wad reading or collection loading they are
renderer-independent and can come across cleanly. README.DC.md's "Load times"
section has the per-stage measurements to compare against — chapter screen
22.5s, `load_collections` 7.7s, monster sounds 13.7s.

### 6. Shadows and lighting do not match the software renderer

Least understood of the list; investigate before planning. Aleph One's GL
renderer shades per-vertex through `FindShadingColor` into
`ExtendedVertexList[].Color`, which is a different model from the software
renderer's shading tables, so some difference is expected — the question is how
much of what Max is seeing is that versus something wrong. Note the shim
synthesises a vertex colour array when the game does not supply one
(`dc_gl_compat.h`), because GLdc fills a disabled colour attribute with white;
that is worth checking first.

### Method

The bisect-from-working discipline in the plan file applies to all of it: one
change per step, look at every build with `tools/shoot-flycast.sh`, and use
`./build.sh GL=1 play` when a human needs to drive.

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
