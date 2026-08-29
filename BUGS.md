# Known bugs

Open issues, most recent first. Fixed items move to the bottom with the build
that fixed them.

## Open

| Since | Symptom | Notes |
|---|---|---|
| b65 | **Damage and pickup flashes stalled the game** — confirmed on hardware in b70, fixed in b71, untested | Being hit or picking something up made the game hitch rather than flash. Same root cause as the pause menu above: `dc_apply_fade_tint` ran as a second pass over `main_surface` *after* the world had been blitted, and `main_surface->pixels` is `vram_l`, so it read every pixel of the 640x320 view back out of video RAM uncached -- ~204,800 of them, immediately after the blit had just written the same pixels. Two full-view passes, one of them the expensive direction. **Fixed in b71** by applying the tint *during* the copy instead: `dc_fade_blit_tinted` reads `world_pixels`, which is ordinary cached main memory, and writes each pixel to VRAM once. When no fade is live it returns 0 and the render path takes the existing store-queue blit unchanged, so nothing changes in the common case. |
| b58 | **X saved over a slot on the SAVE GAME screen** — found by review, fixed in b70, untested | `dc_choose_save_slot` ended with `slot &= ~DC_SCREEN_X;` under the comment "X does nothing here". Masking the flag off does not discard the press, it *converts* it: `dc_screen_run` returns `DC_SCREEN_X \| 2` for X on the second row, and clearing 0x4000 leaves 2 — which is exactly what A returns. So X was a synonym for A on the one screen where X means DELETE everywhere else in the interface. `fill_slot_rows` is called there with `empties_selectable = true`, so no row is disabled and it applied to all four. The OVERWRITE confirm still stood in the way, so this was never silent data loss; it was the destructive button acting as the commit button. **Fixed in b70** by returning 0 for `DC_SCREEN_X` and `DC_SCREEN_Y` instead of masking them. |
| b25 | **A failed flush could delete a save and report success** — found by review, fixed in b70, untested | The DC `FileSpecifier::Exchange` copies the temp over the target and then removes the temp. It checked every `fwrite` and did not check `fclose`. The last partial buffer of a buffered stream is written *by* `fclose`, so a flush that fails leaves `err` at 0 — the temp is removed and `Exchange` returns true, giving the engine a successful save over a truncated file. `/ram` is the only writable filesystem here and a Marathon 2 save is ~215KB, so exhausting it at the flush is the realistic path rather than a theoretical one. This is the last step of every save. **Fixed in b70:** `fclose(out)` is checked and its failure becomes `err`, so the temp survives and the save is reported as failed. |
| pre-tag | **Unplugging the controller left held keys down** — found by review, fixed in b70, untested | `dc_input_poll_body` returns early when `maple_enum_type` finds no controller, zeroing the analog axes but injecting nothing and leaving `previous` intact. Injection is edge-detected, so a key held at the moment the cable leaves the bus has no falling edge to close it and stays set in SDL's key-state array — which `vbl_sdl.cpp` reads during play. Pulling the pad while holding Y therefore walks the player forward until something else calls `dc_input_set_ingame`. **Fixed in b70:** the no-device path releases whatever `previous` says was held, walking both the static table and the configured bindings, then clears `previous` and `have_previous` so reconnecting re-baselines instead of firing a press. |
| b68 | **Pause menu redrew the whole screen on every cursor move** — confirmed on hardware in b70, fixed in b71, untested | Reported as a black sweep from top to bottom before the menu snaps back, on every move and again when Start opens the menu. **The earlier diagnosis here was half right and blamed the wrong half.** It attributed the cost partly to `SDL_UpdateRect(v, 0, 0, 0, 0)`, "a full 600K push". That call does nothing at all: this port never calls `SDL_DC_SetVideoDriver`, so the driver is the default `SDL_DC_DMA_VIDEO`, which `DC_IsTexturedDriver` rejects; `SDL_DOUBLEBUF` is never requested either; so `SDL_SetVideoMode` takes the plain path, sets `current->pixels = vram_l`, and `DC_UpdateRects` has an empty body for that configuration. Removing the call would have bought nothing.<br><br>**The real cost is that the surface *is* video RAM.** With no shadow buffer, `dc_ui_blend(v, 0, 0, v->w, v->h, ink, 230)` is 307,200 uncached VRAM reads, each wrapped in an out-of-line `SDL_GetRGB` and `SDL_MapRGB`. That is the sweep, and it ran again for every keypress because the run loop repaints everything whenever `dirty` is set.<br><br>**Fixed in b71, two ways.** `dc_ui_blend` now unpacks and repacks with the pixel format's own shifts instead of calling into SDL per pixel, the same arithmetic `dc_fade.c` uses. And `draw_screen` takes a `full` flag: the background, title, rules, state column and hint bar are drawn once when the screen opens, and a cursor move repaints only the panels, which are opaque so nothing underneath needs restoring. The screen is static while a `dc_screen_run` loop is up -- the dialog *is* the pause, nothing else draws -- so the assumption the old note flagged as unverified is now the thing the fix is built on, and it is checked rather than assumed: a screen with an explainer sets `partial_ok` false, because the explainer is the one thing outside the panel that changes with the highlight. |
| b68 | **Changing Stereo froze the Preferences screen, and crashed from the pause menu** — fixed in b69, untested | Any sound preference change calls `set_sound_manager_parameters`, which closes and reopens the audio device: `set_sound_manager_status(false)` → `SDL_CloseAudio()`, then `SDL_OpenAudio()`. KOS's SDL audio driver spins unbounded in `DCAUD_PlayAudio` waiting on the AICA play position — `while(SDL_DC_aica_get_pos(0)/spec->samples == nextbuf) thd_pass();` — and `SDL_CloseAudio` joins that thread, so a cycle that leaves the spin unsatisfied hangs whoever called it. Stereo is the worst case because `_stereo_flag`'s *only* effect is `desired.channels`, so turning it off asks the AICA to go from two channels to one mid-session. **Fixed in b69 by not reopening the device, ever.** The original `desired.channels = flags & _stereo_flag ? 2 : 1` is untouched — the flag is simply read only where the device is opened, so it takes effect at the next launch. `set_sound_manager_parameters` stores the preferences and applies volume live, and returns without cycling anything. An earlier attempt centred the pan in the mixer instead; that was an invention where none was needed and it is gone.<br><br>**There is no way to make the cycle safe from here.** SDL's audio thread is `while (audio->enabled) { ...; PlayAudio(); }` and `SDL_AudioQuit` clears `enabled` then joins — but `enabled` is only tested at the top of the loop and `PlayAudio` runs whether or not the device is paused, so the thread makes one more pass through the driver on the way out. Pausing does not skip it and locking does not either. `DCAUD_CloseAudio` also frees the mix buffer while a file-static pointer to the old device survives, which is the likelier crash mechanism specifically. A real fix means patching KOS's SDL, and that toolchain is pinned. **Behaviour change:** Stereo and Quality now say "Applies next launch" in their explainer, because they do. Skipping `unload_all_sounds()` is part of the same bargain — the sounds in memory match the format the device is still running.<br><br>**Narrowed to Stereo alone, by testing.** It crashes with no rumble pack fitted, and every other preference — including ones on other screens — commits and saves normally. So `write_preferences()` and the maple bus are both exonerated, and a mid-session maple hypothesis raised here earlier was wrong. What is left is the one thing Stereo does that nothing else does: change the AICA channel count while the device is open. That is the transition b69 removes.<br><br>Note for anyone reading this as "it used to work": toggling Stereo off and back on before leaving the screen commits nothing, because `out == sound_preferences->flags` and `changed` stays false. Only leaving the screen with it actually changed reaches the device.<br><br>This also closes the same hole in Quality, which would have cycled the device for the sample format. |
| b68 | **The Controls explainer drew through the CONFIGURE CONTROLLER row** — fixed in b69, untested | Eight rows plus a caption is the tallest panel in the interface. At `row_h` 30 from `panel_y` 140 it ended at y=406, and the explainer line sits at 372, so the last row and the sentence shared 34 pixels. Now `row_h` 26 from 128, ending at 362. Every other screen was already clear — Sound ends at 346, the binding pages at 336, Manage Saves at 312 — which is why this was the only one visible. `dc_screen_run` now traces a warning when a panel with an explainer reaches past 372, so a screen nobody has looked at cannot ship with it silently. |

