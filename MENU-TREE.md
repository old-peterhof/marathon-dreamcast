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

**Four of ten are worth keeping.** Five are dead, one is Quit.

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
| Analog Stick | toggle | keep |
| Mouse Control | toggle | dead — no mouse |
| Invert Look | toggle | keep |
| Invert Mouse | toggle | dead |
| Always Run | toggle | keep — interacts with the swim binding |
| Always Swim | toggle | keep — as above |
| Auto-Switch Weapons | toggle | keep |
| Turn Sensitivity | slider | keep — added for this port |
| Look Sensitivity | slider | keep — added for this port |
| CONFIGURE KEYBOARD | button | dead — should become CONFIGURE CONTROLLER |

### Configure keyboard — 20 bindable actions

Move Forward · Move Backward · Turn Left · Turn Right · Sidestep Left ·
Sidestep Right · Glance Left · Glance Right · Look Up · Look Down · Look Ahead ·
Previous Weapon · Next Weapon · Trigger · 2nd Trigger · Sidestep · Run/Swim ·
Look · Action · Auto Map

Currently hardcoded in `dc/dc_input.c` as two tables, one for gameplay and one
for menus. A controller-native version of this screen is the obvious replacement
and would let bindings be changed without a rebuild.

Pad inputs available to map onto those 20: analog stick (2 axes), D-pad (4), A B
X Y, two analog triggers, Start. **Twelve or so inputs for twenty actions**, which
is the core design problem.

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

**Missing and wanted:** a pause menu, a controller configuration screen, and a
way to name a save.

**The hard design problem** is not trimming. It is that twenty actions have to
fit on about twelve inputs, and the current answer — hardcoded tables the player
cannot see or change — is the thing worth replacing first.
