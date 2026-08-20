# Overnight log — interface finishing touches

Newest first. Each entry says what changed and how it was verified.

---

## Session 5

**The overlays had never been checked.** `check.py` skipped `.modal:not(.up)` and
`.capture:not(.up)`, so the four confirmation dialogs and the binding overlay had
no geometry checking at all. Now audited as five extra states —
`modal:delete`, `modal:overwrite`, `modal:quit`, `modal:save`,
`overlay:capture` — each opened through the same call the pad would make, and
each handed the longest content it can ever hold, so a name that only just fits
is caught here rather than on a TV. Sixteen states in total now pass.

Two checker corrections that fell out of it:

- **The collision pass flagged overlays covering the rows behind them**, which is
  their job. It now compares only labels that share a layer.
- **`.modal` and `.capture` scrims are full-bleed** like `.plate`, so they are
  marked `data-bleed`.

`shots.py` can render the overlay states too, so they appear in the contact sheet.

**Then the renders showed something no geometry check can judge, and it was the
real find of the night.** `--label` was **2.78:1** at 11px — and `--label` carries
the hint bar, the explainer line, the modal notes, the panel captions and every
row value. That is most of the secondary information in the interface, all of it
small, all of it well under a 4.5:1 floor, and all of it the first thing to go on
a composite CRT at couch distance.

Measured every pair the design actually uses, then lifted the two that failed
along their own hue:

| | was | now | on panel |
|---|---|---|---|
| `--item` | `#6f8a7e` | `#81a092` | 4.73 → **6.22** |
| `--label` | `#4e6459` | `#6b897a` | 2.78 → **4.63** |
| unbound dash / disabled row | `#3d4c46` | `#546961` | 1.96 → **3.01** |

Both had to move. Raising only the label would have put it within 0.1 of item,
and the two sit side by side in four places — a row's label against its value, a
caption against the rows under it, the hint glyph against its word, the state
column against its figures — where the difference carries the meaning. The new
pair keeps a 1.42× luminance step, and 1.85× from item up to amber, so the
selected row still wins.

**Contrast is now a permanent check.** `check.py` reads the tokens straight out of
`app.css` and measures eleven pairs against 4.5:1, or 3.0:1 at 17px and above, so
this cannot drift back.

The plates did not need re-baking — they carry only the gradient, seams, wordmark
and watermark, none of which use either token.

**Verified.** `check.py` clean: contrast, one-pixel horizontals, and all sixteen
screen and overlay states. Rendered Controls and the quit dialog to confirm the
hierarchy still reads.

**Next:** nothing I can do without a decision from Max. Open questions 1 and 2 need
his eye and his call; 3 is a judgement about the pause menu; 4 needs a real pad.

## Session 4

**Held directions now repeat on the interface's own clock.** This started as open
question 4 and turned out to be a defect. The prototype had been inheriting the
host's keyboard repeat, which is whatever is set in System Settings — and nothing
at all when key repeat is off, so holding a direction did nothing on some
machines. A pad has no operating system behind it making that decision, so the
menu has to.

Only directions repeat. Rules the implementation holds to:

- a held A never fires twice, and a held Start never backs out twice
- a held button inside binding capture names one binding, not a stream, and does
  not move the cursor underneath it
- the host's own auto-repeat events are discarded
- releasing, or the window losing focus, stops it

Numbers, now in `UI-HANDOFF.md` §4 for `dc_input.c`: D-pad 400ms then 120ms;
analog stick 500ms then 150ms. The stick gets the longer lead-in because it is
easy to over-push and hard to nudge exactly once. The prototype runs the D-pad
numbers, since a keyboard cannot tell the two apart — which is what open question
4 now asks someone to check with a real pad.

**Verified** under virtual time:

| | result | expected |
|---|---|---|
| hold Down 1000ms | 7 moves | 7 — one at 0, then every 120 after 400 |
| 600ms after release | 0 more | 0 |
| tap Down, 200ms | 1 move | 1 |
| host auto-repeat events | 1 move | 1 |
| hold A on a slider | one step | one step |
| hold Right on a slider | repeats to the cap | repeats |
| hold D-UP inside capture | bound once, cursor unmoved | bound once |

`check.py` clean on all eleven screens.

**Next:** open question 3 — whether Preferences opened from the pause menu should
hide anything. That one needs a decision from Max more than it needs code, so
unless something else surfaces this is a good place for the loop to rest.

## Session 3

**Contact sheet.** `shots.py` now tiles into a grid:

    python3 shots.py --sheet --cols=3 --overscan

All eleven screens in one image at `mockups/prototype/shots/sheet.png`, with the
40px a TV eats dimmed. Seeing them side by side turned up three things the
geometry checks cannot judge.

**Panels started at four different heights.** 116, 128, 140 and 150 across the
set, so the eye had to re-find the list on every screen. All content panels now
hang from the same line at `top:128px`.

**Three panel captions repeated their own screen title.** Sound lost its one
last night; Preferences and Controls were still doing it. Both dropped, and the
two dead keys removed from `strings.md`. Saves, New Game and Configure
Controller keep theirs, because `SAVED GAMES`, `DIFFICULTY` and `ACTIONS` say
something the title does not.

**The button counter was amber.** `10 OF 10 BUTTONS SPENT` sat in the Configure
Controller header in `--hot`, which put a second amber object on screen
permanently and competed with the selected row. Amber is the brand mark and "you
are here", and a counter is neither. Now `--item`, with the dead `.full` toggle
removed from `app.js`.

**Resolved from last session:** the watermark. Alone on the baked plate the 11%
amber reads heavy, but in the finished main menu the panel and the wordmark pull
attention back and it sits where it should. No change.

**Verified.** `check.py` clean on all eleven. Re-sheeted the six screens the
alignment change touched.

