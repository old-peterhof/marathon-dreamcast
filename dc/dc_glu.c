/*
 *	dc_glu.c -- the one GLU routine Aleph One needs and GLdc's glu.h lacks.
 *
 *	TextureManager::Shrink() calls gluScaleImage to bring an oversized texture
 *	down to whatever the hardware will accept. GLdc ships a glu.h, but not this.
 *
 *	Only the case Aleph One actually asks for is implemented -- GL_RGBA with
 *	GL_UNSIGNED_BYTE, both in and out -- and anything else is refused with
 *	GLU_INVALID_ENUM rather than quietly producing wrong pixels.
 *
 *	The filter is a box average over the source rectangle each destination pixel
 *	covers. Nearest-neighbour would be a line shorter and looks it: Marathon's
 *	wall textures are high-contrast and alias badly under point sampling when
 *	they are being reduced, which is the only direction this is ever used in.
 *	Averaging is also the right thing for the alpha channel, which decides what
 *	is see-through.
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <GL/gl.h>

#ifndef GLU_INVALID_ENUM
#define GLU_INVALID_ENUM	100900
#endif

int gluScaleImage(GLenum format,
                  GLsizei widthin, GLsizei heightin, GLenum typein,
                  const void *datain,
                  GLsizei widthout, GLsizei heightout, GLenum typeout,
                  void *dataout)
{
	const uint8_t *src = (const uint8_t *)datain;
	uint8_t *dst = (uint8_t *)dataout;
	GLsizei x, y;

	if (format != GL_RGBA || typein != GL_UNSIGNED_BYTE ||
	    typeout != GL_UNSIGNED_BYTE)
		return GLU_INVALID_ENUM;

	if (widthin <= 0 || heightin <= 0 || widthout <= 0 || heightout <= 0)
		return GLU_INVALID_ENUM;

	for (y = 0; y < heightout; y++) {
		/* Source rows this destination row covers. */
		GLsizei y0 = (GLsizei)(((int64_t)y * heightin) / heightout);
		GLsizei y1 = (GLsizei)(((int64_t)(y + 1) * heightin) / heightout);
		GLsizei sy;

		if (y1 <= y0)
			y1 = y0 + 1;
		if (y1 > heightin)
			y1 = heightin;

		for (x = 0; x < widthout; x++) {
			GLsizei x0 = (GLsizei)(((int64_t)x * widthin) / widthout);
			GLsizei x1 = (GLsizei)(((int64_t)(x + 1) * widthin) / widthout);
			unsigned r = 0, g = 0, b = 0, a = 0, n = 0;
			GLsizei sx;
			uint8_t *out;

			if (x1 <= x0)
				x1 = x0 + 1;
			if (x1 > widthin)
				x1 = widthin;

			for (sy = y0; sy < y1; sy++) {
				const uint8_t *row = src + ((size_t)sy * widthin) * 4;

				for (sx = x0; sx < x1; sx++) {
					const uint8_t *p = row + (size_t)sx * 4;

					r += p[0];
					g += p[1];
					b += p[2];
					a += p[3];
					n++;
				}
			}

			if (n == 0)
				n = 1;

			out = dst + (((size_t)y * widthout) + x) * 4;
			out[0] = (uint8_t)(r / n);
			out[1] = (uint8_t)(g / n);
			out[2] = (uint8_t)(b / n);
			out[3] = (uint8_t)(a / n);
		}
	}

	return 0;
}

/*
 *	gluBuild2DMipmaps -- also absent from GLdc's glu.h.
 *
 *	Uploads the base image, then halves it repeatedly with the box filter above,
 *	uploading each level until it reaches 1x1. Nothing clever: the source is
 *	always power-of-two here because Aleph One has already run Shrink() to make
 *	it so, and the box filter is exactly right for a 2:1 reduction -- each
 *	destination texel is the average of the four above it.
 */
int gluBuild2DMipmaps(GLenum target, GLint internalFormat,
                      GLsizei width, GLsizei height,
                      GLenum format, GLenum type, const void *data)
{
	const uint8_t *level_data = (const uint8_t *)data;
	uint8_t *owned = NULL;
	GLsizei w = width, h = height;
	GLint level = 0;

	if (format != GL_RGBA || type != GL_UNSIGNED_BYTE)
		return GLU_INVALID_ENUM;

	if (width <= 0 || height <= 0)
		return GLU_INVALID_ENUM;

	for (;;) {
		uint8_t *next;
		GLsizei nw, nh;

		glTexImage2D(target, level, internalFormat, w, h, 0,
		             format, type, level_data);

		if (w == 1 && h == 1)
			break;

		nw = (w > 1) ? w / 2 : 1;
		nh = (h > 1) ? h / 2 : 1;

		next = (uint8_t *)malloc((size_t)nw * nh * 4);
		if (!next)
			break;

		gluScaleImage(format, w, h, type, level_data,
		              nw, nh, type, next);

		/* The caller owns the base image; every level after it is ours. */
		if (owned)
			free(owned);
		owned = next;
		level_data = next;

		w = nw;
		h = nh;
		level++;
	}

	if (owned)
		free(owned);

	return 0;
}
