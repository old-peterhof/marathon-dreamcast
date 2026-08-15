# Known bugs

Open issues, most recent first. Fixed items move to the bottom with the build
that fixed them.

## Open

| Since | Symptom | Notes |
|---|---|---|
| b23 | **Saving a game failed with "file system error ... error -22"** — fixed in b25 | Saving writes a temp file then calls `TempFile.Exchange(File)` (game_wad.cpp:1274), and `Exchange` uses `rename()`. The KOS ramdisk does not implement rename and returns EINVAL, which is errno 22. So every save failed at the very last step, after the data had already been written. `Exchange` now copies the contents across and deletes the temp on this platform. |
| b23 | **Save dialog: D-pad could not reach the "new save game" option** — fixed in b25 | `w_list_base::event` deliberately swallows UP and DOWN ("Prevent selection of previous/next widget"), so once a list has focus the D-pad can never leave it. A keyboard escapes with TAB, which the controller had no binding for. Both triggers now send TAB in menus. |
| b23 | Sensitivity appears to revert after upgrading builds | Not a code regression — the dithering and scale are intact. Preferences are restored from the VMU, and the base scale changed between builds (b20 halved it), so a percentage saved under an older build means a different turn rate under a newer one. Needs the preference versioning so a scale change resets the value rather than reinterpreting it. |

| Since | Symptom | Notes |
|---|---|---|
| b17 | **"Continue Saved Game" crashes to a black screen** on hardware — possibly addressed in b23, unconfirmed | **Does not reproduce under Flycast**, with or without a VMU attached: the game returns to the menu cleanly. So the crash itself is still unexplained. What b23 does fix is real regardless: `saved_games_dir` was `/ram/Saved Games`, and the KOS ramdisk has no subdirectories — `mkdir` returns EINVAL and files cannot be created inside one, both verified with a standalone probe. So that directory could never exist, meaning **saving was impossible** and loading pointed somewhere that would never be there. Saves and recordings now live directly in `/ram`, the preferences file is hidden from save listings so it cannot be offered as a save, and an empty file list now bails out instead of building a list widget over an empty vector. The `-debug` image traces what the dialog reads, so a hardware boot will show whether anything remains. |
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
