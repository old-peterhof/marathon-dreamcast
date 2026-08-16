# Every menu, dialog and setting

Extracted from the source, not from memory. Pairs with `UI-BRIEF.md`.

Status column:

- **keep** — works and is worth having on a console
- **dead** — cannot work on this hardware, currently does nothing or misleads
- **moot** — works, but describes hardware we already know, so the choice is ours
- **absent** — does not exist yet and arguably should

---

## Main menu

Ten items, walked in this order by the D-pad (`shell_sdl.cpp`, `menus[]`). The
buttons are **painted into a bitmap**, so removing one leaves a hole unless the
artwork changes.

| Item | Status | Note |
|---|---|---|
| Begin New Game | keep | |
| Continue Saved Game | keep | Needs the Return-in-list fix to be selectable by pad |
| Gather Network Game | **dead** | `network_dummy.cpp` is linked; blinks and returns |
| Join Network Game | **dead** | as above |
| Replay Saved Film | **dead** | recordings live on the ramdisk and die at power-off |
| Replay Last Film | **dead** | as above |
| Save Last Film | **dead** | as above |
| Preferences | keep | |
| Quit | **dead** | a console has nowhere to quit to |
| Credits | keep | |

## In-game menu (`mGame`)

Defined in `interface_menus.h`. Reachable on desktop via Alt+key chords, which a
pad cannot produce — so on Dreamcast **none of these are reachable today**.

| Item | Status | Note |
|---|---|---|
| Pause | absent | |
| Save | keep | currently only reachable at a save terminal |
| Revert | keep | reload last save |
| Close Game | keep | back to main menu — no other way off a level |
| Quit Game | dead | see Quit above |

The Start-button pause menu is written and parked; it exposes Resume, Save,
Preferences and Quit-to-main-menu.

---

## Preferences

Root dialog: **PLAYER / GRAPHICS / SOUND / CONTROLS / ENVIRONMENT / RETURN**

### Player settings

| Setting | Widget | Status |
|---|---|---|
| Difficulty | select | keep |
| Name | text entry | **dead** — no text entry from a pad |
| Color | player colour | moot — network appearance |
| Team Color | player colour | moot — network appearance |

Everything under "Network Appearance" only matters for network play.

### Graphics setup

| Setting | Widget | Status |
|---|---|---|
| Color Depth | toggle | moot — always 16-bit here |
| Resolution | toggle | **moot, and dangerous** — this is the hi-res flag that got stuck off and made everything render at 320×160 |
| Screen Size | select | moot — Max has said he never wants the reduced view |
| Fullscreen | toggle | moot — a TV is always fullscreen |
| Brightness | select | keep — genuinely useful on a CRT |
| OPENGL OPTIONS | button | dead while the GL path is not shipping |

### OpenGL options

All **dead** in the shipping build; all relevant if the PowerVR renderer ever
ships. Z Buffer, Landscapes, Fog, Static Effect, Color Effects, Transparent
Liquids, OpenGL Overhead Map, OpenGL HUD, 3D Models.

### Sound setup

| Setting | Widget | Status |
|---|---|---|
| Quality | toggle | keep |
| Stereo | toggle | keep |
| Active Panning | toggle | keep |
| Ambient Sounds | toggle | keep |
| More Sounds | toggle | **keep, and surface it** — costs ~7.5s of every level load |
| Channels | select | moot |
| Volume | slider | keep |

### Controls

| Setting | Widget | Status |
|---|---|---|
| Analog Stick | select | **now Look / Move** — sets `dc_stick_mode` and `input_device` together |
| Mouse Control | toggle | dead — no mouse |
| Invert Look | toggle | keep |
| Invert Mouse | toggle | dead |
| Always Run | toggle | keep — interacts with the swim binding |
| Always Swim | toggle | keep — as above |
| Auto-Switch Weapons | toggle | keep |
| Turn Sensitivity | slider | keep — added for this port |
| Look Sensitivity | slider | keep — added for this port |
| CONFIGURE CONTROLLER | button | **built in b57** — replaces CONFIGURE KEYBOARD, see below |

### Configure keyboard — 20 bindable actions

