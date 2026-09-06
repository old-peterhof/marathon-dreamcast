# Independent follow-up — September 6, 2026

## Start here, Fable — not a hardware sign-off

Work is in `/Users/mgibbons/Desktop/GPT Misc/MarathonDC` only. The other
copy on Desktop remains at Fable's `1c29854`. Changes below are uncommitted.
Do not confuse the new replay experiments with the old b78 hardware image.
The user explicitly asked for independent implementation/test verification,
not acceptance of your earlier report. Preserve full texture quality and
gameplay, prefer deletion/simplification, and test moving gameplay, not just
boot or a stationary camera. They have now also requested rumble support.

### Latest checkpoint — user asked to stop and hand off

Rumble IS NOW IMPLEMENTED AS A FIRST PASS and cross-compiles successfully,
but has NOT been run in Flycast or tested with a physical pack. No host timing
tests were written before the user stopped work. Do not call it validated.
No Flycast process was running at handoff. No new hardware image was built.

New files: `dc/dc_rumble.c`, `dc/dc_rumble.h`. Integration: `dc/dc_input.c`,
`Source_Files/Misc/weapons.cpp`, `Source_Files/Misc/player.cpp`, `Makefile.dc`.
All follow-up changes remain uncommitted in this working copy only.

The latest executable/image is the rumble build:
`alephone-b78-residency-replay-rumble.cdi` (level 21, normal replay, no
UPLOADTEST/STATICTEST). `/tmp/alephone-rumble-build.log` records successful
GL=1 FAST=1 compilation/link/image generation. The latest `git diff --check`
also passed. Build log copied into `test-evidence/2026-09-06-audit/`.

Implemented behavior: strength 2/7 for small/missile pulses; deadlines of
70 ms shot, 90 ms hit, 220 ms missile. These are explicit STOP deadlines,
not verified physical pulse lengths. The packet is deliberately non-continuous,
motor 1, fpow=bpow=strength, freq=32, inc=1, no decay flags. KOS/device timing
may make it stop sooner. Tune on hardware, not by assuming these constants
translate to feel. A weaker event cannot overwrite a live stronger one.

The driver uses the first rumble expansion on the active controller's port,
re-enumerates devices, retries busy commands from polling, clears stale events
when no pack is present, and stops on leaving gameplay. No heap allocation,
threads, indefinite device waits, or simulation RNG were added.

Gameplay hooks: one event after ammo/underwater checks per gun discharge,
local player only, excluding melee and ball drops. Damage hook requires local,
alive, positive-energy player, positive damage, not absorbed or oxygen drain.
Energy-drain damage currently DOES rumble: review whether it counts as a hit.

Legacy weapons.cpp/player.cpp were converted to temporary UTF-8 copies for
apply_patch and converted back to ISO-8859-1. Diffs are localized; player.cpp
also lost its final extra blank line. No wholesale source re-encoding remains.

Known review points in the new code (not fixed before stop):

- Switching directly between two present packs/active controller ports can
  carry the pending old pulse to the new pack; decide to discard it instead.
- Same-slot unplug/replug between polls cannot be detected by port/unit alone.
  Do not describe hot-plug as proven safe without tests.
- Busy stop retry works only while input polling continues; non-continuous
  packets are the fallback. Verify pause, chapter screens, loading and shutdown.
- Check that `dc_input_set_ingame` enables effects during actual gameplay and
  that replay tests exercise the local-player hooks rather than assuming so.
- Test repeated automatic fire: generation counter resends equal-strength
  events, but device-native effect duration/stop behavior is unverified.

## Independently confirmed defects and changes

- `TextureState::Use` marked an upload ready before `PlaceTexture` succeeded.
  Upload success now controls readiness, and Setup rebuilds staging data for
  a failed normal/glow upload on the next draw.
- Static rendering deleted texture IDs while queued/current PVR scenes could
  still reference their VRAM. Retired IDs now survive until an idle frame
  boundary. This costs additional VRAM until that boundary; stress it.
- GLdc's allocator still automatically compacts during a failed allocation.
  World raw/VQ uploads now check contiguous capacity before calling it, and
  request safe next-frame eviction/compaction instead. This is NOT yet an audit
  of every UI/font upload call outside TextureManager.
