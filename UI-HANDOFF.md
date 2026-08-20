# Controller-native UI — decisions and handoff

Answers `UI-BRIEF.md`. Uses the audit in `MENU-TREE.md` as the source for what
each screen contains. Everything here is settled — see [Settled](#settled) for
the decisions that were argued over and closed.

The design exists as a working prototype, not a picture:

    mockups/prototype/index.html      open it; the pad is on the keyboard
    mockups/prototype/strings.md      every word the player reads — edit copy here
    mockups/prototype/build.py        layout; reads strings.md, writes index.html
    mockups/prototype/app.css         design tokens and every widget style
    mockups/prototype/app.js          navigation, the four pad operations
    mockups/prototype/check.py        checks every screen against the rules below
    mockups/prototype/check.js        the DOM audit check.py injects
    mockups/prototype/shots.py        renders screens to PNG, alone on the page
    mockups/explorations/             superseded; kept for the reasoning trail

After editing copy or layout:

    cd mockups/prototype && python3 build.py && python3 check.py

`check.py` is the gate. It fails on a one-pixel horizontal, anything drawn
outside the 560x400 safe area, an icon under 60px, two labels sharing pixels, and
text overflowing its own box. A deliberate CSS exception takes a trailing
`/* 480i-ok */`. `shots.py --sheet --overscan <screens>` stacks renders into one
PNG for eyeballing what a TV actually shows.

Keys: arrows or WASD move, `X`/Enter is A, `Z`/Esc is B, `C` is X, `V` is Y,
`Tab` is Start. Clicking a row also works. `?screen=controller` deep-links.
Six toolbar overlays: safe area, overscan, scanlines, **interlace 480i**,
widget map, 2×.

---

## 1. The finding that shapes everything

`screen_sdl.cpp:133` asks for 640×480 and `dc_input.c:171` forces 60Hz NTSC, so
on a CRT the output is **480i, interlaced**. A one-pixel horizontal line exists
on one field only, is drawn 30 times a second, and buzzes. Vertical lines are
unaffected.

Measured on the plate, as flicker energy on the rows a horizontal rule occupies:

| | upper rule | lower rule |
|---|---|---|
| 1px rules | 8.70 | 5.63 |
| 2px core + dim flanks | **6.05** | **3.79** |
| plus per-pixel noise dithering | 7.82 | 4.88 |

Three rules follow.

**Never draw a 1px horizontal.** Every rule is a 2px core with 1px flanks at 35%
opacity above and below, so it lands on both fields. `.hr` in `app.css` is the
one implementation; panel tops, bottoms and caption dividers use 2px for the
same reason. Vertical 1px lines are fine.

**Do not dither.** Per-pixel noise makes every row differ from its neighbours,
which is what interlace turns into shimmer: +33% twitter, measured. A pattern
that varies along a row and repeats identically on every row is interlace-safe,
and was baked for a while on that basis — but measured where banding actually
shows, a flat patch of the gradient, it bought one extra colour out of seven and
raised the error doing it. The whole-image colour count that made it look
worthwhile was dominated by the wordmark, not the gradient. `plate.py` truncates.

**What 16-bit actually costs, by region.** The background is not the problem; the
soft amber ramps are. Amber has a near-zero blue channel and red is only 5 bits,
so a wide amber gradient has almost nothing to spend:

| region | colours, 24-bit → 16-bit |
|---|---|
| bloom halo above the letters | 643 → 90 |
| bloom halo left of the M | 821 → 141 |
| watermark soft edge | 920 → 149 |
| background gradient | 39 → 7 |

The background's bands are one green step apart, roughly 1.6% luminance, at or
below the threshold of notice. The bloom is what visibly breaks up. If it needs
fixing the answer is a shorter ramp, not a dither — but judge it on the CRT
first, because composite low-passes chroma hard and this degradation is almost
entirely chroma.

**Never bake the scanline overlay into artwork.** It is a preview toggle in the
prototype. Baked scanlines against real scanlines is textbook moiré.

The 80px vertical seam grid on the plate is far too coarse to beat against a
shadow mask, so it is safe.

---

## 2. Tokens

Defined once at the top of `app.css`, named to match `sdl_dialogs.h`.

| Token | Value | `sdl_dialogs.h` |
|---|---|---|
| background | `#05080a` | `BACKGROUND_COLOR` |
| panel fill | `#101a1b` | — |
| rule | `#2d4640` | — |
| rule, active | `#4d6f63` | — |
| item | `#81a092` | `ITEM_COLOR` |
| item, active | `#ffc000` | `ITEM_ACTIVE_COLOR` |
| selection bar | `#2a1e04` | — |
| label | `#6b897a` | `LABEL_COLOR` |
| title | `#e2efe8` | `TITLE_COLOR` |
| terminal green | `#7bd98f` | — |

Geometry: 640×480, **40px clear on every edge**, so nothing readable leaves the
560×400 centre. Menu rows are 34px, dense lists 26–30px.

**One warm colour.** `#ffc000` is the glow's own value, and it does two jobs and
no others: the brand mark, and "you are here." Selection is signalled three
ways at once — amber text, a dim amber bar, and a caret — because one signal at
15fps on a composite TV is not enough.

**Every pair clears a contrast floor**, 4.5:1 for small text and 3.0:1 at 17px
and above, measured and enforced by `check.py`. `--item` and `--label` were
originally 4.73:1 and 2.78:1; the label carried the hint bar, the explainer line,
the modal notes, the panel captions and every row value, all at 11px, and would
have gone first on a composite CRT at couch distance. Both were lifted along
their own hue, keeping a 1.42× luminance step between them and 1.85× from item up
to amber, so the label-versus-value hierarchy survives.

---

## 3. Brand art

Both assets are vectors and are inlined by `build.py`, not linked.

**Wordmark** (`~/Downloads/Marathon2.svg`) is four stacked layers: a blurred
amber bloom, solid amber letterforms, **black** letterforms inset 2px on top,
and DURANDAL in grey. The black face means the file is built for a light ground.
On the hull plate the face is flipped to `--face` and the 2px amber offset reads
as a rim light instead of a shadow. Bloom is `feGaussianBlur stdDeviation="3.0"`
in the wordmark's own units, about 5.5px at final scale — close to the blur in
the original raster. Drawn at 380×102 at (40,46).

**Icon** (`~/Downloads/Marathon_Logo.svg`) is a negative-space mark: the ring gap
is **5.03% of its width**. Below about 60px that gap falls under 3px and a CRT
closes it into a lozenge. **This rules the icon out as a row cursor** — rows are
34px. It appears once, as a plate watermark: 430px at (302,52), 11% opacity,
bled off the right edge. It bakes into the static plate, so it is free at
runtime, and overscan eating the right 40px does not matter for a mark meant to
run off.

---

## 4. Screens

Ten, all navigable in the prototype.

| Screen | Contents | Notes |
|---|---|---|
| `main` | New Game · Continue Game · Manage Saves · Preferences · Credits | Wordmark, watermark, last-save column |
| `difficulty` | Kindergarten · Easy · Normal · Major Damage · Total Carnage | Bungie's names, nothing added |
| `saves` | Four slots: name, elapsed, VMU blocks | A loads, X deletes, both confirm |
| `prefs` | Brightness · Sound → · Controls → | `MENU-TREE` 5.2 leaves one graphics row |
| `prefs-sound` | Volume, Quality, Stereo, Active Panning, Ambient, More Sounds | 5.4 minus Channels |
| `prefs-controls` | Analog Stick, both sensitivities, Invert, Always Run/Swim, Auto-Switch, Configure Controller → | 5.5 minus Mouse Control |
| `controller` | 13 + 7 bindings over two pages, two columns | X flips page, Y defaults, A captures |
| `credits` | Scrolling | |
| `term` | Full-screen terminal | Stands in for being in the game |
| `pause` | Resume · Save Game · Preferences · Quit | Draws over the running game |

**Difficulty is chosen on New Game and fixed for the run.** It is deliberately
not in Preferences. `get_difficulty_level()`, which reads the preference, is
called at exactly two places — `interface.cpp:419` on a save load and
`interface.cpp:1394` on a new game — and `goto_level` does not re-read it, so a
Preferences row could only ever have applied to the next new game or load. That
was decided against rather than worked around, so nothing in the interface offers
a change it cannot honour.

**Nothing is hidden in Preferences when it is opened from the pause menu.** Every
setting is visible; the ones that do not take effect immediately say so in their
explainer line rather than being removed. Difficulty is the confirmed case.
Sound Quality and More Sounds are very likely load-time too, because both change
what gets loaded with a level — worth confirming before their explainers claim
it.

**Save Game generates a name** from the level, since a pad cannot type. Free slot
means it writes and confirms; four full slots route to the saves list in
overwrite mode with a banner, because the only decision left is which one to
spend.

**Start is reserved and always backs out**, per `MENU-TREE` §2. In game it opens
the pause menu; in every menu it cancels.

**Left/Right adjusts in place.** No sub-dialogs for a value — a pad has nowhere
good to put one. On the binding screen there is nothing to adjust, so the same
axis moves between the two columns.

**A held direction repeats on the interface's own clock**, not the host's. A pad
has no operating system behind it deciding when a held direction repeats, so the
menu has to. Only directions repeat — a held A must never fire twice, a held
Start must never back out twice, and a held button inside binding capture must
name one binding rather than a stream. Numbers for `dc_input.c`:

| | lead-in | then every |
|---|---|---|
| D-pad | 400ms | 120ms |
| Analog stick | 500ms | 150ms |

The stick gets the longer lead-in because it is easy to over-push and hard to
nudge exactly once; the D-pad gives a clean discrete press. Both sets are
confirmed on hardware. The prototype runs the D-pad numbers, since a keyboard
cannot tell the two apart.

**Every settings row carries one line of plain English** below the panel,
explaining whatever is focused. More Sounds says it costs about 7.5 seconds a
level load. Look Sensitivity says why it is separate from Turn.

**The button shortfall is on screen, not hidden.** The Configure Controller panel
header reads `10 OF 10 BUTTONS SPENT`, amber when full, and binding a taken
button steals it from whoever had it.

---

## 5. What the engine needs

Widgets are the existing set, and the prototype tags each element with the one
it becomes — turn on **widget map** to see them. Five things do not exist yet.

1. **The static plate bitmaps** — baked, in `mockups/prototype/assets/`.
   `menu-plate.png` is the gradient and hull seams, shared by every screen;
   `main-plate.png` is the same plus the wordmark and watermark, for the main
   menu only. Each is a 600K surface in RGB565. Nothing dynamic is baked in: no
   rules, no panels, no text, so a layout change never needs new art, and the
   main menu's painted buttons go away entirely. `plate.py` regenerates both
   alongside `-565.png` previews. What remains is loading them and blitting.
2. **A most-recent-save lookup.** Continue Game needs to know which save to open.
   Nothing does that today.
3. **A selection-change callback.** The explainer line is a `w_static_text` that
   rewrites whenever the highlight moves. Worth checking whether
   `sdl_dialogs.cpp` can notify on selection change, or whether the dialog loop
   needs a hook.
4. **Two-column navigation** on the binding screen. `w_list` is vertical; the
   13-row page needs Left/Right to jump columns. A 13-row single column does not
   fit in 400px at a size readable across a room.
5. **The parked pause menu**, from `measurements-b31`.

Already present: `w_pad_key` and the capture contract (b57), and the fixed menu
binding table that guarantees nobody can strand themselves.

Still outstanding from `MENU-TREE` §7: the Return-in-list fix, without which the
save and load lists cannot be driven by pad at all.

---

## Settled

- **The terminals are never touched.** Original graphics, original text, no
  paging work, no restyling. Not an open question and not a defect to revisit.
- **Amber is `#ffc000`**, one warm colour doing two jobs. It peaks near 116 IRE
  on NTSC composite, above typical broadcast practice and under the 120 ceiling,
  which at worst means the wordmark blooms very slightly. Not worth trading the
  source art's own value for.
- **Four save slots**, auto-named from the level.
- **Difficulty is set on New Game only** and fixed for the run. The engine
  latches it at new game and save load and never re-reads it, and rather than
  work around that the interface simply does not offer the change.
- **Preferences hides nothing** when opened mid-game. Settings that do not take
  effect immediately say so in their explainer instead of disappearing.
- **Menu repeat: D-pad 400ms then 120ms, analog stick 500ms then 150ms.**
  Confirmed on hardware.
- **The wordmark's bloom stays as authored.** It loses more to the 16-bit
  framebuffer than anything else on screen — 643 distinct colours down to 90 in
  the halo above the letters — but it read well on a CRT on hardware, which is
  the only instrument that counts for a chroma artefact over composite.

