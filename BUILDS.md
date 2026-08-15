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
