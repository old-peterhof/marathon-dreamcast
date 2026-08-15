# Overnight log — 2026-08-15

Running unattended. Rules I'm holding myself to:

- Commit and push to `origin/dc-rebuild` after every green build. Nothing lives only on this disk.
- No deletes, no force-push, no rewriting history.
- Every change gets a reason recorded here, including the ones that failed.

## Starting state

Boots to the Marathon 2 main menu in Flycast with full retail data. Two blockers:

1. **Error 4 on Begin New Game.** `errUnknownWadVersion`, raised only at
   `wad_prefs.cpp:291` — so this is the *preferences* wad, not the map, despite
   what the dialog says. Ruled out: the ramdisk. Modern KOS already calls
   `fs_ramdisk_init()` at `init.c:200` and the function early-returns when
   already initialised, so our `fs_mem_init()` is a harmless no-op.
2. **No controller support.** SDL 1.2's DC driver (`SDL_dcevents.c`) polls only
   `MAPLE_FUNC_MOUSE` and `MAPLE_FUNC_KEYBOARD`; the joystick backend is
   `dummy`. `MAPLE_FUNC_CONTROLLER` is never read. This is why BERO's README
   says a keyboard is required.

Cosmetic: the SDL software cursor leaves trails on the menu — it blits without
restoring what was underneath.

## Plan

1. Get hard evidence on error 4 with a standalone filesystem test ELF that
   prints to the framebuffer, rather than guessing at it.
2. DC controller driver: read maple directly, inject via `SDL_PrivateKeyboard`
   (confirmed exported from libSDL.a). That updates SDL's internal key state,
   so `SDL_GetKeyState` in `vbl_sdl.cpp:98` sees it too — a plain
   `SDL_PushEvent` would not.
3. Hide the cursor on DC.
4. Attempt input injection into Flycast so gameplay can be verified, not just
   the menu.

## Entries

### 00:20 — Backup branch pushed

`origin/dc-rebuild` created on the existing repo. `dc-modern` untouched.

### 00:26 — Filesystem probe results (tools/fstest.c)

Built a standalone ELF that boots on its own and prints pass/fail to the
framebuffer with bfont, so the answers can be read straight off the screen.
Beats guessing.

| test | result |
|---|---|
| stat on missing file fails | OK |
| access on missing file fails | OK |
| **mkdir** | **FAIL, errno 22 (EINVAL)** |
| fopen wb + fwrite | OK |
| stat size | FAIL, reports 1024 for a 256-byte file |
| read back matches | OK |
| r+b seek-0 rewrite | OK |
| rewrite persisted | OK |
| SDL_RWFromFile wb | OK |
| SDL_RW r+b rewrite | OK |
| SDL_RW seek-to-end length | OK, 256 |
| stdio seek-to-end length | OK, 256 |
| **write into subdirectory** | **FAIL** |

Two things ruled **out**:

- The wad writer's seek-back-and-rewrite works fine, so `write_wad()` is not
  the problem. That was my first hypothesis and it was wrong.
- `stat()` rounding to 1024 does not matter. `OpenedFile::GetLength`
  (`FileHandler_SDL.cpp:161`) seeks to `SEEK_END` rather than calling stat, and
  seek reports the true 256.

What's actually broken: **the ramdisk is flat.** `mkdir` returns EINVAL and
files cannot be created inside subdirectories. `shell_sdl.cpp` creates
`Saved Games`, `Recordings`, `MML` and `Themes` under `local_data_dir` at
startup, and all four fail silently.

Also worth noting: a mistake of mine earlier in the session. I "tested"
`dc/fs_mem.c` against modern KOS and concluded it would not compile. That test
was bogus — this shell is zsh, which does not word-split unquoted `$KOS_CFLAGS`,
so the whole flag string went in as one argument and every include path was
lost. Replacing fs_mem.c was still right (fs_ramdisk supersedes it, and
syscalls.c genuinely does duplicate newlib symbols), but the compile-failure
evidence I cited for it was not real.

Next: instrument the real game. Add framebuffer tracing around
`w_open_preferences_file` / `load_preferences` / `w_write_preferences_file` to
see which branch actually sets error 4, rather than reasoning about it from the
source.

