# Known bugs

Open issues, most recent first. Fixed items move to the bottom with the build
that fixed them.

## Open

| Since | Symptom | Notes |
|---|---|---|
| b17 | **"Continue Saved Game" crashes to a black screen** on hardware | Reported 2026-08-15. Not investigated. Hunch: saved games are not mirrored to VMU and `mkdir` fails outright on the KOS ramdisk, so `Saved Games/` is never created and the load walks into a directory that does not exist. |
| b17 | **CONFIRMED, cause identified, SHELVED: a rumble pack prevents boot** — hangs on a black screen after the Sega licence screen. | **Cause: KOS's unbounded maple bus scan.** `maple_wait_scan()` calls `thd_poll(maple_scan_done, &maple_state, 0)` — timeout 0, meaning forever — and `maple_scan_done` requires `scan_ready_mask == 0xf`, all four ports reporting. The pack makes one port never report, so KOS never finishes init and nothing downstream runs.<br><br>Two pieces of evidence settle it. Max found that **pulling the pack out while hung lets the boot continue**, which is precisely a blocking wait releasing when the offending device leaves the bus. And b21 had `INIT_PURUPURU` cleared, so the rumble driver never initialised, yet it still hung — exonerating the driver.<br><br>**Not our code.** We ask for `MAPLE_FUNC_CONTROLLER` specifically and `fs_vmu` only mounts `MAPLE_FUNC_MEMCARD`. Does not reproduce under Flycast, which boots fine at 29fps with a pack emulated.<br><br>**Shelved.** Bounding the wait is not available: `maple_wait_scan` is bound to `INIT_MAPLE_ALL` alongside `maple_init` itself, so clearing it takes the controller too. What remains — patching KOS's maple, or taking over maple init ourselves — belongs with the force-feedback work in BACKLOG.md. Cheapest next step is not code at all: try an **official** pack, since this is a third-party unit and may simply not answer enumeration correctly. |
| b17 | Analog stick still much too fast over the outer half of travel — roughly 2.5 full view rotations per second | **Cause found, fixed in b18.** `test_mouse` was not zeroing the delta after reading it. `execute_timer_tasks()` calls `mouse_idle()` once per pass but runs the game tick N times to catch up, and each tick calls `test_mouse`, so the same deflection was applied N times. Turn rate therefore scaled with how far behind the renderer was: N is about 1 at Flycast's 30fps and much larger at hardware's 20, which is the whole 20x gap. |

## Fixed

| Fixed in | Symptom |
|---|---|
| b17 | Rim of the stick disproportionately sensitive (squared curve steepest at full deflection) — saturation zone added |
| pre-tag | Controller could not navigate the Preferences dialog — dialogs run their own event loop and nothing polled the pad |
| pre-tag | Hardware image shipped with AUTOSTART/PADTEST markers — corrupted `-include` line in Makefile.dc |
| pre-tag | "Error 4" starting a new game — retail Map is MacBinary I, only MacBinary II was detected |
| pre-tag | Black 3D view and black HUD — `SDL_BlitSurface` returns success and copies nothing |