| Since | Symptom | Notes |
|---|---|---|
| b23 | **Saving a game failed with "file system error ... error -22"** — fixed in b25 | Saving writes a temp file then calls `TempFile.Exchange(File)` (game_wad.cpp:1274), and `Exchange` uses `rename()`. The KOS ramdisk does not implement rename and returns EINVAL, which is errno 22. So every save failed at the very last step, after the data had already been written. `Exchange` now copies the contents across and deletes the temp on this platform. |
| b23 | **Save dialog: D-pad could not reach the "new save game" option** — fixed in b25 | `w_list_base::event` deliberately swallows UP and DOWN ("Prevent selection of previous/next widget"), so once a list has focus the D-pad can never leave it. A keyboard escapes with TAB, which the controller had no binding for. Both triggers now send TAB in menus. |
| b23 | Sensitivity appears to revert after upgrading builds | Not a code regression — the dithering and scale are intact. Preferences are restored from the VMU, and the base scale changed between builds (b20 halved it), so a percentage saved under an older build means a different turn rate under a newer one. Needs the preference versioning so a scale change resets the value rather than reinterpreting it. |

| Since | Symptom | Notes |
|---|---|---|
| b17 | **"Continue Saved Game" crashes to a black screen** on hardware — possibly addressed in b23, unconfirmed | **Does not reproduce under Flycast**, with or without a VMU attached: the game returns to the menu cleanly. So the crash itself is still unexplained. What b23 does fix is real regardless: `saved_games_dir` was `/ram/Saved Games`, and the KOS ramdisk has no subdirectories — `mkdir` returns EINVAL and files cannot be created inside one, both verified with a standalone probe. So that directory could never exist, meaning **saving was impossible** and loading pointed somewhere that would never be there. Saves and recordings now live directly in `/ram`, the preferences file is hidden from save listings so it cannot be offered as a save, and an empty file list now bails out instead of building a list widget over an empty vector. The `-debug` image traces what the dialog reads, so a hardware boot will show whether anything remains. |
| b17 | **FIXED in b68, confirmed on hardware: a rumble pack prevented boot** — hangs on a black screen after the Sega licence screen. | **Cause: KOS's unbounded maple bus scan.** `maple_wait_scan()` calls `thd_poll(maple_scan_done, &maple_state, 0)` — timeout 0, meaning forever — and `maple_scan_done` requires `scan_ready_mask == 0xf`, all four ports reporting. The pack makes one port never report, so KOS never finishes init and nothing downstream runs.<br><br>Two pieces of evidence settle it. Max found that **pulling the pack out while hung lets the boot continue**, which is precisely a blocking wait releasing when the offending device leaves the bus. And b21 had `INIT_PURUPURU` cleared, so the rumble driver never initialised, yet it still hung — exonerating the driver.<br><br>**Not our code.** We ask for `MAPLE_FUNC_CONTROLLER` specifically and `fs_vmu` only mounts `MAPLE_FUNC_MEMCARD`. Does not reproduce under Flycast, which boots fine at 29fps with a pack emulated.<br><br>**Fixed in b68, and confirmed on hardware — it boots with a third-party pack fitted.** The earlier conclusion here — that bounding the wait was unavailable because `maple_wait_scan` shares `INIT_MAPLE_ALL` with `maple_init` — was true of the flag and false of the mechanism. `KOS_INIT_FLAGS()` only *defines variables*: one function pointer per subsystem, which `init.c` later calls if it is non-NULL. So `dc/dc_maple.c` now emits KOS's own list with the `maple_wait_scan` line dropped and that one pointer defined NULL by hand, and `shell_sdl.cpp` calls `dc_maple_wait_scan_bounded(1500)` instead — the identical predicate, `maple_state.scan_ready_mask == 0xf`, with a deadline. Every driver including `purupuru_init` starts exactly as before; verified at the object level, `maple_wait_scan_weak` in .bss and zero references to the function. A normal boot is unchanged because the scan completes in a few frames. Trying an **official** pack is still worth doing: this fix stops the hang, it does not make a third-party pack answer enumeration. |
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