## Work queue

Worked top to bottom. Tick items off in this file as they land, and note what
was learned even when something is abandoned.

- [ ] **A. Error 4.** Instrument `wad_prefs.cpp` with framebuffer tracing to see
      which branch sets it. Suspect the flat ramdisk, but prove it. Candidate
      fixes once known: make the subdirectory creation failures non-fatal, or
      flatten the paths so nothing needs a subdirectory.
- [ ] **B. DC controller driver.** `dc/dc_input.c`: poll
      `maple_enum_type(0, MAPLE_FUNC_CONTROLLER)`, diff against the previous
      state, inject with `SDL_PrivateKeyboard` — confirmed exported from
      libSDL.a, and unlike `SDL_PushEvent` it updates the key-state array that
      `vbl_sdl.cpp:98` reads via `SDL_GetKeyState`. Map the D-pad to **both**
      arrows and the keypad, so it drives the menu (UP/DOWN/RETURN) and the game
      (standard setup is KP8/KP5/KP4/KP6) without touching the default key
      bindings. A -> RETURN + SPACE, B -> LALT, X -> TAB, Y -> M,
      Start -> ESCAPE, triggers -> LSHIFT / LCTRL, analog stick -> arrows.
- [ ] **C. Cursor trails.** Hide the SDL cursor on DC; the software cursor blits
      without restoring the background.
- [ ] **D. Input injection.** Drive Flycast from osascript so gameplay can be
      verified, not just the menu. Unproven — if it does not work, say so and
      fall back to verifying boot-and-menu only.
- [ ] **E. Gameplay.** Once past the menu: does a level load, does it render,
      does sound work, what is the framerate.

## Rules for unattended work

- Commit and push to `origin/dc-rebuild` after every green build.
- Never delete, never force-push, never rewrite history.
- If a change does not build, revert it fully rather than leaving it half-applied.
- Prefer evidence from a booted probe over reasoning about 2002 source.
- `${=VAR}` when passing KOS flags in this zsh shell.
- Flycast fails to launch roughly half the time with a VMEM assertion; retry in
  a loop rather than concluding the image is broken.

### 00:37 — Error 4 root cause: MacBinary I. Fixed (built, not yet confirmed in-game)

First, a tooling trap that cost me a wrong diagnosis, and which anything working
in this tree needs to know:

**`grep` here is `ugrep`, and it silently skips non-UTF-8 files.** 61 of the
source files in `Source_Files/` are ISO-8859, including `wad.cpp`, `render.cpp`,
`player.cpp` and `map.h`. Searching them returns *nothing* — no match, no
warning, no error. `grep -a` helps but is still unreliable here. **Search this
tree with Python**, not grep.

That is exactly how I got error 4 wrong. I searched for `errUnknownWadVersion`,
found one hit in `wad_prefs.cpp:291`, and concluded with some confidence that
the failure was the preferences wad and not the map. There is a second site —
`wad.cpp:138`, inside `read_wad_header` — and ugrep hid it because `wad.cpp` is
not UTF-8. The dialog was telling the truth all along: it is the map.

**The actual cause.** `read_wad_header` rejects a file when

    header->version > 4 || header->data_version > 2 || header->wad_count < 1

The retail `Map` begins `00 03 4d 61 70 00`, which parses as version 3 and
data_version 0x4D61 — the "Ma" of "Map". The wad header is not at offset 0,
because the file is wrapped in a **MacBinary** container. The real header sits
at offset 128 and reads version 2, data_version 1, **wad_count 41** — the full
41-level campaign.

Aleph One already handles MacBinary transparently, but `is_macbinary()` in
`FileHandler_SDL.cpp` demanded bytes 122 and 123 be >= 0x81. That is the
MacBinary **II** version stamp, and the retail Map is MacBinary **I** — both
bytes are zero — so detection bailed at the first test and the wrapper was
never stripped.

The demo data was never affected because it ships as AppleSingle (magic
0x00051600), which is detected separately just above.

Container survey of the four retail files:

