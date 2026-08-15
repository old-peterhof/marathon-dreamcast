# Known bugs

Open issues, most recent first. Fixed items move to the bottom with the build
that fixed them.

## Open

| Since | Symptom | Notes |
|---|---|---|
| b17 | **"Continue Saved Game" crashes to a black screen** on hardware | Reported 2026-08-15. Not investigated. Hunch: saved games are not mirrored to VMU and `mkdir` fails outright on the KOS ramdisk, so `Saved Games/` is never created and the load walks into a directory that does not exist. |
| b17 | **Suspected: a rumble pack in the controller prevents boot** | Max's hunch, unconfirmed. **Not reproducible under Flycast**: with a Puru Puru Pack emulated in expansion slot 1 the game boots and plays normally at 29fps, and the maple dump shows controller at A0, pack at A1, VMU at A2 with the VMU correctly still found. Our code asks for `MAPLE_FUNC_CONTROLLER` specifically and `fs_vmu` only mounts `MAPLE_FUNC_MEMCARD`, so neither is confused by an extra peripheral. If it is real it is likely below our code -- bus power or enumeration timing on real hardware. The `-debug` image now dumps the maple bus at startup, which will show whether the pack and controller are enumerated at all on hardware. |
| b17 | Analog stick still much too fast over the outer half of travel — roughly 2.5 full view rotations per second | **Cause found, fixed in b18.** `test_mouse` was not zeroing the delta after reading it. `execute_timer_tasks()` calls `mouse_idle()` once per pass but runs the game tick N times to catch up, and each tick calls `test_mouse`, so the same deflection was applied N times. Turn rate therefore scaled with how far behind the renderer was: N is about 1 at Flycast's 30fps and much larger at hardware's 20, which is the whole 20x gap. |

## Fixed

| Fixed in | Symptom |
|---|---|
| b17 | Rim of the stick disproportionately sensitive (squared curve steepest at full deflection) — saturation zone added |
| pre-tag | Controller could not navigate the Preferences dialog — dialogs run their own event loop and nothing polled the pad |
| pre-tag | Hardware image shipped with AUTOSTART/PADTEST markers — corrupted `-include` line in Makefile.dc |
| pre-tag | "Error 4" starting a new game — retail Map is MacBinary I, only MacBinary II was detected |
| pre-tag | Black 3D view and black HUD — `SDL_BlitSurface` returns success and copies nothing |
