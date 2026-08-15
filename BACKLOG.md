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

## PowerVR / hardware-accelerated renderer

Would buy real frames — hardware runs about 20fps software-rendered. Blocked on
GLdc lacking clip planes, which `OGL_Render.cpp` uses five of for portal and
liquid-surface clipping. See the note in `Makefile.dc` under `GL=1`, which
records the full list of missing entry points.

## sh4zam store-queue blit

`dc_copy_to_screen` copies 400KB per frame into VRAM with a per-row `memcpy`.
`shz_sq_memcpy32` from sh4zam uses the SH4 store queues, the standard fast path
for VRAM writes. Most of `shz_mem.h` is `SHZ_INLINE`, so it can be used
header-only without building the kos-port. Measure with the VMU Profiler either
side.

## Saved games on VMU

Only preferences are mirrored today. Saved games far exceed a VMU's 128K, so
this needs splitting across blocks or compressing, and is a real project rather
than an afternoon.