| file | container | notes |
|---|---|---|
| `Map` | MacBinary I | data 5,479,687 + rsrc 14,998,386 + 128 = 20,478,322 vs 20,478,336 on disc |
| `Images` | MacBinary II | bytes 122/123 = 0x81, so it always worked — which is why the title screen rendered |
| `Shapes` | none | byte 74 = 195, correctly rejected and read raw |
| `Sounds` | none | correctly rejected and read raw |

**Fix:** `is_macbinary()` now accepts MacBinary I. The CRC test is applied only
when the II version stamp is present, since I has no meaningful CRC. To avoid
mistaking a raw wad for a container without a CRC to lean on, MacBinary I
additionally has to have plausible fork sizes that fit inside the file.

Built green, ELF 14,612,320 bytes, and the menu still comes up with no
regression. **Not yet confirmed past the menu** — see below.

### 00:37 — Input injection blocked (item D)

`osascript` refuses: *"osascript is not allowed to send keystrokes. (1002)"*.
Sending keys needs Accessibility permission for Terminal, which is a system
setting only Max can grant:

  System Settings -> Privacy & Security -> Accessibility -> enable Terminal

Flycast's own config is fine — `device1 = 5` is `MDT_Keyboard`, so a Dreamcast
keyboard is already emulated on port A, which is what BERO's UP/DOWN/RETURN menu
navigation needs. The only thing missing is permission to press the keys.

Until that is granted I can verify boot and menu, but not gameplay. So the
MacBinary fix is **built and verified at the byte level, not observed working
in-game**. Stating that plainly rather than claiming the level loads.

### 00:45 — MacBinary fix CONFIRMED. New blocker: no video in-game.

Max pressed Return on the keyboard and **the level loaded, with sound**. Error 4
is gone. The MacBinary I fix is confirmed working end to end, and a Dreamcast
keyboard in Flycast (`device1 = 5`, `MDT_Keyboard`) drives BERO's menu
navigation without any changes needed.

New symptom: gameplay runs — audio plays — but nothing is drawn.

What the SDL DC video driver actually does, from
`kos-ports/SDL/dist/SDL-/src/video/dc/SDL_dcvideo.c`:

- `DC_SetVideoMode` only maps **320x240** and **640x480** to a display mode.
  Anything else returns NULL. All four of the first `ViewSizes` entries are
  640x480 overall, so the size Aleph One asks for is fine.
- `switch(bpp)` accepts 15/16/24/32 but **not 8**. An 8-bit request returns NULL,
  which would make Aleph One `exit(1)` — it didn't, so we are at 16-bit.
- `DC_UpdateRects` is a **no-op unless the textured driver is active**:

      static void DC_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
      {
          if (!SDL_DC_USING_GL() && sdl_dc_textured)
              sdl_dc_blit_textured();
      }

  That looked like the culprit, because gameplay updates a sub-rectangle while
  the menu does a full-screen `SDL_UpdateRect(main_surface, 0,0,0,0)`. It is
  **not** the culprit: in the non-textured path `current->pixels = vram_l`
  (line 548) — writes land directly in video RAM, so a no-op UpdateRects is
  harmless. The display path is fine either way.

So the blit path works and the mode is valid. Suspicion moves to the renderer
itself, or to `world_pixels` never being drawn into. Candidates, in order:

1. `render_view()` producing nothing — possibly the shapes/textures not loading
   from the retail `Shapes` (which is *not* MacBinary-wrapped, unlike `Map`).
2. Memory pressure. 16MB of RAM against a 20MB `Map` and a 10MB `Shapes`.
   `reallocate_world_pixels` needs ~614KB for a 640x480x16 surface on top.
3. `clear_screen()` on mode change leaving black and nothing overwriting it.

Asked Max for a cheap diagnostic that separates these: whether the HUD draws,
and whether the overhead map (the `m` key, `_toggle_map`) draws. HUD or map
visible means the display path is alive and only the 3D view is dead, which
points hard at (1). Nothing visible at all points at the display path after the
in-game mode switch.

### 01:03 — IT RENDERS. Root cause: SDL_BlitSurface silently no-ops.

The game draws. Textured walls and floor, a BOB and a security guard, the pistol
in view. The bottom 160px (HUD area) is still black — next job.

How it was found, because the wrong turns matter as much as the right one:

1. **My own Makefile was hiding my edits.** `shell_sdl.o` does not exist —
   Aleph One picks its platform layer by `#include`ing one .cpp from another,
   so `shell_sdl.cpp` is compiled as part of `shell.cpp` and make cannot see it
   as an input. BERO's Makefile carried `$(MI)/shell.o : $(MI)/shell_sdl.cpp`
   for exactly this reason and I dropped that line when rewriting it. Every edit
   I made to `shell_sdl.cpp` after the first full build produced an unchanged
   binary. Nine files are affected; all now have explicit rules.

2. **Tracing over serial.** Once Max enabled Flycast's serial console, KOS's
   printf became visible, which beat drawing to the framebuffer. `dc_trace()`
   now writes to both.

3. **The evidence chain.** The mode was right (640x480x16, pixels at
   `0xa5000000`, i.e. VRAM). `render_view` was running. Sampling the renderer's
   own buffer showed **12389 of 12800 pixels non-zero, changing every frame** —
   so a complete scene was being drawn. And `SDL_BlitSurface` returned **0, for
   success**, with a full 640x480 clip rect, while the destination VRAM pixel
   read `0x0000` both before and after the call.

   The tell was that bfont text written to the same VRAM address survived
   across frames. It could not have, if the blit were landing.

**Fix:** copy the rows directly instead of calling `SDL_BlitSurface`. Both
surfaces are 16-bit and 640 wide, so it is a plain `memcpy` per row. Whatever
SDL 1.2's blit map does with a hardware destination on this driver, it does not
copy, and it does not report an error either.

Next: the HUD band at the bottom is black. Very likely the same cause in
`HUDRenderer_SW` / `draw_interface`, which will use the same SDL blit path.

### 01:07 — HUD renders. The game is visually complete.

Motion sensor, weapons panel (".44 MAGNUM MEGA CLASS" with ammo pips), health
and oxygen bars, all under the 3D view. Same cause and same fix as the world
blit: `DrawHUD` (`screen_sdl.cpp`) blits `HUD_Buffer` to `main_surface` and that
blit no-ops too.

Refactored the one-off row copy into `dc_copy_to_screen()`, which now serves
both the world view and the HUD. It clips against both surfaces rather than
trusting the caller's rects, and returns false for anything not 16-bit so the
caller can fall back to `SDL_BlitSurface`.

Worth noting for whoever picks this up: not every blit to the screen is broken.
The main menu's title screen goes through `images_sdl.cpp` straight to the video
surface and has always worked. Only these same-format surface-to-display blits
fail, which is why the menu looked fine while the game was black.

- [x] **A. Error 4** — MacBinary I detection.
- [ ] **B. DC controller driver** — still open, but far less urgent than it was:
      Flycast emulates a DC keyboard (`device1 = 5`) and BERO's UP/DOWN/RETURN
      navigation works, so the game is playable as-is with a keyboard. A real
      controller is still the right thing for hardware.
- [ ] **C. Cursor trails** — still open.
- [x] **D. Input injection** — Accessibility granted, `osascript` key injection
      confirmed working against Flycast. Also added the AUTOSTART marker so a
      test run needs no input at all; both routes work.
- [ ] **E. Gameplay** — renders and runs. Not yet checked: framerate, sound
      beyond "it plays", whether the level is completable, whether saving works
      given the ramdisk is wiped at power-off.

### 01:11 — Cursor trails fixed (item C)

`show_cursor()` in `mouse_sdl.cpp` is the single place the pointer is made
visible, plus one direct `SDL_ShowCursor(true)` in `sdl_dialogs.cpp`. Both are
now no-ops under `-DDC`, so the cursor never appears. SDL's software cursor on
this driver blits itself without restoring the background, which is why every
mouse movement smeared arrows across the menu.

Nothing needs a pointer here: the menu is driven with UP/DOWN/RETURN.

Verified with a menu-only image (`alephone-menu.cdi`, built without the AUTOSTART
marker so it stays on the menu). Clean title screen, no smearing.

### 01:20 — Playable. 30 fps, input confirmed moving the player (item E)

Added an fps/state trace to the render path. It answers two things at once.

