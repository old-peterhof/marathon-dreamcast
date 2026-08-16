/*
 *	dc_plate.cpp -- the background plate every screen is drawn over.
 *
 *	UI-HANDOFF.md section 5.1 replaces the main menu's painted-in buttons with a
 *	static plate and text drawn on top. That is what retires the eighteen
 *	hardcoded hit rectangles and the pair of full-screen PICTs, and it is why the
 *	menu's item list can change from now on without anyone opening a paint
 *	program.
 *
 *	Two plates, baked by tools/bake-plate.py out of the prototype's own CSS:
 *
 *		plate.bmp		gradient and seam grid -- every screen
 *		plate-main.bmp	the same plus the wordmark and the watermark
 *
 *	BMP rather than PNG because SDL_LoadBMP is core SDL -- already used for the
 *	dialog theme at sdl_dialogs.cpp:545 -- whereas PNG would mean linking libpng,
 *	which this port does not (Makefile.dc: SDL_LIBS = -lSDL -lGL -lz).
 *
 *	COORDINATES, which are the whole difficulty here. dialog_surface is
 *	dialog-local: dialog::update() blits local (x,y) to screen (x + rect.x,
 *	y + rect.y), and only ever touches the dialog's own rectangle. So a dialog
 *	drawing "its" piece of a full-screen plate has to read from the plate at the
 *	dialog's screen position, which is what the src_x/src_y arguments below are
 *	for. Getting this wrong is not subtle -- every dialog shows the top-left
 *	corner of the plate instead of the part behind it.
 */

#include "cseries.h"

#include <SDL.h>
#include <stdio.h>

#include "dc_plate.h"

#ifdef DC

#define PLATE_DIR	"/cd/AlephOne/UI/"

static SDL_Surface *plate[2];		/* indexed by dc_plate_kind */
static int plate_tried[2];
static int plate_kind = DC_PLATE_PLAIN;

/*
 *	Load once and keep. The file is 24-bit and the display is 16, so it is
 *	converted on the way in and the 24-bit original freed -- otherwise every blit
 *	pays for a format conversion, and the plate is blitted on every widget
 *	redraw.
 *
 *	A missing or unreadable plate is not fatal. Everything falls back to the flat
 *	BACKGROUND_COLOR the dialogs used before, which is ugly but playable, and the
 *	trace says which file was wanted.
 */
static SDL_Surface *load(int kind)
{
	static const char *names[2] = { "plate.bmp", "plate-main.bmp" };
	char path[128];
	SDL_Surface *raw, *conv;

	if (kind < 0 || kind > 1)
		return NULL;

	if (plate[kind] || plate_tried[kind])
		return plate[kind];

	plate_tried[kind] = 1;

	sprintf(path, "%s%s", PLATE_DIR, names[kind]);

	raw = SDL_LoadBMP(path);
	if (!raw) {
		dc_trace(25, "plate: %s missing -- falling back to flat fill", path);
		return NULL;
	}

	conv = SDL_DisplayFormat(raw);
	SDL_FreeSurface(raw);

	if (!conv) {
		dc_trace(25, "plate: %s would not convert", path);
		return NULL;
	}

	plate[kind] = conv;
	dc_trace(25, "plate: %s loaded %dx%d", names[kind], conv->w, conv->h);

	return conv;
}

void dc_plate_select(int kind)
{
	plate_kind = kind;
}

int dc_plate_ready(void)
{
	return load(plate_kind) != NULL;
}

/*
 *	Paint the whole plate to the video surface.
 *
 *	Dialogs only ever repaint their own rectangle, so something has to cover the
 *	rest of the screen first. This is what clear_screen() used to do with a flat
 *	fill, and every caller of clear_screen() before a dialog wants this instead.
 */
void dc_plate_to_screen(void)
{
	SDL_Surface *video = SDL_GetVideoSurface();
	SDL_Surface *p = load(plate_kind);

	if (!video)
		return;

	if (!p) {
		SDL_FillRect(video, NULL, SDL_MapRGB(video->format, 0x05, 0x08, 0x0a));
	} else {
		SDL_Rect dst = { 0, 0, (Uint16)p->w, (Uint16)p->h };
		SDL_BlitSurface(p, NULL, video, &dst);
	}

	SDL_UpdateRect(video, 0, 0, 0, 0);
}

/*
 *	Restore one rectangle of the plate into a destination surface.
 *
 *	`dst_rect` is where to draw, in the destination's own coordinates.
 *	`src_x`/`src_y` are where that rectangle sits on the 640x480 screen, which is
 *	where the corresponding plate pixels live. For a dialog those differ by
 *	exactly the dialog's position, which is the offset dialog::update() applies.
 *
 *	Returns false if there is no plate, so the caller can do its old flat fill.
 */
bool dc_plate_region(SDL_Surface *dst, const SDL_Rect *dst_rect,
                     int src_x, int src_y)
{
	SDL_Surface *p = load(plate_kind);
	SDL_Rect src, d;

	if (!p || !dst)
		return false;

	if (dst_rect) {
		d = *dst_rect;
	} else {
		d.x = 0;
		d.y = 0;
		d.w = (Uint16)dst->w;
		d.h = (Uint16)dst->h;
	}

	src.x = (Sint16)(src_x + d.x);
	src.y = (Sint16)(src_y + d.y);
	src.w = d.w;
	src.h = d.h;

	/* Off the edge of the plate -- let the caller fill instead of stretching or
	   wrapping, either of which would look like a bug rather than a fallback. */
	if (src.x < 0 || src.y < 0 || src.x + src.w > p->w || src.y + src.h > p->h)
		return false;

	SDL_BlitSurface(p, &src, dst, &d);

	return true;
}

#endif	/* DC */