Move Forward · Move Backward · Turn Left · Turn Right · Sidestep Left ·
Sidestep Right · Glance Left · Glance Right · Look Up · Look Down · Look Ahead ·
Previous Weapon · Next Weapon · Trigger · 2nd Trigger · Sidestep · Run/Swim ·
Look · Action · Auto Map

Currently hardcoded in `dc/dc_input.c` as two tables, one for gameplay and one
for menus.

### CONFIGURE CONTROLLER — built in b57

Takes the place of CONFIGURE KEYBOARD in the Controls dialog. Every action is
listed over two pages — thirteen on the main page, seven under ADVANCED — and
each binds to any button on the pad. The analog stick gets a mode switch rather
than a binding:

- **Look** — stick turns and looks, as the port is configured today. Feeds the
  analog path in `mouse_sdl.cpp`, which produces `delta_yaw` and `delta_pitch`.
- **Move** — stick moves forward and back and turns left and right, the way the
  D-pad or arrow keys do. Feeds the same action flags as the movement keys.

Inputs available: analog stick (2 axes), D-pad (4), A B X Y, two analog triggers,
Start. Twelve or so digital inputs for twenty actions — but with the stick in
Move mode, four of those actions are covered by the stick, and in Look mode two
more are. So the shortfall is smaller than it looks, and the player decides how
to spend what is left.

Three things this changed, and how each was settled:

**Bindings live in preferences.** `dc_pad_bindings[NUMBER_OF_KEYS]` and
`dc_stick_mode`, appended to `input_preferences_data`. They store a small button
id, not a raw button mask: the `DCK_` codes are `1<<28` and `1<<29`, which do not
fit an `int16`, and an id is a stable name besides. The struct grew 44 bytes, so
`dc_prefs_format()` moved and **every existing card is rejected once** — the
format stamp doing its job. Confirmed in Flycast: b57 logs `prefs carry no format
stamp -- ignoring`, then writes a fresh file.

**A player cannot bind themselves out of the interface.** The menu table in
`dc_input.c` stays fixed and unconfigurable, so dialog navigation never depends
on player bindings; only the gameplay table is editable. The worst outcome is a
level that cannot be played, and DEFAULTS on the main page restores from there.

**Analog and digital actions are not interchangeable**, which is what the two
pages are for. Turn Left and Turn Right are on ADVANCED because the stick owns
turning in both modes, so a button for it is a preference rather than a
necessity. Look Up / Down / Ahead are on the main page: they are digital key
actions in the engine, and in Move mode they are the only way to aim vertically.

One trap worth recording. Firing lives in `test_mouse()` in `mouse_sdl.cpp`,
which `vbl_sdl.cpp` only calls when `input_device` is set — so Move mode, which
clears it, would have switched the weapon off. The triggers are therefore always
readable as bindable buttons, and Trigger / 2nd Trigger default to R and L. In
Look mode both paths fire; they set the same action flag, so it costs nothing.

### Environment settings

| Setting | Widget | Status |
|---|---|---|
| Map | file select | moot — one scenario on the disc |
| Physics | file select | moot |
| Shapes | file select | moot |
| Sounds | file select | moot |
| Theme | file select | keep if more than one theme ships |

All five browse for replacement data files. On a fixed disc there is nothing to
browse.

---

## Other dialogs

| Dialog | Status | Note |
|---|---|---|
| Continue Saved Game (file list) | keep | needs Return-in-list |
| Save Game (file list + name) | keep | naming needs an on-screen keyboard or generated names |
| Replay film (file list) | dead | |
| Alerts / errors | keep | |
| "Cancel the game in progress?" | keep | |
| Vidmaster oath / level select | moot | only reached with cheats |
| Terminals | keep | full-screen text, read mid-level |

---

## Summary

**Dead or moot and removable:** 5 of 10 main menu items, all 5 environment
settings, 4 of 6 graphics settings, all 9 OpenGL options, 2 control toggles,
player name and both colours, Quit in two places.

**Missing and wanted:** a pause menu, a controller configuration screen (spec
above), and a way to name a save.

**The hard design problem** is not trimming. It is that twenty actions have to
fit on about twelve inputs, and the current answer — hardcoded tables the player
cannot see or change — is the thing worth replacing first.