**Framerate: a steady 29–30 fps.** That is the Marathon engine's own 30Hz tick
target, so the software renderer is keeping up on a 200MHz SH-4 at 640x320 with
no frames being dropped. No optimisation needed; the PowerVR is not required.

**Input reaches the player.** With injected right-arrow presses:

    fps 30.3  yaw=382  pos=10775,12663
    fps 30.3  yaw=337  pos=10512,11024
    fps 30.3  yaw=240  pos=9917,10657
    Key: 0x4F (79), SDL Key: 0x0113        <- SDLK_RIGHT

yaw swings 382 -> 337 -> 240 and the position changes. The game is being played.

**What made it work:** the default key layout. Aleph One's `_standard_keyboard_setup`
puts movement on the numeric keypad (KP8/KP5/KP4/KP6). Those never arrived —
Flycast's host-to-DC keyboard mapping did not deliver them, and a Dreamcast
keyboard may not have a keypad at all. `preferences.cpp` now defaults to
`_left_handed_keyboard_setup` under `-DDC`, which uses the arrow keys. Those map
cleanly (`SDLK_RIGHT` above) and are the natural target for a D-pad once
controller support lands.

Max confirmed independently that `z`/`x` strafe, which fits: letter keys were
always getting through, only the keypad was not.

Dead end worth recording: my first injection attempt sent zero key events
because `osascript -e 'tell application "System Events" to repeat 40 times ...'`
does not parse as a one-liner. A single `key code N` works inline, but a repeat
block needs a real multi-line `tell ... end tell` script. I wasted a test cycle
concluding "input does not reach the game" when nothing had actually been sent.

### 01:30 — Correction: the 30 fps figure is NOT a hardware measurement

Max caught this and he is right. The 29-30 fps I measured came from Flycast on
an Apple-silicon Mac. Flycast's SH-4 dynarec does not throttle to real 200MHz
silicon, so that number is an **upper bound from emulation**, not evidence about
hardware. The engine also caps at a 30Hz tick, so "30 fps" only says it is not
falling below the cap under emulation.

The earlier claim that "the PowerVR is not needed for full speed" is withdrawn.
Nothing measured so far says anything trustworthy about framerate on a real
Dreamcast. Software rendering at 640x320 on a 200MHz SH-4 could well be much
slower, and the hardware-accelerated path (BERO's untested `GL=1`) may turn out
to matter after all. This needs a real console to settle.

### 01:30 — DC controller driver (item B): detects, mapping unverified

`dc/dc_input.c` reads the controller directly off the maple bus and injects the
result into SDL. `dc_input_poll()` is called from `main_event_loop`, so it
covers menus and gameplay alike.

Injection goes through `SDL_PrivateKeyboard`, not `SDL_PushEvent`, and that
choice is load-bearing: `SDL_PrivateKeyboard` updates SDL's internal key-state
array as well as queueing the event, and Aleph One polls `SDL_GetKeyState`
(`vbl_sdl.cpp:98`) for gameplay input. A pushed event alone would drive menus
but leave the player standing still.

Mapping targets the arrow-key layout, now the Dreamcast default, so the D-pad
serves menus and movement without rebinding:

| control | key | effect |
|---|---|---|
| D-pad / analog stick | arrows | menu nav, move and turn |
| A | RETURN + SPACE | menu select / primary trigger |
| B | LALT | secondary trigger |
| X | TAB | action (switches, terminals) |
| Y | M | overhead map |
| Start | ESCAPE | pause / abort |
| L / R triggers | Z / X | sidestep left / right |

**Status: detection confirmed, mapping not yet exercised.** With a Sega
Controller configured on port A the trace reports `controller: found, polling`,
so enumeration and status reads work. No button events have been generated yet
because Flycast is not routing synthesised host keys to the emulated D-pad.
Verifying the table needs either a keyboard-to-gamepad mapping in Flycast's
input settings, or a human pressing an actual pad.

Dead end worth recording: my first two attempts reported "none found on the
maple bus" even with a controller configured. Cause was ordering, not the
driver — I edited `emu.cfg` and *then* killed Flycast, and Flycast rewrites its
config on quit, silently reverting the edit. Kill first, then edit, then launch.
