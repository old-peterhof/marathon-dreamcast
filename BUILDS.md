# Builds

Every disc image is named `alephone-b<N>-<slug>.cdi`, and the same `b<N> <slug>`
is drawn at the bottom-left of the main menu. Quote either when reporting how a
build behaved and there is no ambiguity about which one you mean.

The tag is also burned into the disc volume label, so it survives even if the
file gets renamed on the way to an SD card.

Variants of the same build share its number:

| Suffix | Contents |
|---|---|
| *(none)* | the real thing — no markers, safe for hardware |
| `-debug` | adds on-screen `dc_trace` diagnostics |
| `-profile` | adds the VMU Profiler, showing live FPS on the VMU LCD |
| `-test` | unpadded, emulator only, auto-starts a level. **Never boot on hardware** |

To start a new one:

```sh
tools/new-build.sh <slug> "what changed"
```

| Build | Name | Commit | What changed |
|-------|------|--------|--------------|
| b17 | flat-rim | 288bf67 | Stick saturates at 80% of travel so the outer fifth is flat; default sensitivity 50%, equal in absolute rate to the old build's 25% |

Builds before b17 were not tagged. Roughly, in order: the first hardware boot,
the 60Hz prompt fix, controller support, the analog aim scheme, the yaw rescale
to the engine clamp, and the VMU Profiler. Git history has the detail.
| b18 | tick-fix | 5b8fe35 | test_mouse now zeroes the delta after reading; catch-up ticks were each re-applying the full stick deflection, multiplying turn rate by how far behind the renderer was |
| b19 | maple-probe | 4ba82d2 | b18's turn-rate fix plus a startup dump of the maple bus in debug builds, to characterise the suspected rumble-pack boot failure |
| b20 | half-rate | e57d4ee | base yaw scale halved again (FIXED_ONE/4) after b18 was reported ~2x too fast at 30%; default sensitivity 30% |
| b21 | fine-aim | dc15958 | dither the turn delta against the engine's quantiser so slow turns are possible (centre was twitchy); disable INIT_PURUPURU as an experiment to identify the rumble-pack boot hang |
| b22 | rumble-shelved | 32498df | restore INIT_PURUPURU (exonerated by b21); rumble-pack boot hang traced to KOS's unbounded maple_wait_scan and shelved |
| b23 | flat-saves | f066cd0 | saved games and recordings live directly in /ram since the ramdisk has no subdirectories; guard the empty file list and hide preferences from save listings |
| b24 | sq-blit | ee4f34a | framebuffer blit uses sh4zam's store-queue memcpy; measure against b23 with the profile image |
| b25 | save-fix | 7213b93 | saving failed with error -22 because the ramdisk has no rename(); Exchange now copies and deletes. Triggers send TAB in menus so the D-pad can escape a focused list |
| b26 | vmu-saves | e529f9c | Saved games mirrored to the VMU so they survive a power cycle |
| b27 | vmu-saves-zlib | e529f9c | Deflate saved games so they fit on a VMU |
| b28 | vmu-card-full | 50c5e4c | Tell the player when a save will not fit on the card |
| b29 | vmu-delta-saves | 50c5e4c | Fold saves against the map level: 163 blocks to 23 |
| b30 | dialog-return | 717ac50 | Return selects a file in list dialogs, so saved games can be loaded |
| b31 | start-pause-menu | 9d6a0ad | Start opens a pause menu: resume, save, preferences, quit |
| b32 | menu-nav | 7caf9d9 | Main menu: no highlight trail, skips dead entries |
| b33 | gl-links | a1cfca4 | GL renderer compiles and links against GLdc |
