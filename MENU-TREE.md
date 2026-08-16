# Every menu, dialog and setting

Extracted from the source at b59, not from memory. The design reference: every
screen the player can reach, every widget on it, what each one is worth on a
console, and what the pad can and cannot do. Pairs with `UI-BRIEF.md`, which
covers the visual constraints.

**Status column**

- **keep** — works and is worth having on a console
- **dead** — cannot work on this hardware; currently does nothing or misleads
- **moot** — works, but describes hardware we already know, so the choice is ours
- **absent** — does not exist yet and arguably should

---

## 1. The whole tree at a glance

```
MAIN MENU  (plate + drawn text, b58)
├── New Game → DIFFICULTY → play
├── Continue Game ................................. opens the newest save;
│                                                   greyed and skipped if none
├── Manage Saves → four slots, A loads, X deletes
├── Preferences
│   ├── Brightness ................................ the one live graphics row
│   ├── SOUND ..................... 6 settings (Channels dropped)
│   ├── CONTROLS .................. 7 settings + CONFIGURE CONTROLLER
│   │   └── CONFIGURE CONTROLLER .. 13 actions, two columns
│   │       └── ADVANCED .......... 7 more, X flips between them
│   ├── MANAGE SAVES .............. temporary second entrance
│   └── RETURN
└── Credits

IN GAME
├── Start → PAUSED ................ Resume · Save Game · Preferences · Quit
├── Terminals (full-screen text)
├── Overhead map
└── Save, at a terminal or from the pause menu → four-slot picker
```

**Gone from the menu:** both network items, all three film items, and Quit —
five of the original ten, none of which could work on a console. Nothing was
deleted from the code to do it: `iManageSaves` is appended to the enum, every
other id keeps its value, and the eighteen-rectangle table is untouched, so
non-DC builds are unchanged.

The sections below describe both what was there and what replaced it, because
the reasoning is the part worth keeping.

---

## 2. What the player is holding

Eleven digital inputs and one stick.

| Input | Id | Free to bind? |
|---|---|---|
| A | 1 | yes |
| B | 2 | yes |
| X | 3 | yes |
| Y | 4 | yes |
| D-Pad Up | 5 | yes |
| D-Pad Down | 6 | yes |
| D-Pad Left | 7 | yes |
| D-Pad Right | 8 | yes |
| L Trigger | 9 | yes |
| R Trigger | 10 | yes |
| Start | 11 | **no — reserved** |
| Analog stick | — | **no — mode switch instead** |

Ids are what `input_preferences->dc_pad_bindings[]` stores, one per engine
action. Not raw button masks: the `DCK_` codes for the triggers are `1<<28` and
`1<<29`, which will not fit the `int16` the preferences file uses.

**Reserved, and why.** Start always sends ESCAPE in game and always cancels in
menus, and no screen offers it as a binding — capture reads a Start press as
"cancel" and hands back nothing. The point is a button that still works when
every other binding is wrong. Design around Start meaning *back out*, in every
context, always.

**Ten free buttons, twenty actions.** That shortfall is the central fact of this
interface. Two things soften it: in Look mode the stick covers turning and
looking, and in Move mode it covers forward, back and turning. Neither closes the
gap. Some actions will have no button, and the screen has to make that normal
rather than an error.

---

## 3. Main menu — replaced in b58

The old menu was ten items, and the buttons were **painted into a bitmap** with
navigation moving a highlight between eighteen hardcoded rectangles. Removing an
item left a hole unless the artwork changed, and four files had to agree on item
order through `rect = item - 1 + _new_game_button_rect`. That was the biggest
single obstacle to any redesign.

It is now a static plate with text drawn over it (`dc_mainmenu.cpp`). The item
list is an array, greying an item out is a colour rather than a third bitmap, and
no rectangle is involved. **Changing which items exist no longer needs artwork.**

One thing the rewrite had to fix: the stock walk never consulted
`enabled_item()`, so it would land on a disabled row and let Return be pressed
there. With Continue Game greyed out on a fresh card that row is second in the
list, so skipping disabled items is the difference between the menu working and
not.

The original ten, for the record:

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

