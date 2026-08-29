/*
 *	dc_fade.c -- the damage flash, and every other tinting fade.
 *
 *	Aleph One applies a fade one of two ways. At 8 bits it rewrites the palette,
 *	which tints everything drawn through it. At 16 it calls SDL_SetGammaRamp:
 *
 *	    succeeded = -1;
 *	    if ( video->SetGammaRamp ) succeeded = video->SetGammaRamp(this, ...);
 *	    else SDL_SetError("Gamma ramp manipulation not supported");
 *	    return succeeded;
 *
 *	SDL's Dreamcast video driver has no SetGammaRamp hook -- the drivers that do
 *	are x11, quartz, windib, dx5, macrom, dga and nanox -- so the call fails
 *	silently and the fade does nothing. This console runs at 16 bits. Being shot
 *	has never flashed the screen here, and neither has picking up a powerup or
 *	going under a liquid.
 *
 *	fades.cpp records the colour and opacity it computed; this blends it over the
 *	world. Only tinting fades are captured. The static effect and the negation
 *	effect do things one colour cannot express, and they are left doing nothing
 *	rather than doing something wrong.
 *
 *	COST, AND WHY THIS IS NOT DONE IN PLACE ANY MORE.
 *
 *	This used to run as a second pass over the video surface after the world had
 *	been blitted: a read-modify-write of every pixel of the view, in video RAM.
 *	On this SDL driver `main_surface->pixels` is `vram_l` -- the surface points
 *	straight at VRAM, there is no shadow buffer -- and a VRAM read on this
 *	machine is an uncached access over the bus. 640x320 of them, on top of the
 *	blit that had just written the same pixels, is what made being shot or
 *	picking something up read as a stall rather than a flash.
 *
 *	So the tint is applied *during* the copy instead. The read is from
 *	world_pixels, which is ordinary cached main memory, and each pixel is written
 *	to VRAM exactly once. Two full-view passes become one, and the expensive
 *	half of the old one disappears entirely.
 */

#include <stdint.h>

#include <SDL.h>

int dc_fade_tint(int *r, int *g, int *b, int *alpha);
void dc_trace(int slot, const char *fmt, ...);

/*
 *	Blend one colour over a rectangle of a 16-bit surface.
 *
 *	The channels are unpacked, mixed and repacked per pixel. Doing it in RGB565
 *	rather than promoting to 8-bit-per-channel keeps it to shifts and one
 *	multiply-add each, which matters on a 200MHz SH4 with 400KB of view to cover.
 */
/*
 *	Copy `src` to `dst` at `dstrect` with the live fade tint applied on the way.
 *
 *	Returns 0 when no fade is running, or when either surface is not 16-bit, so
 *	the caller can fall back to its ordinary fast blit. Returns 1 when it has
 *	drawn -- in which case the caller must not blit again.
 */
int dc_fade_blit_tinted(SDL_Surface *src, SDL_Surface *dst,
                        const SDL_Rect *dstrect)
{
	int r, g, b, a;
	int x, y, w, h, dx, dy;
	int tr, tg, tb;

	if (!src || !dst || !src->pixels || !dst->pixels)
		return 0;
	if (src->format->BytesPerPixel != 2 || dst->format->BytesPerPixel != 2)
		return 0;

	if (!dc_fade_tint(&r, &g, &b, &a))
		return 0;

	{
		static int told = 0;

		if (!told) {
			told = 1;
			dc_trace(28, "fade: first tint r=%d g=%d b=%d a=%d", r, g, b, a);
		}
	}

	/* 0..255 mapped to 0..256, so a full-strength tint is exact under >>8. */
	a += a >> 7;

	tr = r >> dst->format->Rloss;
	tg = g >> dst->format->Gloss;
	tb = b >> dst->format->Bloss;

	dx = dstrect ? dstrect->x : 0;
	dy = dstrect ? dstrect->y : 0;
	w  = src->w;
	h  = src->h;

	if (dx < 0) { w += dx; dx = 0; }
	if (dy < 0) { h += dy; dy = 0; }
	if (dx + w > dst->w) w = dst->w - dx;
	if (dy + h > dst->h) h = dst->h - dy;

	if (w <= 0 || h <= 0)
		return 1;	/* nothing to draw, but the caller must not blit either */

	for (y = 0; y < h; y++) {
		const uint16_t *sp = (const uint16_t *)
		        ((const uint8_t *)src->pixels + y * src->pitch);
		uint16_t *dp = (uint16_t *)
		        ((uint8_t *)dst->pixels + (dy + y) * dst->pitch) + dx;

		for (x = 0; x < w; x++) {
			uint16_t v = *sp++;
			int sr = (v & dst->format->Rmask) >> dst->format->Rshift;
			int sg = (v & dst->format->Gmask) >> dst->format->Gshift;
			int sb = (v & dst->format->Bmask) >> dst->format->Bshift;

			sr += ((tr - sr) * a) >> 8;
			sg += ((tg - sg) * a) >> 8;
			sb += ((tb - sb) * a) >> 8;

			*dp++ = (uint16_t)((sr << dst->format->Rshift) |
			                   (sg << dst->format->Gshift) |
			                   (sb << dst->format->Bshift));
		}
	}

	return 1;
}

/*
 *	The old in-place version. Kept because it is the only way to tint something
 *	that has no source surface to copy from, and it is correct -- it is simply
 *	the expensive way round when a copy is about to happen anyway. Nothing calls
 *	it at present.
 */
void dc_apply_fade_tint(SDL_Surface *dst, const SDL_Rect *area)
{
	int r, g, b, a;
	int x, y, x0, y0, x1, y1;
	int tr, tg, tb;

	if (!dst || dst->format->BytesPerPixel != 2)
		return;

	if (!dc_fade_tint(&r, &g, &b, &a))
		return;

	{
		/* Once per boot: proof the fade reached the screen at all, which is the
		   thing that was broken. */
		static int told = 0;

		if (!told) {
			told = 1;
			dc_trace(28, "fade: first tint r=%d g=%d b=%d a=%d", r, g, b, a);
		}
	}

	/* The tint in the destination's own channel widths. */
	tr = r >> dst->format->Rloss;
	tg = g >> dst->format->Gloss;
	tb = b >> dst->format->Bloss;

	x0 = area ? area->x : 0;
	y0 = area ? area->y : 0;
	x1 = area ? x0 + area->w : dst->w;
	y1 = area ? y0 + area->h : dst->h;

	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > dst->w) x1 = dst->w;
	if (y1 > dst->h) y1 = dst->h;

	if (x1 <= x0 || y1 <= y0)
		return;

	if (SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0)
		return;

	for (y = y0; y < y1; y++) {
		uint16_t *p = (uint16_t *)((uint8_t *)dst->pixels + y * dst->pitch) + x0;

		for (x = x0; x < x1; x++) {
			uint16_t v = *p;
			int sr = (v & dst->format->Rmask) >> dst->format->Rshift;
			int sg = (v & dst->format->Gmask) >> dst->format->Gshift;
			int sb = (v & dst->format->Bmask) >> dst->format->Bshift;

			sr += ((tr - sr) * a) >> 8;
			sg += ((tg - sg) * a) >> 8;
			sb += ((tb - sb) * a) >> 8;

			*p++ = (uint16_t)((sr << dst->format->Rshift) |
			                  (sg << dst->format->Gshift) |
			                  (sb << dst->format->Bshift));
		}
	}

	if (SDL_MUSTLOCK(dst))
		SDL_UnlockSurface(dst);
}
