# Aleph One 0.12.0 on Dreamcast, modern toolchain

Marathon 2: Durandal running on a Sega Dreamcast, built with a 2026 KallistiOS
toolchain instead of the 2002 one the port was written against.

This is BERO's `AlephOne-0.12.0-dc-1` port (2002, sdl-dc.sourceforge.net) applied
to a pristine Aleph One 0.12.0 tarball, plus the changes needed to build it on
sh-elf-gcc 15.2.0. Nothing about the game was rewritten.

## Build

You need KallistiOS at `/opt/toolchains/dc/kos`, with `kos-ports` providing SDL
1.2 and libGL, and `mkdcdisc`.

```sh
./tools/fetch-data.sh     # full Marathon 2 retail data (once, ~29MB download)
./build.sh -j8            # -> alephone.elf
./build.sh flycast        # build a test image and run it
```

### Game data

Aleph One ships no game data. `tools/fetch-data.sh` pulls the full retail
Marathon 2 from Aleph One's GitHub releases — Bungie made the trilogy freely
available in 2005 and granted the project a distribution license in 2021. Free
for noncommercial use; Bungie keeps the copyright.

The modern package names its files `Map.sceA`, `Shapes.shpA`, `Sounds.sndA` and
`Images.imgA`. Aleph One 0.12.0 predates those extensions and wants bare names,
so the script renames them. The contents are the original Bungie files, so the
formats match. `Plugins/`, `Scripts/` and `Physics Models/` are deliberately
skipped: they target Aleph One 1.x and an MML dialect 0.12.0 cannot parse.

|          | demo    | retail    |
|----------|---------|-----------|
| `Map`    | 2.4 MB  | 20.5 MB   |
| `Shapes` | 5.3 MB  | 10.0 MB   |
| `Sounds` | 3.75 MB | 14.2 MB   |
| `Images` | 3.5 MB  | 4.1 MB    |

Always go through `build.sh`; it sources `environ.sh`, which is where `KOS_BASE`,
`KOS_PORTS`, `KOS_CFLAGS`, `KOS_LDFLAGS` and `KOS_LIBS` come from. Calling `make
-f Makefile.dc` directly fails on purpose.

### Targets

| Target      | Result                                                     |
|-------------|------------------------------------------------------------|
| *(default)* | `alephone.elf`                                             |
| `disc`      | stage `disc/AlephOne` from the skeleton plus the demo data  |
| `test`      | unpadded `.cdi`, ~20 MB — **emulator only**                 |
| `cdi`       | padded `.cdi` — this is the one that boots on hardware      |
| `gdi`       | GDEMU's native format                                       |
| `flycast`   | build the test image and launch it                          |
| `clean`     | objects and the elf                                         |
| `distclean` | also disc images and the staged disc tree                   |

**Do not burn `alephone-test.cdi`.** `-N` drops the data-track padding to keep
the image small for emulator runs. Unpadded images do not boot on a real
console — use `cdi`.

## What had to change, and why

Six things stood between BERO's 2002 source and a linking binary. All are
toolchain drift, not game bugs.

**1. A missing `typename`.** `sdl_widgets.h` declares

```cpp
virtual void draw_item(vector<T>::const_iterator i, ...) const = 0;
```

`vector<T>::const_iterator` is a dependent type. gcc 2.95 accepted it bare; gcc
15 parses the parameter as `int`. Every `draw_item` override then had a
different signature from the pure virtual, so `w_levels`, `w_env_list`,
`w_read_file_list` and `w_write_file_list` all stayed abstract and five
translation units failed on `invalid new-expression of abstract class type`.
One keyword fixed all of it.

**2. `struct dirent` lost its `size`.** `FileHandler_SDL.cpp` read `de->size`
and treated a negative value as "directory". That was a KOS 1.1.7 extension.
Modern KOS uses the standard newlib `dirent`, which has neither — but its
iso9660 driver implements `stat()`, so the generic non-DC code path now works
on Dreamcast and the special case is gone.

**3. One surviving `try`/`catch`.** The port builds `-fno-exceptions`, which is
why BERO rewrote `try`/`throw` as `goto` elsewhere. He missed `wad_prefs.cpp`;
it is now `#ifndef DC`. A failed allocation there is fatal rather than
recoverable, which is the honest outcome on a 16 MB console.

**4. A gcc 15 SH4 codegen bug.** `weapons.cpp` at `-O2` produces:

```asm
mul.l   r1,r8
mov     #0,r0
mov.w   r0,@(10,macl)
```