Five of ten do nothing.

## 4. In-game menu (`mGame`) — reached via the pause menu since b59

Defined in `interface_menus.h:13` and opened on the desktop with Alt+key chords,
which a pad cannot produce. For most of this port's life **none of it was
reachable**: no way to pause, no way to save away from a terminal, and no way off
a level short of resetting the console.

Start now opens a PAUSED dialog — Resume, Save Game, Preferences, Quit to Main
Menu. The dialog *is* the pause: dialogs run their own event loop, so the world
stops while one is open, and Start closes it again because a dialog reads Escape
as cancel.

Save Game calls `save_game()` directly rather than going through `iSave`, whose
single-player arm is `#if 0`'d upstream and would have been a button that
silently did nothing.

| Item | Status | Note |
|---|---|---|
| Pause | **the dialog is the pause** | no separate item needed |
| Save | **reachable** | → the four-slot picker |
| Revert | not offered | Manage Saves loads any slot, which covers it |
| Close Game | **reachable** | Quit to Main Menu; asks for confirmation first |
| Quit Game | dead | a console has nowhere to quit to |

Difficulty is deliberately not on this menu. It belongs to the run and is chosen
when the run starts — Max's call on UI-HANDOFF open question 3.

---

## 5. Preferences — restructured in b59

Root was **PLAYER · GRAPHICS · SOUND · CONTROLS · ENVIRONMENT · RETURN**. It is
now **Brightness · SOUND · CONTROLS · MANAGE SAVES · RETURN**.

Player is gone: difficulty moved to New Game, the name needs a keyboard, and
both colours are network appearance for a game with no network. Environment is
gone entirely — all five rows browse for replacement data files and a fixed disc
has nothing to browse. Graphics collapsed to Brightness, its one live row,
promoted to the root because a screen with one control on it is not a screen.

**Every settings row now carries a line of plain English** describing whatever
is focused, driven by the focus callback in `dialog::activate_widget`. The
audit below is what decided which rows survived to have one.

### 5.1 Player settings

| Setting | Widget | Values | Status |
|---|---|---|---|
| Difficulty | select | Kindergarten · Easy · Normal · Major Damage · Total Carnage | keep |
| Name | text entry | up to `PREFERENCES_NAME_LENGTH` | **dead** — no text entry from a pad |
| Color | player colour | 8 | moot — network appearance |
| Team Color | player colour | 8 | moot — network appearance |

The last three sit under a "Network Appearance" heading and matter only for
network play, which does not exist here. Difficulty is the whole screen.

### 5.2 Graphics setup

| Setting | Widget | Values | Status |
|---|---|---|---|
| Color Depth | toggle | 8 Bit · 16 Bit | moot — always 16-bit here |
| Resolution | toggle | Low · High | **moot, and dangerous** — this is the flag that stuck off and rendered everything at 320×160 |
| Screen Size | select | 12 sizes, 320x160 up to 1600x1200 | moot — Max never wants the reduced view |
| Fullscreen | toggle | on / off | moot — a TV is always fullscreen |
| Brightness | select | Darkest · Darker · Dark · Normal · Light · Really Light · Even Lighter · Lightest | keep — genuinely useful on a CRT |
| OPENGL OPTIONS | button | → 5.3 | dead while the GL path is parked |

Of six rows, one earns its place. Twelve screen sizes on a device with one screen
is the clearest case of a desktop menu stranded on a console.

### 5.3 OpenGL options

Nine toggles: Z Buffer · Landscapes · Fog · Static Effect · Color Effects ·
Transparent Liquids · OpenGL Overhead Map · OpenGL HUD · 3D Models.

All **dead** in the shipping build, all relevant the day the PowerVR renderer
ships. Worth keeping in the design even if hidden today.

### 5.4 Sound setup

| Setting | Widget | Values | Status |
|---|---|---|---|
| Quality | toggle | 8 Bit · 16 Bit | keep |
| Stereo | toggle | on / off | keep |
| Active Panning | toggle | on / off | keep |
| Ambient Sounds | toggle | on / off | keep |
| More Sounds | toggle | on / off | **keep, and surface it** — costs ~7.5s of every level load |
| Channels | select | 0–8 | moot |
| Volume | slider | `NUMBER_OF_SOUND_VOLUME_LEVELS` | keep |