- REJECTED and removed: full-size static silhouettes, packed 16-bit static
  staging, and same-bitmap/density per-frame reuse, tested together. Busy
  Waterloo scenes fell to 3–4 fps. The retained code still has Fable's quarter
  width/height static effect. That is a known unresolved quality compromise,
  NOT permission from the user to sacrifice texture quality.
- `tools/run-flycast.sh` accepts a fourth argument: monitoring duration in
  seconds. This holds the launch session, fails on early exit, and stops only
  its test PID at the deadline. A live process alone still proves nothing
  about simulation progress.

## Tests actually performed

Build with `./build.sh -j8 GL=1 FAST=1 replay-test FILM=...` (GL=1 matters).

- `/tmp/alephone-guarded.log`: quarter-size static plus retirement/VRAM guards.
  Moving Waterloo replay, no upload errors/unsafe allocator-defrag messages
  observed. Early busy samples are about 22–29 fps, not steady 30; later 30.
  Screenshot `/tmp/alephone-guarded.png` inspected: textured world, HUD, static
  silhouette. Much of the long tail is dead-player time, not traversal coverage.
- `/tmp/alephone-recovery.log`: level 21, `UPLOADTEST=1`, suffix `-recovery`.
  Contains `upload-test: injected failure` then `failed texture recovered`.
  Subsequently moving, alive gameplay around 30 fps. Short test, not a soak.
- `/tmp/alephone-fullstatic.log`: discarded startup/liveness-only attempt;
  process vanished before gameplay. NOT evidence of an engine crash or a pass.
- `/tmp/alephone-fullstatic-monitored.log`: rejected full-resolution static run,
  launched with `12 240`. Real moving gameplay; e.g. tick 189 at 3.2 fps,
  tick 223 at 4.4 fps. Later dead-player 30 fps is misleading. Experiment removed.
- `/tmp/alephone-handoff-build.log`: rebuild after removing the experiment,
  creates `alephone-b78-residency-replay-handoff.cdi`, level 21 + UPLOADTEST.
  This suffix distinguishes it from the original b78 image. The main build
  identity still says b78; assign a new build number before releasing anything.

The screenshot script captured a PNG but its final metadata helper failed
because that Python has no PIL. The image was opened independently.
Copies of the guarded/recovery/fullstatic-monitored logs and guarded screenshot
are preserved in `test-evidence/2026-09-06-audit/`, so `/tmp` cleanup will not
erase the main evidence. Final reverted-code GL replay rebuild succeeded;
`git diff --check` and `bash -n tools/run-flycast.sh` passed. A new monitored
soak of the final handoff image has NOT been run.

## Work queue, in order

