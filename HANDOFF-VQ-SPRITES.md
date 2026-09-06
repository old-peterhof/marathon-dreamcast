# VQ sprites + full-resolution texture residency handoff

Date: 2026-09-03

## User decision

Max wants the Dreamcast GL build to stop using half-resolution, linearly
filtered sprites as its permanent compromise.  The selected direction is the
combination discussed with Codex:

1. Keep the HUD full resolution.
2. Render inhabitants and weapons-in-hand at full resolution with nearest
   filtering, closer to the software renderer's crisp appearance.
3. Pre-encode stock sprite frames with the Dreamcast's native VQ format when
   VQ is actually smaller than the 16-bit texture.
4. Add a bounded VRAM LRU so first-seen animation frames do not remain resident
   until the end of the level.
5. Leave walls at half resolution for the first controlled test; revisit them
   only after the sprite residency numbers are known.

This is a VRAM change.  PNG/JPEG/Zstd compression of the source files would
only affect disc/main-RAM size because PowerVR cannot sample those encodings.
GLdc 1.1.1 does support precompressed ARGB4444 VQ through
`glCompressedTexImage2DARB`.

## Existing measurements driving the change

- Full-resolution sprites and weapon frames previously retained 28--30 FPS.
- At 100 uploads they cost about 1630 KiB more VRAM than half resolution.
- That build eventually died because `TextureState` never evicts anything.
- Font deferral has since reduced the first-world-upload VRAM baseline from
  about 1120 KiB to about 451 KiB, so the old experiment should be repeated,
  but it still needs an eviction policy for worst-case levels.
- The GLdc texture pool is about 4958 KiB; it is not the full 8 MiB because PVR
  bins and vertex buffers occupy the rest.

## Claude work preserved

At the start of this work Claude had edits in:

- `Source_Files/Misc/HUDRenderer_OGL.cpp`
- `Source_Files/Misc/OGL_Textures.cpp`
- `Source_Files/Misc/marathon2.cpp`
- `Source_Files/Misc/shapes.cpp`
- `dc/dc_compat.c`

Those changes include the HUD-only `DC_ForceFullResolution` override, compact
two-level GL shading tables, and heap tracing. Claude committed them as
`33aed2b` while Codex was working. The VQ changes below are still uncommitted;
no existing Claude changes were reverted or overwritten.

## Codex implementation

`tools/build-vq-sprites.py` is new.  It parses the 8-bit indexed art directly
from the Marathon `Shapes` file, expands object/scenery bitmaps through each
native CLUT, applies the same power-of-two sprite padding as
`OGL_Textures.cpp`, and invokes KOS's host-side `pvrtex` encoder.

The packer deliberately:

- uses full-codebook, twiddled ARGB4444 VQ that stock GLdc can upload;
- omits MML-overridden textures so opacity/substitute-image behavior keeps
  using the existing runtime conversion;
- includes only VQ blobs whose 256-byte-rounded allocation beats raw 16-bit;
- emits one sector-aligned `VQSprites.dat` pack rather than hundreds of ISO
  files.

The first parser test exposed and fixed an important Shapes detail: most
collections have no separate 16-bit payload, so the loader must fall back from
`offset16` to the primary `offset`, exactly as `load_collection()` does.

The completed pack contains **4,048 normal/glow texture variants**. Its useful
VQ payload is **25,460 KiB**, versus **138,912 KiB** for the same padded frames
as raw 16-bit textures. The file is about 26 MiB after its index and per-entry
CD-sector padding. Another 334 candidates were omitted because VQ would not
beat raw storage after allocator rounding.

Runtime code is in `dc/dc_vq_sprites.{h,cpp}`. It keeps only the sorted 126 KiB
index in main RAM, binary-searches `(collection, clut, bitmap, normal/glow)`,
then reads one compressed payload on first use. Payload starts and read lengths
are both 2,048-byte aligned, applying the same GD-ROM lesson as commit
`11c39f3`; the padding is not uploaded to VRAM. A missing/stale/failed entry
falls back to the existing raw upload.

