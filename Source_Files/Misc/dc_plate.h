/*
 *	dc_plate.h -- the background plate every screen is drawn over.
 *
 *	See dc_plate.cpp. The one thing worth knowing at a call site: dc_plate_region
 *	takes both where to draw and where that lands on the 640x480 screen, because
 *	dialog_surface is dialog-local and the plate is not.
 */

#ifndef DC_PLATE_H
#define DC_PLATE_H

#ifdef DC

#include <SDL.h>

enum dc_plate_kind {
	DC_PLATE_PLAIN = 0,		/* gradient and seams -- every screen */
	DC_PLATE_MAIN  = 1		/* plus wordmark and watermark -- main menu only */
};

void dc_plate_select(int kind);
int  dc_plate_ready(void);
void dc_plate_to_screen(void);
/* Free the cached plates; they reload on the next menu. */
void dc_plate_release(void);
/* Allow plate loading again after a level is left. */
void dc_plate_resume(void);
bool dc_plate_region(SDL_Surface *dst, const SDL_Rect *dst_rect,
                     int src_x, int src_y);

extern "C" void dc_trace(int slot, const char *fmt, ...);

#endif	/* DC */

#endif	/* DC_PLATE_H */
