#ifndef ALEPHONE_DC_VQ_SPRITES_H
#define ALEPHONE_DC_VQ_SPRITES_H

#ifdef DC

/*
 * Upload one pre-encoded sprite texture into the currently bound GL texture.
 * Returns false when the pack has no matching entry or cannot be read, letting
 * OGL_Textures.cpp fall back to its ordinary uncompressed upload.
 */
bool dc_vq_sprite_upload(unsigned collection, unsigned clut, unsigned bitmap,
                         bool glowing, unsigned width, unsigned height);

/* Test the index without reading or uploading the payload. */
bool dc_vq_sprite_available(unsigned collection, unsigned clut, unsigned bitmap,
                            bool glowing, unsigned width, unsigned height);

#endif

#endif
