# Working on this port

Orientation for an agent picking this up. Everything factual lives in the docs
below — this file exists to say which one to read, what the constraints are, and
where things stand.

## Read in this order

| | |
|---|---|
| `README.DC.md` | How to build, run and test. Start here. Also two search traps that have already produced one confidently wrong diagnosis. |
| `BUILDS.md` | Every disc image, its number, slug, commit and what changed. Quote `b<N> <slug>` when discussing a build. |
| `BUGS.md` | Open issues first, fixed ones below with the build that fixed them. Read the open table before touching anything. |
| `BACKLOG.md` | What is left, roughly by appetite. Completed sections are kept because the reasoning is the useful part. |
| `UI-HANDOFF.md` | The interface: decisions, the 480i rules with measurements, the palette, all ten screens, and what the engine still needs. Has a **Settled** section — those are closed, do not reopen them. |
| `MENU-TREE.md` | Audit of every menu, dialog and setting, and what each is worth on a console. |
| `LOOP-LOG.md`, `OVERNIGHT-LOG.md` | Working notes from unattended sessions. Includes things that were got wrong and corrected. |

## What this machine needs that the repo does not carry

- **KallistiOS at `/opt/toolchains/dc/kos`**, with kos-ports providing SDL 1.2
  and libGL, plus `mkdcdisc`.
- **`tools/fetch-data.sh`** once, for the Marathon 2 retail data (~29MB).
- **Disc images are gitignored.** They are ~740MB, over GitHub's file limit.
  `BUILD_NUMBER` and `BUILD_NAME` are committed, so `./build.sh cdi` reproduces
  the current image with the right stamp on the menu.

**If you are building the toolchain on a fresh machine, match the flags.**
`KOS_CFLAGS` in `environ.sh` carries `-O3`, `-fbuiltin -ffast-math
-ffp-contract=fast` and `-flto=auto -ffat-lto-objects`, and they reach this
project's compiles and its link. `BACKLOG.md` records how each was tested and
that together they bought about +1fps — so if they are missing, builds are
comparable to each other but not to the recorded ones.

## Hard constraints

- **Never rebuild or update the toolchain at `/opt/toolchains/dc` on Max's
  machine.** This port depends on that exact install. New kos-ports only.
- **`cdi` is the only target safe on hardware.** `test`, `play`, `-debug` and
  `-slow` are unpadded or carry markers. `tools/verify-image.sh` checks before
  you hand one over.
- **The terminals are never touched.** Original graphics, original text. Not a
  defect, not an open question, and it has been raised and closed more than once.
- **Look for an upstream Aleph One solution before designing against the 2001
  code.** Cost the lift against the `gnu++98` pin. Several problems assumed to
  need a rewrite turned out to be one number.
- **Flycast:** launch with `tools/run-flycast.sh`, which retries past its ASLR
  assertion and captures KOS stdout. Flycast holds 30fps because that is the
  engine tick, so it says nothing about renderer speed.

## What you cannot verify, and who can

Only Max can test on hardware, and the difference matters more here than on most
projects: at least one failure reproduces **only** on a console, Flycast is
faster than the real drive, and the framerate on the VMU is the only real number.

So: build, verify the image, and be explicit about which claims are proven and
which are inference. Object-level checks — `nm`, the compiled `.o`, a string in
the binary — are worth more than a plausible argument, and have twice overturned
one here.

## How Max works

- **Batch fixes into one build.** Three discs in a session makes a regression
  harder to find, not easier. Accumulate, then build when asked or when there is
  a reason to draw a line.
- **Roll back means the whole change**, not the half that looks guilty.
- **Do what was asked at the size asked.** Note out-of-scope defects and move on.
- **Raise a caveat once.** After it is settled it goes in a Settled list and is
  not brought up again.
- Commits go straight to `dc-rebuild`, which is the default branch and where
  every prior build landed.

## Where things stand

**b69 is built and untested.** `alephone-b69-sound-freeze.cdi`. Two fixes:
Stereo and Quality now apply at the next launch rather than reopening the audio
device, which is what froze and crashed; and the Controls panel no longer runs
34px past the explainer line. Both need a console.

**Confirmed working on hardware:** b68's bounded maple scan — the game boots with
a third-party rumble pack fitted, which it never did before. b67's contrast lift
and the held-direction repeat in menus.

**On the pile, not built:**

1. The pause menu re-blends the whole 640×480 screen in software on every cursor
   move, then pushes a full `SDL_UpdateRect`. Diagnosed in `BUGS.md`, fix
   designed, not written. It rests on one unverified assumption: that nothing
   repaints the framebuffer between cursor moves.
2. Whether `fs_vmu` remounts a VMU pulled and reinserted mid-session.
3. Native keyboard and mouse are **compiled in and never tested** — see
   `BACKLOG.md`. A keyboard would drive `w_text_entry`, which is the one widget a
   pad cannot work. Needs peripherals, not code.
4. The PowerVR renderer measured on hardware. `BACKLOG.md` calls it the only
   number that matters, and it decides whether the renderer is where the frames
   are.

## The interface prototype

`mockups/prototype/` is the design as a working, pad-navigable prototype rather
than a picture. It is ahead of the engine in places, and `UI-HANDOFF.md` says
which.

    cd mockups/prototype
    python3 build.py && python3 check.py     # after editing anything
    open index.html

- `strings.md` holds all 121 player-visible strings; `build.py` reads it, so copy
  is edited without touching code, and a missing key fails the build loudly.
- `check.py` is the gate. Contrast, one-pixel horizontals, the 560×400 safe area,
  icon size, label collisions and text overflow, across sixteen screen and
  overlay states. A deliberate CSS exception takes a trailing `/* 480i-ok */`.
- `shots.py --sheet --cols=3 --overscan` renders the set into one image.
- `plate.py` bakes the static background bitmaps into `assets/`.
- `tools/bake-plate.py` is the engine-side bake, from the prototype's own CSS.