### A listed saved game could not be opened (fixed in b30, lost, restored in b59)

**This entry was wrong for twenty-eight builds.** The fix below landed in b30 and
was then removed by `e3fdc7b`, "Return the tree to b29, the only build confirmed
working on hardware" -- which reverted the whole tree, this fix included, while
this page went on claiming it was fixed. Anyone reading here would have concluded
list dialogs worked when they did not.

It was restored verbatim from `9d6a0ad` in b59, when the four-slot save screens
needed it. Worth remembering as a failure mode: a wholesale revert silently
un-fixes everything it passes over, and the notes do not notice.

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

- **The VMU profiler costs framerate.** With PROFILE staged, Flycast fell from a
  steady 30.3 to 21-24. It is a background thread writing to the VMU, so the
  number on the LCD understates what the game does without it. Fine for comparing
  one build against another, since both carry the same overhead; not an absolute.

## A preferences file from another build stops the game starting a level

Fixed in b45, and worth describing carefully because it cost a whole night and
gives almost nothing away while it is happening.

**How it presents.** Start New Game, the splash appears, then black, and the
level never arrives. No error, no text, nothing on the serial console. It follows
you across every disc you burn, including builds that worked yesterday, because
the cause is on the memory card rather than the disc. Deleting the preferences
file on the VMU fixes it instantly.

**How to recognise it in one test.** Pull the memory card out and boot. If the
game starts a level with no card, it is this.

**Why it happens.** Adding a field to any of the preference structs changes the
size of a stored chunk. Aleph One notices the mismatch in
w_get_data_from_preferences and appends a replacement -- and
append_data_to_wad does replace the tag correctly rather than duplicating it, so
that part is sound and is NOT the fault. The damage is done elsewhere, and it
breaks in both directions: a newer build reading an older file, and an older
build reading a newer one.