`macl` is the multiply-accumulate result register, not a GPR, and cannot be the
base of a displacement address — the backend owed us an `sts macl,rN` first.
The assembler refuses it with `syntax error in @(disp,[Rn, gbr, pc])`. That
one file is pinned to `-O1`. It is game logic, not renderer code, so nothing
measurable is lost.

**5. The link line.** Three separate problems:

- SDL 1.2's Dreamcast video backend calls `glKosInit`, `glKosSwapBuffers` and
  ~48 core `gl*` entry points, so `-lGL` is required — even though Aleph One
  itself is built without `HAVE_OPENGL` and uses the software renderer. `nm`
  confirms our own `OGL_*.o` reference zero GL symbols.
- `KOS_LDFLAGS` passes `-nostdlib` and `KOS_LIBS` is C-only, so `-lstdc++
  -lsupc++` have to be named explicitly for `std::string`, `std::map` and
  `operator new`.
- BERO linked `startup.o` by hand as KOS 1.1.7 required. Modern KOS pulls it in
  through libkallisti and its own linker script, so naming it again gives
  `multiple definition of _arch_mem_top`. This is also why `environ.sh` leaves
  `KOS_START` empty.

**6. A name collision.** Pfhortran's script VM had a global `stack_top`, which
KallistiOS also defines. It is now `static`; nothing outside `scripting.cpp`
ever used it.

### The 2002 DC glue is gone

`dc/syscalls.c` and `dc/fs_mem.c` could not be reused. `syscalls.c` defines
`_read`, `_write`, `_open`, `_close`, `_lseek`, `_fstat`, `_stat`, `_sbrk` and
`_exit`, all of which modern KOS supplies through newlib — linking it gives
duplicate symbols. `fs_mem.c` hand-rolled an in-RAM filesystem at `/mem`
because KOS 1.1.7 had none; modern KOS ships exactly that as `fs_ramdisk`.

`dc/dc_compat.c` replaces both with the only two things still missing:
`fs_mem_init()`, now a call to `fs_ramdisk_init()`, and `access()`, implemented
via `stat()`.

`shell_sdl.cpp` accordingly points `local_data_dir` at `/ram` instead of
`/mem`. Game data is read from `/cd/AlephOne`.

### Things BERO warned about that no longer apply

His README says you must raise `MAX_ISO_FILES` from 8 in `fs_iso9660.c` and
rebuild KOS. The modern iso9660 driver uses dynamic handles and has no such
constant. Stock KOS is fine.

## Known gaps

- **Saves do not survive a power cycle.** Preferences, saved games and film
  recordings go to the ramdisk. VMU write-through is the obvious next job;
  BERO never implemented it either.
- **Keyboard required.** No Dreamcast controller mapping. His README says the
  same. This is the most worthwhile thing to fix next.
- **Software renderer only.** Built without `HAVE_OPENGL`. The PowerVR is
  sitting idle; `GL=1` was BERO's untested path.
- **No sound verification.** It links against KOS audio through SDL but has not
  been checked past the menu.
- `-fpermissive` and `-fno-strict-aliasing` are load-bearing. The renderer
  type-puns freely and the 2002 code leans on conversions the modern front end
  rejects.
- **Retail data on 16 MB of RAM is unproven.** `Sounds` alone is 14.2 MB and
  `Map` is 20.5 MB, against a 16 MB console. Aleph One streams both from disc
  rather than loading them whole, so it should hold, but memory pressure is the
  first thing to suspect if a level fails to load where the demo worked.

## Flycast

Flycast's address-space init is flaky on macOS: roughly half of all launches
die with

```
Verify Failed : &mem_b[0] == ((u8*)getContext()->sq_buffer + sizeof(Sh4Context) + 0x0C000000)
 in Init -> core/hw/sh4/dyna/driver.cpp : 349
```

and exit 6. It depends on where ASLR puts things, not on the disc image, and
`Dynarec.Enabled = no` does not help — the check runs before dynarec matters.
Just launch again. Use `open -a Flycast --args <disc>` rather than exec'ing the
binary.

## Provenance

- Aleph One 0.12.0 — `downloads.sourceforge.net/marathon/AlephOne-0.12.0.tar.gz`
- DC port — BERO, 2002, `AlephOne-0.12.0-dc-1`, GPL-2.0
- Marathon 2 retail data — Aleph One release `release-20250829`, `Marathon2-20250829-Data.zip`

Marathon is © Bungie. Aleph One is GPL-2.0; see `COPYING`.
