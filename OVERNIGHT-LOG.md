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
