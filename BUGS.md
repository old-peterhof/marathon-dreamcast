# Known bugs

Open issues, most recent first. Fixed items move to the bottom with the build
that fixed them.

## Open

| Since | Symptom | Notes |
|---|---|---|
| b71 | **The GL renderer draws every texture magenta** — cause found, not yet fixed | b36 renders correctly; b71 does not; the difference is one deleted `#ifdef` in `screen_sdl.cpp`.<br><br>b36 asked for `SDL_OPENGL` on Dreamcast, with a comment saying why: *"SDL_OPENGLBLIT is the deprecated 1.2 mode that keeps a 2D surface alongside the GL context and blits between them every frame. The Dreamcast backend has no such thing -- it hands the PowerVR to GLdc -- so ask for a plain GL surface."* That `#ifdef DC` was dropped before b71, so the build falls through to `SDL_OPENGLBLIT`.<br><br>`SDL_OPENGLBLIT` makes SDL allocate a full-screen shadow surface and composite it over the GL scene every frame. It is 32-bit, because SDL only picks 16-bit inside `#ifdef GL_VERSION_1_2` and GLdc's `gl.h` does not define that macro. So: 640x480x4 = **1,228,800 bytes** -- which is exactly the allocation that would not fit on a 16MB machine -- and the interface then writes 16-bit pixels into it, which read back as `0x7BEF7BEF`: R=EF G=7B B=EF. Magenta.<br><br>**Restoring `SDL_OPENGL` alone is not the fix.** Tried: the game crashes with an SH4 exception the moment the world boots, every time. With a plain GL surface SDL's Dreamcast driver sets `current->pixels = NULL`, and b71's `clear_screen` calls `dc_plate_to_screen()`, which blits into it. b36 had no plate and no software interface, which is why it never hit this.<br><br>So it is an architectural tension, not a typo: **the PowerVR wants no 2D surface, and the interface layer needs one.** Three ways out, none tried -- guard every UI path on a null surface and accept no menus under GL; keep OPENGLBLIT and make the interface honour the real 32-bit format instead of assuming 16; or draw the interface through GL. The middle one is probably cheapest and would also make the 1.2MB affordable, since SDL's 16-bit branch works fine on GLdc (`GL_UNSIGNED_SHORT_5_6_5` is the PowerVR's native format, `texture.c:380`) and only needs `-DGL_VERSION_1_2` when the SDL port is built. |

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

### Start did nothing (fixed in b31)

Every in-game command in this engine is an Alt+key chord -- Alt+P to pause,
Alt+S to save, Alt+C to leave the level. A pad has no Alt and no letters, so
from a console there was no way to pause, no way to save away from a terminal,
and no way out of a level short of resetting the console. Start was bound to
Escape, which gameplay ignores outside demo mode.

Start now opens a PAUSED dialog: RESUME, SAVE GAME, PREFERENCES, QUIT TO MAIN
MENU. The dialog is itself the pause, since dialogs run their own event loop and
the world stops while one is up, and Start closes it again because a dialog reads
Escape as cancel. Quitting goes through iCloseGame, which asks for confirmation,
so a mis-press cannot throw a level away.

This depended on b30: a dialog whose buttons cannot be chosen with Return is no
use on a console.

Verified unattended -- AUTOKEY now also presses Start once a level is running:

    autokey: injecting START in game
    pause menu: opened
    autokey: injecting RETURN into dialog
    fps 30.3  yaw=392 pos=11456,19616


### A listed saved game could not be opened (fixed in b30)

Continue Saved Game showed the save, but choosing it dropped back to the main
menu. The load was never attempted: `w_list_base::event` in sdl_widgets.cpp
handled UP, DOWN, PAGEUP, PAGEDOWN, HOME and END, and nothing else.
`item_selected()` was reachable only from the mouse-click path, so on a console
Return fell through the list to the dialog, which has exactly one button --
CANCEL. The dialog dutifully cancelled.

`w_list_base::event` now treats Return and keypad Enter the way a click is
treated. This fixes every list dialog at once: saved games, replay films and the
level picker.

Proved end to end in Flycast rather than by reading the code. The file dialogs
cannot be reached unattended, so a new AUTOKEY disc marker makes dc_input.c
inject one Return after 60 passes of `dialog::run()`, and `make loadtest` builds
an image that autostarts into CONTINUE SAVED GAME. The run reports:

    autokey: injecting RETURN into dialog
    load: /ram/Untitled Game -> load_level_from_map=1 err=0
    autostart: returned, state=8
    fps 30.3  yaw=507 pos=29696,11407

The position is the saved one, not the level's spawn point, so the state really
came back. Along the way this also confirmed the VMU restore and the XOR unfold
are correct: the fault was never in the save.


| Fixed in | Symptom |
|---|---|
| b17 | Rim of the stick disproportionately sensitive (squared curve steepest at full deflection) — saturation zone added |
| pre-tag | Controller could not navigate the Preferences dialog — dialogs run their own event loop and nothing polled the pad |
| pre-tag | Hardware image shipped with AUTOSTART/PADTEST markers — corrupted `-include` line in Makefile.dc |
| pre-tag | "Error 4" starting a new game — retail Map is MacBinary I, only MacBinary II was detected |
| pre-tag | Black 3D view and black HUD — `SDL_BlitSurface` returns success and copies nothing |

## Fixed

### Start did nothing (fixed in b31)

Every in-game command in this engine is an Alt+key chord -- Alt+P to pause,
Alt+S to save, Alt+C to leave the level. A pad has no Alt and no letters, so
from a console there was no way to pause, no way to save away from a terminal,
and no way out of a level short of resetting the console. Start was bound to
Escape, which gameplay ignores outside demo mode.

Start now opens a PAUSED dialog: RESUME, SAVE GAME, PREFERENCES, QUIT TO MAIN
MENU. The dialog is itself the pause, since dialogs run their own event loop and
the world stops while one is up, and Start closes it again because a dialog reads
Escape as cancel. Quitting goes through iCloseGame, which asks for confirmation,
so a mis-press cannot throw a level away.

This depended on b30: a dialog whose buttons cannot be chosen with Return is no
use on a console.

Verified unattended -- AUTOKEY now also presses Start once a level is running:

    autokey: injecting START in game
    pause menu: opened
    autokey: injecting RETURN into dialog
    fps 30.3  yaw=392 pos=11456,19616


### A listed saved game could not be opened (fixed in b30)

Continue Saved Game showed the save, but choosing it dropped back to the main
menu. The load was never attempted: `w_list_base::event` in sdl_widgets.cpp
handled UP, DOWN, PAGEUP, PAGEDOWN, HOME and END, and nothing else.
`item_selected()` was reachable only from the mouse-click path, so on a console
Return fell through the list to the dialog, which has exactly one button --
CANCEL. The dialog dutifully cancelled.

`w_list_base::event` now treats Return and keypad Enter the way a click is
treated. This fixes every list dialog at once: saved games, replay films and the
level picker.

Proved end to end in Flycast rather than by reading the code. The file dialogs
cannot be reached unattended, so a new AUTOKEY disc marker makes dc_input.c
inject one Return after 60 passes of `dialog::run()`, and `make loadtest` builds
an image that autostarts into CONTINUE SAVED GAME. The run reports:

    autokey: injecting RETURN into dialog
    load: /ram/Untitled Game -> load_level_from_map=1 err=0
    autostart: returned, state=8
    fps 30.3  yaw=507 pos=29696,11407

The position is the saved one, not the level's spawn point, so the state really
came back. Along the way this also confirmed the VMU restore and the XOR unfold
are correct: the fault was never in the save.


### Saved games did not survive a power cycle (b23 - b26, fixed in b27)

Saves lived only in `/ram`, the KOS ramdisk, which is wiped at power-off. Nothing
crashed: after a reboot the load dialog simply found an empty directory and
returned, which on screen looks like the menu flashing. It is the same silence as
the network and film buttons, which also have nothing to list.

They are now mirrored to a VMU either side of a write, the way preferences
already were -- `dc_vmu_save_game()` after `save_game_file()` succeeds, and
`dc_vmu_load_saves()` before Aleph One looks at `saved_games_dir`.

The obstacle was size, and the measured figure is nothing like the one the web
repeats for Aleph One saves:

| | bytes |
|---|---|
| Marathon 2 save, as written | 214,629 |
| Free on a VMU with prefs already on it | 99,328 |
| Deflated, as stored | 82,944 |

A VMU does not hold the 128K on the box: 200 of its 256 blocks are available to
files. So the save is over twice too large at full size, and the first attempt
correctly refused it -- `vmu: card full, need 421 blocks, 194 free`. That refusal
is what produced the measurement, instead of a half-written card.

zlib gets 2.59:1 on a save wad, which is mostly zeroes and repeated structure,
so it now fits in 163 of the 194 free blocks. One save fits; a second will not.
The free-block check still runs on the compressed size, so a save that is too big
even deflated is declined rather than truncated.

Verified end to end in Flycast: written, then restored across a relaunch with
`vmu: restored 1 saved game(s)`.

### Every Flycast build got a blank memory card (fixed in b27)

Not a port bug, but it invalidated a lot of testing. mkdcdisc derives a disc
serial from a hash of the boot binary unless told otherwise, so every build had a
different one -- and Flycast keys its emulated VMU on the serial. Each build
therefore started with a blank card, which is indistinguishable from the VMU code
not working. Thirty-odd `IND-*_vmu_save_A1.bin` files had accumulated, one per
build.

`Makefile.dc` now passes a fixed `-s MARATHON2`, so every build shares one card,
the way one console and one physical VMU do. Every earlier "prefs saved to the
VMU" check under Flycast was in fact writing to a fresh card and proving nothing;
only the hardware test ever exercised that path.

### Flycast could not turn left (not a port bug)

The 8BitDo Lite 2 mapping bound `0+:btn_analog_right` with no `0-` binding at
all, so the analog X axis only existed in one direction. Same omission on the
second stick. Added `0-:btn_analog_left` and `2-:axis2_left` to
`~/Library/Application Support/Flycast/mappings/`. Nothing to do with the game.

## Observations, not yet bugs

- **The main menu is slower and laggier under Flycast than on hardware.** Max's
  observation, and the opposite of what the rest of the port does. The main menu
  redraws the build stamp with bfont on every pass of the event loop, which is
  free on a console and evidently is not through Flycast's blit path. Worth
  checking before blaming the emulator, since the same loop drives menu input and
  that is what feels laggy.

- **Third-party rumble packs are reportedly prone to locking up on maple bus
  activity.** Max found this written up elsewhere, and it matches what the port
  sees exactly: the hang is in KOS's unbounded `maple_wait_scan()` waiting for
  all four ports to report, and pulling the pack out mid-hang lets boot continue.
  Worth trying an official pack before spending anything more on it.
