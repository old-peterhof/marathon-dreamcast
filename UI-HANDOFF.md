# Controller-native UI — decisions and handoff

Answers `UI-BRIEF.md`. Uses the audit in `MENU-TREE.md` as the source for what
each screen contains. Everything here is settled unless it appears under
[Open questions](#open-questions).

The design exists as a working prototype, not a picture:

    mockups/prototype/index.html      open it; the pad is on the keyboard
    mockups/prototype/build.py        screen content — edit here, then re-run
    mockups/prototype/app.css         design tokens and every widget style
    mockups/prototype/app.js          navigation, the four pad operations
    mockups/explorations/             superseded; kept for the reasoning trail

Rebuild after editing content: `cd mockups/prototype && python3 build.py`.

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
| plus 16-bit dithering | 7.82 | 4.88 |

Three rules follow.

**Never draw a 1px horizontal.** Every rule is a 2px core with 1px flanks at 35%
opacity above and below, so it lands on both fields. `.hr` in `app.css` is the
one implementation; panel tops, bottoms and caption dividers use 2px for the
same reason. Vertical 1px lines are fine.

**Do not dither.** Per-pixel noise makes every row differ from its neighbours,
which is exactly what interlace turns into shimmer — it made every band worse.
The 16-bit banding it would have fixed is an LCD problem; a CRT's own blur and
the composite path smear it away.

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
| item | `#6f8a7e` | `ITEM_COLOR` |
| item, active | `#ffc000` | `ITEM_ACTIVE_COLOR` |
| selection bar | `#2a1e04` | — |
| label | `#4e6459` | `LABEL_COLOR` |
| title | `#e2efe8` | `TITLE_COLOR` |
| terminal green | `#7bd98f` | — |

Geometry: 640×480, **40px clear on every edge**, so nothing readable leaves the
560×400 centre. Menu rows are 34px, dense lists 26–30px.

**One warm colour.** `#ffc000` is the glow's own value, and it does two jobs and
no others: the brand mark, and "you are here." Selection is signalled three
ways at once — amber text, a dim amber bar, and a caret — because one signal at
15fps on a composite TV is not enough.

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

**Difficulty is not in Preferences.** It is chosen when a game starts. A
consequence worth deciding on: there is now no way to change it mid-campaign.

**Save Game generates a name** from the level, since a pad cannot type. Free slot
means it writes and confirms; four full slots route to the saves list in
overwrite mode with a banner, because the only decision left is which one to
spend.

**Start is reserved and always backs out**, per `MENU-TREE` §2. In game it opens
the pause menu; in every menu it cancels.

**Left/Right adjusts in place.** No sub-dialogs for a value — a pad has nowhere
good to put one. On the binding screen there is nothing to adjust, so the same
axis moves between the two columns.

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

1. **A static plate bitmap at 640×480.** Gradient, seams, watermark and wordmark
   bake into one image; the menu draws text over it. The main menu's painted
   buttons go away with it, which is what lets the item list change without new
   artwork.
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

## Open questions

1. **`#ffc000` on composite.** It computes to roughly 116 IRE peak — legal-ish,
   but hot enough to bloom slightly on a consumer set. `#f5be2e` is visually
   near-identical at about 107 IRE. Worth a look on Max's TV before it is fixed
   in the theme.
2. **The terminal text is a pastiche, not Bungie's.** It reads as if it came from
   the game and it did not. Either pull a real passage from the scenario data on
   the disc, replace it with obviously greeked text, or label it. It should not
   be screenshotted as-is.
3. **Changing difficulty mid-campaign.** If that should be possible, the row
   belongs on the pause menu, where it reads as changing the run in progress
   rather than a global setting.
4. **Preferences on the pause menu** currently opens the same screen the main
   menu does. Some settings there are stranger mid-level than others.
5. **Analog stick vs. D-pad in menus.** The prototype treats them alike. On
   hardware the stick may want a repeat delay the D-pad does not.