1. **Review and validate the retained safety changes.** Inspect the actual
   diff, especially delayed deletes, upload retries including glow/VQ fallback,
   and the allocation-size guards. Force low/fragmented VRAM, not only a
   synthetic failed upload. The guard assumes PVR true-colour 16-bit storage;
   mip generation is conservatively budgeted for base + replacement chain.
   Audit UI/font paths outside TextureManager (screen_sdl.cpp around 585 and
   OGL_Textures.cpp's font texture path). Do not claim all mid-frame defrag is
   impossible: only the world raw/VQ paths were guarded. Check PVR-wait timeout
   behavior and texture teardown too.
2. **Finish and validate rumble (new user request).** Small pulses for actual shots and
   actual incoming hits; a bigger pulse for missile-launcher firing. A
   first-pass implementation now exists; see the latest checkpoint above.
   Detailed entry points and constraints below.
3. **Resolve static quality/performance without unsafe frees.** The full-size
   per-frame regeneration experiment failed. Consider reusing immutable shape
   masks/noise resources or a different rendering approach; measure it. The
   current retirement solution preserves storage safety but busy all-static
   scenes can dip below 30. Benchmark ordinary teleporting separately from
   STATICTEST, which changes every sprite including weapon-in-hand.
4. **Run meaningful gameplay/heap/save tests.** Use native films and player
   movement, quantify time alive and position/level progress, test death reloads
   and real level transitions. Include heavy level 21 and save/load round-trip,
   pause/preferences/map, flashes, teleporting. Further VMU-size reduction is
   still requested/deferred; 22 blocks is a measurement, not an improvement.
   Film desync remains unexplained, not proof of gameplay fidelity.
5. **Package and document honestly.** New build identity, GL=1 FAST=1, compile
   checks and monitored runs on the final code. Check AUTOSTART, PADTEST, DEBUG,
   PROFILE, AUTOKEY, FADETEST, STATICTEST, SAVETEST, UPLOADTEST and TestFilm.
   Existing verify-image only checks the first five and rebuilds the staging
   tree rather than inspecting an arbitrary CDI. Produce a marker-free hardware
   image without overwriting the old known image. Hardware testing remains
   essential; no console has been tested in this follow-up.

## Rumble implementation/review notes

These were the implementation plan. The first pass now exists; check it
against these requirements rather than reimplementing it from scratch.

User wording: "Rumbles when you shoot (small rumbles) and also when you get hit
(small rumbles). When you fire missile launcher it's a big rumble."

- Hook `fire_weapon()` in `Source_Files/Misc/weapons.cpp` (~1740), after the
  underwater/empty-ammo rejection and actual rounds-to-fire checks. One event
  per discharge, not one per shotgun pellet. Compare `player_index` with
  `local_player_index`; `_weapon_missile_launcher` is the weapon enum. Avoid
  rumbling for empty clicks, remote players, ball drops or an uncharged shot
  that spawned no rounds. Decide explicitly whether fists count (request was
  shooting; default to guns only).
- Hook `damage_player()` in `Source_Files/Misc/player.cpp` (~663), local player
  only, positive actual damage while alive. Avoid healing, oxygen depletion,
  invincibility-absorbed damage, and corpse damage. Keep effects out of the
  deterministic simulation state and RNG.
- Add a small C module/header (C linkage for the C++ hooks). Pump its nonblocking
  state from `dc_input_poll()` in `dc/dc_input.c` (~471); stop on
  `dc_input_set_ingame(0)` for menus/pause and on disconnect. `dc_input` selects
  `maple_enum_type(0, MAPLE_FUNC_CONTROLLER)`; select a rumble expansion on THAT
  controller's port, not another player's pack. Re-enumerate instead of caching
  a device pointer through hot-unplug. No device must be a cheap no-op.
- Installed KOS API is in
  `/opt/toolchains/dc/kos/kernel/arch/dreamcast/include/dc/maple/purupuru.h`.
  `purupuru_effect_t`: motor must be nonzero (normally 1), fpow/bpow 0–7,
  freq typically 4–59; conv/div must not both be set. `inc` is inclination
  period, NOT a reliable millisecond duration. Hardware differs; the header
  specifically warns decay can produce no vibration on Sega packs. Use short
  bounded pulses, real-time deadlines and explicit stop; don't assume a field
  named duration in the deprecated API means portable duration. Starting tune
  only: ~70 ms mild shot, ~90 ms mild hit, ~220 ms stronger missile. Let a weak
  hit/shot never truncate a stronger ongoing missile pulse.
- `purupuru_rumble()` implementation at
  `/opt/toolchains/dc/kos/kernel/arch/dreamcast/hardware/maple/purupuru.c`
  uses `maple_frame_trylock` and queues a frame; MAPLE_EAGAIN must not cause
  a busy wait. Retry from polling with expiry, including stop retries. Test
  without pack, with pack, hot-unplug/replug, busy bus, pause while vibrating,
  automatic fire, overlapping hit/missile, and both expansion slots. Unit-test
  scheduling with a fake clock/device, then verify Maple effects in Flycast
  configured for a rumble accessory. Actual strength/feel still needs hardware.

IMPORTANT: read `dc/dc_maple.c` before changing initialization. This port already
has a documented hardware boot hang with an attached rumble pack. It replaces
KOS's unbounded initial scan with a bounded one while keeping INIT_PURUPURU.
Do not reintroduce `maple_wait_scan()` or wait indefinitely for an accessory.
The installed KOS rumble example itself has a blocking device-attach loop;
that is NOT suitable for this game.

Some old source files, including player.cpp, contain legacy non-UTF8 comments.
Preserve encoding when editing; do not silently rewrite entire files.

## Reproduction commands

For the latest rumble code (compile passed, runtime NOT yet tested):

```sh
./build.sh -j8 GL=1 FAST=1 replay-test \
  'FILM=/tmp/marathon2-replays/Marathon2_TC_JimM_films/21.My Own Private Thermopylae' \
  REPLAY_SUFFIX=-rumble
tools/run-flycast.sh alephone-b78-residency-replay-rumble.cdi \
  /tmp/alephone-rumble-soak.log 12 900
```

Configure a rumble accessory in Flycast first for command testing; the previous
configuration had a controller and two VMUs, not a rumble pack. Preserve VMU
save files/configuration. Use `rumble: power=` traces to confirm commands,
then test a missile explicitly; level 21's replay may not fire that weapon.

For upload-failure recovery:

```sh
./build.sh -j8 GL=1 FAST=1 replay-test \
  'FILM=/tmp/marathon2-replays/Marathon2_TC_JimM_films/21.My Own Private Thermopylae' \
  UPLOADTEST=1 REPLAY_SUFFIX=-handoff
tools/run-flycast.sh alephone-b78-residency-replay-handoff.cdi \
  /tmp/alephone-handoff-soak.log 12 900
```

Use exec's session/yield support and inspect logs during the 15-minute run;
do not block all communication until it finishes. The launcher stops its PID
at the end. `rg -a` handles the non-UTF8 Maple device strings in serial logs.
For the static stress test substitute film `1.Waterloo Waterpark`,
`STATICTEST=1`, and a different suffix. No new Flycast source build was needed
or attempted in this follow-up. Startup VMEM/ASLR assertions are still retried.

## Completion gates for pushing the port further

1. **Correctness baseline:** fresh build plus host rumble scheduling tests,
   packet traces, fault-injected texture retries, and no unsafe texture movement
   through any upload path. Record failures, not just successful boot logs.
2. **Representative gameplay:** several alive, moving level traversals,
   collection-heavy transitions, repeated death/reload, and save/load round-trip.
   Compare replay/physics behavior with a known-good reference before blaming
   the films. Separate loading, alive-play and dead-player FPS statistics.
3. **Measured resource budgets:** track live heap, heap high-water, stack
   headroom, largest contiguous VRAM and upload stalls over an extended session.
   Optimize the largest measured transient/duplicate next; don't reduce stock
   artwork or gameplay to make an arbitrary average fit.
4. **Visual/performance fidelity:** fix teleporting with a measured alternative
   to full-sprite regeneration, inspect fades, skies, map, HUD and menus, and
   keep a regression route. Flycast FPS alone is not Dreamcast performance.
5. **Hardware release candidate:** new versioned marker-free image, both rumble
   slots and hot-plug tests, actual vibration tuning, long console sessions and
   VMU round-trip. Clearly document any remaining limitations before claiming
   the port is done. Preserve the old build as a rollback candidate.

---

## Follow-up, 6 September 09:00-10:00 (Fable)

Everything below is committed in the working copy; the original tree is
synchronised at the end of the session.

- **Texture safety changes reviewed and kept** (`cab043c`): upload readiness
  follows PlaceTexture's result, failed normal/glow uploads rebuild and retry,
  the allocation-size guard defers a raw or VQ upload that would not fit the
  largest free block and raises the frame-boundary eviction target instead.
  UPLOADTEST reproduced the injected failure and the recovery line.
- **Retired-texture list replaced.** It waited for `pvr_wait_ready()` and
  `pvr_wait_render_done()` at the start of every frame with a static sprite on
  screen, serialising CPU and GPU, which is why all-static scenes ran 22-29 fps.
  The static texture object is now kept and re-uploaded in place: GLdc writes
  the same VRAM block for an existing texture of the same size, so nothing is
  freed while a queued polygon holds the address, and nothing waits. STATICTEST
  on Waterloo: 28-30 fps. Ordinary teleports are a few sprites, not all of them.
- **Rumble validated** (`f1d4787`): with Flycast's port A slot 2 set to a Puru
  Puru pack (`emu.cfg` `device1.2 = 3`, backup `emu.cfg.pre-claude-2026-09-06-
  rumble`) the pack enumerates at port 0 unit 2, every film hit and every
  manual trigger pull logs a request, a power-2 command and a stop after the
  deadline. Missile branch untested (no launcher reached). Feel wants a console.
- **Hooks checked against the plan**: one event per trigger pull after the
  round count is pinned; hit hook reads energy before damage is applied; KOS
  `purupuru_rumble` returns EAGAIN without blocking and the poll retries.
- **Saved games were never restored at boot** (`b84a263`): `dc_vmu_load_saves()`
  ran before the bounded Maple scan attached the card, found an empty /vmu and
  said nothing. Moved after the scan. Round trip verified: SAVETEST save, clean
  Flycast quit, reboot into the loadtest image -> `vmu: restored 2 saved
  game(s)`, CONTINUE GAME loads slot 1, the world comes back as saved. Note for
  emulator testing: Flycast writes its card image only on a clean quit
  (`osascript -e 'quit app "Flycast"'`); a `pkill` loses the session's saves.
- **Build b79 "rumble"** built from `b84a263`: `alephone-b79-rumble.cdi`
  (padded, marker-free, verify-image clean) and `alephone-b79-rumble-play.cdi`,
  in both trees. b78 images are untouched as the rollback candidate.
- **Queue item 3 (static quality) measured and settled.** STATICTEST on
  Waterloo (every sprite static, an artificial worst case), fps over the first
  eight seconds:

  | Static texture size | fps | Look |
  |---|---|---|
  | quarter, fixed shift | 28-30 | large blocks on near sprites |
  | half, fixed shift | 12-30 | close to the software renderer |
  | half, 16-bit staging | 12-30 | same; the staging format is not the cost |
  | texel budget 8192 | 8-23 | many small sprites go full size |
  | **texel budget 4096 (kept)** | 16-29 | real static |

  The cost is the per-frame rebuild and upload, so it is now bounded per
  sprite by texel count (`STATIC_TEXEL_BUDGET` in `OGL_Textures.cpp`) rather
  than by a fixed shrink: sprites up to 64x64 keep full size, up to 128x64
  half, larger ones a quarter, 8x8 minimum. The largest monster sprite costs
  at most twice its quarter-size cost; eight monsters teleporting together is
  about the all-static quarter load that ran 28-30. The 16-bit staging path
  was reverted as it bought nothing measurable. Screenshots: `test-evidence/
  2026-09-06-audit/static-{quarter,half,budget4k}.png`.
- Item 4's save/load round trip is done; films, heap and map were covered
  overnight. Hardware remains untested.
- **Moving gameplay, measured** (the audit asked for it): b80 soaked films 21,
  8, 2, 5, 11, 16, 19, 24, no fatals, no failed uploads, heap tops within the
  b78 envelope. The fps traces showed 3-7 seconds at 1-3 fps after every level
  start on films 2 and 5. The VQ sprite pack was still read through stdio, one
  GD-ROM command per sector per new sprite frame; `5540eb5` routes it through
  `dc_read_file_span()`. Film 2: 21 sub-10 fps seconds of 279 before, 7 of 284
  after, all of them the load second. Build b81 "vqspan".
- **Saves now stay on the card until chosen.** With the restore working, boot
  decompressed every save into the ramdisk and kept it: 430 KB live for two
  saves on level 21 (heap top 0x8cf06000 at the reload). The boot scan reads
  headers only and `dc_vmu_restore_slot()` restores one save on demand,
  dropping other slots' copies. Live heap after a Continue load: 11,741 KB
  (was 12,472). The b79 images were rebuilt from this commit.
- **Heap on the final b79** (`/tmp/alephone-b79-l21d.log`, level 21): heap top
  0x8cdc4000 after the first load and 0x8ceba000 after the death reload, the
  same as b78 with the sound flush. The morning's extra 430 KB was entirely the
  boot-time save copies.
