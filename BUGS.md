# Known bugs

Open issues, most recent first. Fixed items move to the bottom with the build
that fixed them.

## Open

| Since | Symptom | Notes |
|---|---|---|
| b17 | **"Continue Saved Game" crashes to a black screen** on hardware | Reported 2026-08-15. Not investigated. Hunch: saved games are not mirrored to VMU and `mkdir` fails outright on the KOS ramdisk, so `Saved Games/` is never created and the load walks into a directory that does not exist. |
| b17 | **CONFIRMED: a rumble pack in the controller prevents boot** — hangs after the Sega licence screen; boots fine with the pack removed. b21 disables `INIT_PURUPURU` as an experiment to identify which of the two candidates is responsible. | Confirmed on hardware 2026-08-15, and **not reproducible under Flycast**, which boots fine at 29fps with a pack emulated. Our own code is clean: it asks for `MAPLE_FUNC_CONTROLLER` specifically, and `fs_vmu` only mounts `MAPLE_FUNC_MEMCARD`. Two candidates in KOS init: `maple_wait_scan`, which waits forever for all four ports (`thd_poll(..., 0)`), and `purupuru_init`. Only the second is separately controllable — `maple_wait_scan` is bound to `INIT_MAPLE_ALL` alongside `maple_init` itself. b21 clears `INIT_PURUPURU`: if it boots, the driver is the cause and that is where rumble support must start; if it still hangs, the bus scan is the remaining suspect. |
| b17 | Analog stick still much too fast over the outer half of travel — roughly 2.5 full view rotations per second | **Cause found, fixed in b18.** `test_mouse` was not zeroing the delta after reading it. `execute_timer_tasks()` calls `mouse_idle()` once per pass but runs the game tick N times to catch up, and each tick calls `test_mouse`, so the same deflection was applied N times. Turn rate therefore scaled with how far behind the renderer was: N is about 1 at Flycast's 30fps and much larger at hardware's 20, which is the whole 20x gap. |

## Fixed

| Fixed in | Symptom |
|---|---|
| b17 | Rim of the stick disproportionately sensitive (squared curve steepest at full deflection) — saturation zone added |
| pre-tag | Controller could not navigate the Preferences dialog — dialogs run their own event loop and nothing polled the pad |
| pre-tag | Hardware image shipped with AUTOSTART/PADTEST markers — corrupted `-include` line in Makefile.dc |
| pre-tag | "Error 4" starting a new game — retail Map is MacBinary I, only MacBinary II was detected |
| pre-tag | Black 3D view and black HUD — `SDL_BlitSurface` returns success and copies nothing |