**What b45 does about it.** The card copy carries a stamp in front of the
payload: a magic, and a number derived from the sizes of the preference structs
the build was compiled against. A card that does not match is ignored and Aleph
One writes fresh defaults over it. Verified in Flycast both ways -- a matching
stamp restores, a changed one logs "prefs carry no format stamp -- ignoring",
writes defaults, and reaches gameplay.

**What is NOT proven.** The struct-size mismatch is not the mechanism. Tried to
reproduce it in Flycast by growing input_preferences_data by two bytes exactly as
b35 did, writing preferences with that build, then reading them back with the
normal one -- with the stamp check disabled so the pre-b45 behaviour was in play.
Both directions loaded, applied defaults correctly (sens=30/30) and reached
gameplay. Aleph One's fallback handles the mismatch properly, which agrees with
reading append_data_to_wad: it replaces a tag rather than duplicating it.

So something about Max's card stops a level starting on hardware in a way an
emulated card does not. The stamp is still worth having -- it makes a whole class
of cross-build damage impossible, and it is verified working in both directions --
but it should not be described as the fix for the black screen until the actual
mechanism is understood. Deleting the preferences file demonstrably cures it on
hardware; why, is still open.

**What it cost.** Three builds were blamed and rolled back, a toolchain was
rebuilt on a hypothesis, and the port was reverted to a build whose binary turned
out to be byte-identical to the one already failing. The lesson is cheaper than
the diagnosis: when a symptom survives a change of disc, the cause is not on the
disc.

## The chapter screen cannot be skipped from a controller

Fixed. A level load holds the chapter screen for ten seconds:

    wait_for_click_or_keypress(text_block ? -1 : 10*MACHINE_TICKS_PER_SECOND);

That is meant to be skippable -- the name says so -- but the loop in
csmisc_sdl.cpp only ever looked at SDL's event queue, and nothing puts a
Dreamcast controller into that queue except dc_input_poll(), which it never
called. So from a console the wait was unskippable: the screen is up, every
button does nothing, and the full ten seconds is spent on every single load.

It now polls the pad. The ten-second timeout is left alone deliberately -- it is
the original design, and shortening it is Max's call rather than mine.

The same loop also tested `event` without clearing it, so the first pass read
uninitialised stack and later passes re-tested the previous event.

## Where a level load actually goes

Measured under Flycast, which is faster than the real drive, so treat these as a
floor rather than an estimate of hardware:

Measured stage by stage:

| stage | ms |
|---|---|
| menu fade out | 503 |
| movie | 0 |
| **chapter screen** | **22553** |
| **new_game()** | **22972** |
| start_game | 518 |
| **total** | **46587** |

and inside those two:

| | ms |
|---|---|
| chapter: picture clut | 22 |
| chapter: start fade | 0 |
| chapter: draw picture | 4707 |
| chapter: long fade | 2721 |
| chapter: the rest, mostly the ten-second wait above | ~15000 |
| new_game: 11 shape collections | 7112 |
| new_game: update_color_environment | 539 |
| new_game: unaccounted | ~15300 |

So half the load is the chapter screen rather than loading anything, and ten
seconds of that was a wait nobody on a console could skip. The remaining ~15
seconds inside new_game() is still unmeasured and is the next thing to break
down.

### Full accounting, stage by stage

| stage | ms |
|---|---|
| menu fade out | 503 |
| movie | 0 |
| chapter screen | 22553 |
| new_game(): goto_level / load_level_from_map | 1603 |
| new_game(): load_collections | 7663 |
| new_game(): load_all_monster_sounds | 13700 |
| new_game(): load_all_game_sounds | 0 |
| start_game | 512 |
| **total** | **46605** |

Flycast reproduces these to the millisecond across runs, which makes
single-variable experiments trustworthy. Two things dominate and neither is
reading the level:

**The chapter screen, 22.5 seconds.** About ten of those are
wait_for_click_or_keypress, which could not be skipped from a controller until
this was fixed. The rest is drawing the picture (4.7s) and the long fade (2.7s).

**Loading monster sounds, 13.7 seconds.** Every sound is a separate seek and read
into the Sounds file. The "More Sounds" preference makes each one load all of its
permutations rather than just the first, and turning it off halves this:

| More Sounds | load_all_monster_sounds |
|---|---|
| on | 13700 ms |
| off | 6240 ms |

Worth knowing when testing this: preferences stored on the card override
default_sound_manager_parameters entirely, so changing the default has no effect
on a console that already has a card. The toggle in Preferences is the lever.

Also recorded, since it was tried and made things worse: giving each file stream
a 64KB buffer with setvbuf. KOS's ISO9660 driver does have a bulk-read fast path
for sector-aligned requests, but Aleph One reads a wad chunk by seeking to its
offset and then reading a few hundred bytes (wad.cpp), so a large buffer means
every seek discards it and re-reads 64KB to serve a short read. That build failed
to load a level at all on hardware.
