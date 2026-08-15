/*
 *	dc_blit.c -- the row copy that puts the rendered world on screen.
 *
 *	Split out of screen_sdl.cpp and compiled as C on purpose. sh4zam's headers
 *	need C++11 and use inline-asm string forms that gcc rejects under
 *	-std=gnu++98, which the rest of this port is pinned to because the game is
 *	2002 C++. As C they compile cleanly, so the store-queue path lives here and
 *	screen_sdl.cpp just calls in.
 *
 *	Why bother: the destination is video RAM, and the SH4's store queues are the
 *	hardware path for writing through the cache to external memory. This copy is
 *	640x320x2 = 400KB every frame, so it is not a rounding error at ~20fps on a
 *	200MHz part.
 *
 *	sh4zam is a library of hand-tuned SH4 routines originally written for the
 *	Grand Theft Auto 3 Dreamcast port. Vendored under dc/vendor with its licence.
 */

#include <string.h>
#include <stdint.h>

#include <sh4zam/shz_mem.h>

/*
 *	Copy `rows` rows of `bytes` each, advancing by the given pitches.
 *
 *	shz_sq_memcpy32 requires src 8-byte aligned, dst 4-byte aligned, and a length
 *	that is a multiple of 32. A 640-pixel row at 16bpp is 1280 bytes, which is 40
 *	whole chunks, so the ordinary case qualifies. Anything that does not falls
 *	back to memcpy, which is always correct -- an unaligned or odd-width blit
 *	should render, not crash.
 */
void dc_blit_rows(void *dst, const void *src, int bytes, int rows,
                  int dst_pitch, int src_pitch)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;
	int y;

	int use_sq = ((bytes & 31) == 0) &&
	             (((uintptr_t)d & 3) == 0) &&
	             (((uintptr_t)s & 7) == 0) &&
	             ((dst_pitch & 3) == 0) &&
	             ((src_pitch & 7) == 0);

	if (use_sq) {
		for (y = 0; y < rows; y++) {
			shz_sq_memcpy32(d, s, (size_t)bytes);
			d += dst_pitch;
			s += src_pitch;
		}
	} else {
		for (y = 0; y < rows; y++) {
			memcpy(d, s, (size_t)bytes);
			d += dst_pitch;
			s += src_pitch;
		}
	}
}
