# Controller-native UI — brief for the design conversation

Paste this into the new conversation. It is the set of constraints a mockup has
to respect to be buildable, and the reasons behind each one, so they can be
argued with rather than just obeyed.

## What we are replacing, and why

Marathon 2's interface is a **mouse** interface with keyboard navigation bolted
on. On a Dreamcast there is no mouse. Today that means:

- The main menu is a fixed bitmap with the buttons *painted into the artwork*.
  Navigation moves a highlight between hardcoded rectangles.
- Dialogs are built from a small widget set and laid out vertically. Choosing a
  list item was originally only reachable by clicking it.
- Preferences, save/load and terminals are all the same machinery.

The goal is an interface designed for a pad that still looks like Marathon —
not a modern console UI wearing a Marathon skin.

## Hard constraints

**Screen.** 640×480, 16-bit colour. It is a *television*: overscan eats the
edges, so nothing readable may sit within ~40 pixels of any edge. This is not
theoretical — traces drawn 8 pixels in were visible on Max's set and completely
unreadable.

**Input.** A Dreamcast pad and nothing else. Analog stick, D-pad, A/B/X/Y, two
analog triggers, Start. No mouse, no keyboard, no text entry that is not painful.
Anything requiring a typed name needs an on-screen keyboard or a generated name.

**Renderer.** The shipping build is the *software* renderer at roughly 15–20 fps.
Menus are drawn by blitting into a software framebuffer, so per-frame full-screen
effects are expensive. Static screens are cheap; animated ones are not.

**No text entry today.** `w_text_entry` exists but there is no way to drive it
from a pad. Saving a game currently relies on a default name.

## What already exists to build from

Widgets, in `Source_Files/Misc/sdl_widgets.h`:

    w_spacer          w_static_text     w_pict           w_button
    w_left_button     w_right_button    w_select_button  w_select
    w_toggle          w_player_color    w_text_entry     w_number_entry
    w_key             w_slider          w_list_base      w_list

Theming already exists and is data-driven. `sdl_dialogs.h` defines fonts —
`TITLE_FONT`, `BUTTON_FONT`, `LABEL_FONT`, `ITEM_FONT`, `MESSAGE_FONT`,
`TEXT_ENTRY_FONT` — and colours for every element in both normal and active
states: `TITLE_COLOR`, `BUTTON_COLOR` / `BUTTON_ACTIVE_COLOR`, `ITEM_COLOR` /
`ITEM_ACTIVE_COLOR`, `LABEL_*`, `MESSAGE_COLOR`, `BACKGROUND_COLOR`,
`KEY_BINDING_COLOR`, `TEXT_ENTRY_*`.

**This matters for the mockup**: a design expressed as "these fonts, these
colours, these widgets, this arrangement" maps onto existing code. A design that
needs new widget types is a bigger job — worth proposing, but say so explicitly.

## Screens to design, roughly in order of how often they are hit

1. **Main menu** — the one every session starts with. Note the artwork problem:
   the buttons are painted into a bitmap, so changing which items exist means new
   artwork, not just code.
2. **Pause / Start button menu** — resume, save, preferences, quit. Does not
   exist yet in the shipping build; the code for it is written and parked.
3. **Save / load game** — a list. Currently unreachable without the Return fix.
4. **Preferences** — graphics, sound, controls. Sliders and toggles.
5. **Terminals** — full-screen text the player reads mid-level.

## Things that should probably go

Not decided — this is Max's call — but these cannot work on a console:

- Gather / Join Network Game: `network_dummy.cpp` is linked, so they do nothing.
- Replay / Save Film: recordings live on the ramdisk and die at power-off.
- Quit: a console has nowhere to quit to.
- Preferences describing hardware we already know: resolution, colour depth,
  fullscreen, OpenGL options while the GL path is not shipping.

## Useful for a mockup

- Marathon's palette is dark greys and greens with amber/green terminal text.
- The HUD (motion sensor, weapon panel, health and oxygen bars) is the strongest
  visual reference for what "Marathon UI" means, and it is already on screen
  every frame.
- 640×480 is 4:3. Design at that size, then check nothing important is outside a
  560×400 safe area centred in it.

## Where the code lives

    Source_Files/Misc/sdl_dialogs.cpp     dialog loop, event handling
    Source_Files/Misc/sdl_widgets.cpp     widget drawing and behaviour
    Source_Files/Misc/shell_sdl.cpp       main menu handling, file dialogs
    Source_Files/Misc/interface.cpp       menu commands, game state
    Source_Files/Misc/preferences_sdl.cpp CONFIGURE CONTROLLER, both pages
    dc/dc_input.c                         pad driver, capture mode, menu table

Since b57 the gameplay bindings are the player's, held in preferences and pushed
to the driver by `dc_apply_pad_bindings()`. The **menu** table in `dc_input.c` is
still fixed and must stay that way: it is what guarantees a dialog can always be
navigated, whatever the player has done to the gameplay bindings. A mockup may
assume A confirms and Start backs out.

`w_pad_key` in `sdl_widgets.cpp` is a new widget worth knowing about — it reads a
pad button rather than a key, by asking the driver to enter capture mode. It is
the only existing answer to "read an input from the player", so an on-screen
keyboard would be built next to it rather than from `w_text_entry`.

Repo: `old-peterhof/marathon-dreamcast`, branch `dc-rebuild`. Current shipping
build is b57.
