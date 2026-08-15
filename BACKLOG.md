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

Would buy real frames — hardware runs about 20fps software-rendered. Blocked on
GLdc lacking clip planes, which `OGL_Render.cpp` uses five of for portal and
liquid-surface clipping. See the note in `Makefile.dc` under `GL=1`, which
records the full list of missing entry points.

## sh4zam store-queue blit — DONE in b24, effect unmeasured

Implemented. `dc/dc_blit.c` uses `shz_sq_memcpy32`, falling back to `memcpy`
when alignment or length does not qualify.

Two things worth knowing for next time. It is **not** header-only: the symbol
lives in hand-written assembly (`source/sh4/shz_mem_sh4.s`), vendored under
`dc/vendor/sh4zam_src` because the kos-port needs cmake, which is not installed.
And sh4zam's headers need C++11 and use asm string forms `-std=gnu++98` cannot
parse, so the blit had to move into a C file — the rest of the port is pinned to
gnu++98 for the 2002 code.

Still to do: **measure it**. Flycast reports no change because it does not model
store-queue timing. Boot `alephone-b23-flat-saves-profile.cdi` and then
`alephone-b24-sq-blit-profile.cdi` and compare the FPS on the VMU.

## Saved games on VMU

Only preferences are mirrored today. Saved games far exceed a VMU's 128K, so
this needs splitting across blocks or compressing, and is a real project rather
than an afternoon.

## Start button does nothing useful

Start is mapped through to the key Aleph One treats as pause, but on hardware it
does not visibly pause or open anything. A console player has no other way out
of a level: there is no Escape key to reach the menu, quit to the main screen, or
save outside a terminal.

What it should do is open a small in-game menu -- resume, options, quit to main
menu -- rather than only freezing the world. That overlaps with the controller-
native UI work below, and should probably be built as the first screen in it,
since it is the one every player will meet.

## Shrink the saved game to a level diff

A saved game in this 2002 engine is a complete standalone level wad: it carries
the whole map, plus the stock physics models, plus the state that actually
changed. Measured by pulling a real save back off the VMU and parsing its chunk
directory:

| group | bytes | share |
|---|---|---|
| level geometry (SIDS, POLY, LINS, EPNT, LITE, OBJS, terminals, placement) | 168,821 | 79% |
| map index table (iidx) | 19,988 | 9% |
| stock physics (MNpx, PRpx, WPpx, FXpx, PXpx) | 11,774 | 6% |
| genuinely dynamic state (mobj, mOns, PLAT, plyr, dwol, weap, automap) | 13,376 | 6% |
| **total** | **214,629** | |

The three largest chunks alone -- sides, polygons and lines -- are 68% of the
file, and every byte of them is already on the disc in the map wad.

Aleph One changed this after 2012, which is why every figure quoted online puts a
save at 30K to 90K. Doing the same here would take a save from 214629 bytes to
roughly 13K raw, maybe 5K deflated, and turn one save slot on a VMU into a dozen.
It would also make saving quicker, which matters at 200MHz.

The catch, and the reason this is a project rather than a tweak: the geometry
cannot simply be dropped. Sides and polygons mutate during play -- switches
retexture panels, terminals change polygon permutations, platforms move -- so
what is needed is a diff against the map file, not an omission. `process_map_wad`
with `restoring_game = true` currently assumes a self-contained wad, and the
revert-game path leans on the same assumption.

Not urgent. One save fits today and works. This buys slots and speed, not
function.