More Sounds is the only preference in the game with a large, measured cost, and
it sits among six that cost nothing. Whatever the redesign does, that row should
say what it costs.

### 5.5 Controls

| Setting | Widget | Values | Status |
|---|---|---|---|
| Analog Stick | select | **Look · Move** | keep — see 6.2 |
| Mouse Control | toggle | — | **removed on DC** — there is no mouse |
| Invert Look | toggle | on / off | keep |
| Always Run | toggle | on / off | keep — interacts with Run/Swim |
| Always Swim | toggle | on / off | keep — as above |
| Auto-Switch Weapons | toggle | on / off | keep |
| Turn Sensitivity | slider | `NUMBER_OF_SENS_LEVELS` | keep — added for this port |
| Look Sensitivity | slider | `NUMBER_OF_SENS_LEVELS` | keep — added for this port |
| CONFIGURE CONTROLLER | button | → 6 | keep — replaces CONFIGURE KEYBOARD |

Two sensitivity sliders rather than one because turning and looking want
different rates: Marathon's vertical range is small, so a comfortable yaw speed
feels twitchy on pitch.

### 5.6 Environment settings

| Setting | Widget | Status |
|---|---|---|
| Map | file picker | moot — one scenario on the disc |
| Physics | file picker | moot |
| Shapes | file picker | moot |
| Sounds | file picker | moot |
| Theme | file picker | keep if more than one theme ships |

All five browse for replacement data files. On a fixed disc there is nothing to
browse. The whole screen can go.

---

## 6. CONFIGURE CONTROLLER — built in b57

Replaces CONFIGURE KEYBOARD, which offered a device this console does not have.

### 6.1 The two pages

Twenty actions over two screens. The split is not cosmetic: with ten buttons for
twenty actions, a screen listing all twenty as equals invites the player to spend
a button on Glance Left before Trigger.

**Main page — 13 rows**

| Action | Index | Default |
|---|---|---|
| Move Forward | 0 | Y |
| Move Backward | 1 | A |
| Sidestep Left | 4 | X |
| Sidestep Right | 5 | B |
| Look Up | 8 | — |
| Look Down | 9 | — |
| Look Ahead | 10 | — |
| Previous Weapon | 11 | — |
| Next Weapon | 12 | D-Pad Right |
| Trigger | 13 | R Trigger |
| 2nd Trigger | 14 | L Trigger |
| Action | 18 | D-Pad Up |
| Auto Map | 19 | D-Pad Down |

**ADVANCED page — 7 rows**

| Action | Index | Default |
|---|---|---|
| Turn Left | 2 | — |
| Turn Right | 3 | — |
| Glance Left | 6 | — |
| Glance Right | 7 | — |
| Sidestep | 15 | — |
| Run/Swim | 16 | D-Pad Left |
| Look | 17 | — |

Ten of twenty are bound by default, which is every button spent.

**Why those two lists.** Look Up / Down / Ahead are on the main page because they
are digital key actions in the engine (`_looking_up`, `_looking_down`,
`_looking_center`) — a button gives exactly what the original keyboard gave — and
because in Move mode they are the only way to aim up or down. Turn Left / Right
are on ADVANCED because the stick owns turning in both modes, so a button for it
is a preference, not a necessity.

A compile-time check asserts the two lists add to twenty. An action on neither
page could never be bound and nothing would say so.

### 6.2 Stick modes

The Controls screen's "Analog Stick" select, not a binding.

| Mode | Stick does | Under the hood |
|---|---|---|
| **Look** | turns and looks | `input_device = _mouse_yaw_pitch`, so `vbl_sdl.cpp` calls `test_mouse()` and the analog path in `mouse_sdl.cpp` makes `delta_yaw` and `delta_pitch` |
| **Move** | forward, back, turn left, turn right | `input_device = _keyboard_or_game_pad`; the four `DCK_STICK_*` codes are ORed into the movement actions' masks |

Look is the default and is how the port has played all along.