`OGL_Textures.cpp` checks the VQ index before constructing the RGBA staging
image. When both required normal/glow entries exist it skips that full-size
temporary allocation entirely. If the later read or GL upload fails, it builds
the old buffer lazily and takes the old path. Thus the feature saves transient
main RAM as well as persistent VRAM.

Dreamcast defaults keep walls and landscapes at half resolution, while
inhabitants and weapons-in-hand use full resolution plus nearest filtering.
Interface/HUD art remains full resolution through Claude's existing override.
Non-crisp opacity, infravision/silhouette, MML overrides, substitute images,
walls, landscapes, and interface textures deliberately bypass the pack.

`Makefile.dc` generates the pack only for `GL=1`, adds the runtime object, and
stages `VQSprites.dat` at `/cd/AlephOne/VQSprites.dat`. The generated file and
disc staging tree remain ignored build artifacts; only the packer is source.

## Safety constraints for the LRU

Do not call `glDeleteTextures` while GLdc's queued polygons can still reference
the texture.  GLdc records texture addresses in its vertex lists, and deletion
immediately returns the allocation to the VRAM pool.  Eviction will therefore
happen at the beginning of a world frame and call `pvr_wait_ready()` only when
the low-water mark requires deletion.  That occasional synchronization is
preferable to corrupting an in-flight PowerVR scene.

The declaration matches KOS's real `int pvr_wait_ready(void)` signature. If its
100-tick wait times out, eviction is deferred and no VRAM is freed. Slot 53
traces before the wait and after the eviction, so a serial log can establish
whether this path ran before a failure.

The intended first thresholds are 1024 KiB low water and 1536 KiB high water.
Interface collection textures are pinned.  A texture used during the previous
frame is not eligible.  Thresholds are provisional until the 175+ upload soak.

## Verification and the reported Flycast stop

- `./build.sh -j8 GL=1 FAST=1` compiles and links successfully. Warnings shown
  are pre-existing warnings elsewhere in the port.
- `./build.sh -j8 GL=1 FAST=1 play` rebuilt the final pack, staged it, and
  produced `alephone-b76-gl-colours-play.cdi` successfully after the
  no-RGBA-buffer and sector-end-alignment refinements.
- The pack header/index validator passes: version and entry size match the
  runtime structs; all 4,048 keys are sorted and unique; all offsets are sector
  aligned; all extents are in bounds; every format tag is ARGB4444 VQ twiddled.
- Max ran the first VQ build interactively for roughly 90 seconds before
  Flycast stopped with `Fatal: SH4 exception when blocked`.

Do **not** infer “the VQ decoder crashed” from that Flycast string alone. The
same generic message occurs repeatedly in the existing Flycast log for older
images, including several runs near 1:29, and it is also a known Flycast error
for unrelated commercial/homebrew images. We still need the KOS serial tail to
tell whether this run hit a game exception, GLdc OOM, the LRU, or only an
emulator failure. Relevant slot-53 lines are:

    vq: 4048 entries, 126 KB index
    vq: 1 compressed sprite uploads
    vq: 25 compressed sprite uploads
    vram lru: low-water at ...; waiting for PVR
    vram lru: evicted ..., ... -> ... KB free

## Files changed by Codex

- `Makefile.dc`
- `Source_Files/Misc/OGL_Render.cpp`
- `Source_Files/Misc/OGL_Setup.cpp`
- `Source_Files/Misc/OGL_Textures.cpp`
- `Source_Files/Misc/OGL_Textures.h`
- `Source_Files/Misc/shell_sdl.cpp`
- `dc/dc_vq_sprites.cpp`
- `dc/dc_vq_sprites.h`
- `tools/build-vq-sprites.py`
- this handoff

## Next test

1. Launch `alephone-b76-gl-colours-play.cdi` through
   `tools/run-flycast.sh <image> <serial-log> 20` so KOS
   output survives a Flycast stop.
2. Confirm the slot-53 index and first-upload lines, sprite orientation,
   transparency, crisp filtering, and weapon sprites.
3. Soak past the old 175-upload / 90-second failure point and preserve the last
   100 serial lines if it stops. The presence or absence of the `vram lru:`
   pre-wait line is the first branch in the diagnosis.

Codex left the final image running in Flycast with serial output at
`/tmp/alephone-vq-final.log` for Max's hands-on soak.