**Next:** nothing on the punch list. Candidates, none urgent — the analog stick
repeat delay in open question 4, and a look at whether Preferences opened from
the pause menu should hide anything.

## Session 2

**Your `strings.md` edits are in.** Rebuilt, all eleven screens still pass. One
thing to fix by hand or tell me to: `controls.configure.note` reads
**"Self-explantory."** — missing the second `a`.

Two notes on your edits, neither a problem:

- `2ND TRIGGER` → `ALT TRIGGER` and `AUTO MAP` → `MAP` change display labels
  only. The engine action names in `MENU-TREE` 6.1 are unchanged, so the mapping
  still holds.
- **`controller.actions.main` and `.adv` are position-coupled** to the default
  bindings in `build.py`. Renaming a line is safe; reordering or inserting one
  silently attaches defaults to the wrong actions. Warning added to the file.

**Baked the static plates.** `plate.py` writes into `assets/`:

| | contents | PNG | RGB565 surface |
|---|---|---|---|
| `menu-plate.png` | gradient + hull seams, every screen | 98K | 600K |
| `main-plate.png` | the same plus wordmark and watermark | 130K | 600K |

Nothing dynamic is baked in — no rules, no panels, no text — so a layout change
never needs new art. That is what retires the main menu's painted buttons.

**Corrected last night's dither rule; I had drawn it too broadly.** I wrote "do
not dither", from noise dithering measuring +33% twitter. The real variable is
whether the two interlace fields see the same pixels. A pattern that varies
along a row and repeats identically on every row costs nothing:

| | distinct colours | twitter | vertical striping |
|---|---|---|---|
| no dither | 421 | 0.541 | 0.871 |
| per-pixel noise | 523 | 0.717 | — |
| column, ±1 | **444** | **0.517** | **0.931** |
| column, ±2 | 471 | 0.496 | 3.175 |
| column, ±3 | 497 | 0.502 | 3.683 |

±1 is free — slightly less twitter than not dithering at all, no measurable
striping. ±2 shows as vertical corduroy, 3.6× the striping, and that is visible
on the VGA output where no interlace hides it. `plate.py` bakes at ±1, and
`UI-HANDOFF.md` §1 now says this instead of "do not dither".

**Worth your eye:** `assets/main-plate-565.png`. Alone on the plate the 11%
watermark reads heavier than it did in the review tiles — the amber survives
quantising better than the gradient around it does. In the finished screen the
panel and menu text pull attention back, so this may be nothing. Not changing it
without you.

**Verified.** `check.py` clean on all eleven screens. Full navigation driven
through the real event path: every main-menu entry and its back-out, brightness
cycling, the sound screen, and the whole controller screen — page flip, column
jump, capture, button stealing, defaults restore, and Start unwinding three
levels to the main menu.

**Next:** a contact sheet of all eleven screens under the overscan crop, so you
can review the set in one image.

## Session 1

**Built a rules checker.** `mockups/prototype/check.py` plus `check.js`. Two
passes: a static one over `app.css` for one-pixel horizontals, and a live one in
headless Chrome that walks the real DOM of all ten screens (eleven, counting the
controller's ADVANCED page). It fails on

- anything painted outside the 560×400 safe area
- a one-scanline horizontal wider than 20px
- the Marathon icon under 60px
- two labels sharing pixels
- text overflowing its own box

A deliberate CSS exception takes a trailing `/* 480i-ok */`. Exit status is 1 on
failure, so it can gate anything.

**Six real defects it found, all fixed.**

1. **Every panel was 2px too wide.** `width:560px` plus two 1px borders in
   content-box made 562, so every 560-wide panel overhung the safe area.
   `box-sizing:border-box` on `.panel`. It also aligned panels with the rules
   above them, which had been 2px off all along.
2. **`.sec` drew a 1px horizontal** — my own 480i rule, broken by me. Now 2px.
3. **The Controls panel grew into the explainer line by 4px.** Eight rows plus
   caption plus borders came to 248, not the 244 I had counted. Panel moved from
   `top:128px` to `top:116px`.
4. **`.glyph` was a fixed 15px box**, so the multi-character START pill on the
   pause screen spilled out of it. Now `min-width:15px` with padding.
5. **The Sound panel caption repeated the screen title.** Dropped.
6. **`data-bleed` was inherited by descendants**, so marking a full-bleed
   backdrop quietly exempted every label inside it from checking. Split into
   `data-bleed` (this element) and `data-bleed-all` (subtree).

**All ten screens pass.** Verified with `check.py`; three rendered under the
overscan crop with `shots.py --sheet --overscan`.

**Copy moved out of code.** `mockups/prototype/strings.md` now holds all 122
player-visible strings under stable `###` keys. `build.py` reads it and fails
loudly on a missing key; `app.js` reads the same strings through `window.T`, so
the confirmation dialogs are editable too. Round-trip proved by editing two
strings, rebuilding, confirming both reached the page, and reverting.

**Also added** `shots.py`, which renders any screen alone on the page at 640×480,
optionally dimming the 40px a TV eats, optionally stacking several into one sheet.

**Reverted.** I touched the terminal twice — a footer overlap fix and pulling its
text into `strings.md`. Both reverted; the terminal markup is byte-for-byte what
it was, and it is out of `strings.md` entirely. Recorded under Settled in
`UI-HANDOFF.md` so it stops resurfacing.

One thing kept that touches it indirectly: the `.glyph` fix in defect 4. The
defect was on the pause screen, but `.glyph` is shared, so the terminal's START
pill is now a few pixels wider. Say the word and I will scope it away.

**Next:** bake the 640×480 static plate PNG — item 1 on the engine's list in
`UI-HANDOFF.md`.