**The trap in Move mode.** Firing lives in `test_mouse()` — R primary, L alt —
and `vbl_sdl.cpp` only calls it when `input_device` is set. Move mode clears it,
so Move mode would have switched the weapon off. The triggers are therefore
always readable as bindable buttons, and Trigger and 2nd Trigger default to R and
L. In Look mode both paths fire; they set the same action flag, so it costs
nothing.

### 6.3 Binding a button

`w_key` could not be reused. It enters binding mode on a mouse click, and there
is no mouse; and it captures an `SDL_KEYDOWN`, but `dc_input.c` has already
turned the pad button into a key by the time a widget sees it, so it would record
"z" rather than "X".

So `w_pad_key` drives a capture mode in the driver: while capturing, the poll
records the first newly-pressed button and **injects no keys at all**, so the
press that names a binding cannot also drive the row underneath it. Start reports
cancel. Capture times out after about a minute, so a mode entered by accident
cannot strand the interface.

Two consequences for a designer. Any "press an input" flow has exactly one
existing mechanism, and this is it. And any such flow must state its own way out
on screen, because while it runs the pad does nothing else.

### 6.4 Nobody can get stuck

Four things guarantee it, and a redesign must keep all four:

1. **The menu binding table in `dc_input.c` is fixed and not configurable.**
   Dialog navigation never depends on player bindings. Only the gameplay table
   is editable.
2. **Start is reserved** in both contexts.
3. **DEFAULTS** on the main page restores the table above.
4. **CANCEL is per page**, so abandoning ADVANCED does not throw away edits made
   on the main page.

The worst reachable outcome is a level that cannot be played, and that is fixed
from the menu without touching the VMU.

### 6.5 Storage

```c
int16 dc_pad_bindings[NUMBER_OF_KEYS];  // button id per engine action
int16 dc_stick_mode;                    // 0 = Look, 1 = Move
```

appended to `input_preferences_data`. Only the button moves — which key an action
sends is still the engine's own `keycodes[]`, so rebinding the pad disturbs
nothing else in the game.

The struct grew 44 bytes, so `dc_prefs_format()` moved and **every card written
before b57 is rejected once**. That is the format stamp in `dc/dc_vmu.c` working,
not a fault. Flycast confirms it: `prefs carry no format stamp -- ignoring`, then
a fresh file is written.

---

## 7. Other dialogs

| Dialog | Status | Note |
|---|---|---|
| Continue Saved Game (file list) | keep | needs Return-in-list |
| Save Game (file list + name) | keep | naming needs an on-screen keyboard or generated names |
| Replay film (file list) | dead | |
| Alerts and errors | keep | |
| "Cancel the game in progress?" | keep | |
| Vidmaster oath / level select | moot | only reached with cheats |
| Terminals | keep | full-screen text, read mid-level |

## 8. Widgets available

From `sdl_widgets.h`. A design built from these maps onto existing code; one that
needs a new widget type is a bigger job — worth proposing, worth saying out loud.

    w_spacer        w_static_text    w_pict          w_button
    w_left_button   w_right_button   w_select_button w_select
    w_toggle        w_player_color   w_text_entry    w_number_entry
    w_key           w_slider         w_list_base     w_list
    w_pad_key  ← new in b57, reads a pad button

`w_text_entry` exists but nothing can drive it from a pad. An on-screen keyboard
would be built next to `w_pad_key`, not from `w_text_entry`.

---

## 9. What to cut

**Dead or moot, and removable now**

- 5 of 10 main menu items — both network, all three film, Quit
- all 5 environment settings, so the whole screen
- 4 of 6 graphics settings — depth, resolution, screen size, fullscreen
- all 9 OpenGL options, until the GL path ships
- player name and both colours
- Channels, under sound

That is roughly half of every screen.

**Missing and wanted**

- a pause menu — written and parked, and Start is already free
- a way to name a save, or generated names
- health and air on the VMU, beside the FPS readout

**The hard problem is not trimming.** It is that twenty actions have to fit on
ten buttons. b57 answers that by making the choice the player's and showing them
what they have spent. A redesign should keep that honesty rather than hide the
shortfall.
