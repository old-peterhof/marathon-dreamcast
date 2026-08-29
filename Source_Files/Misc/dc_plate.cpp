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

extern "C" void dc_blit_rows(void *dst, const void *src, int bytes,
                            int rows, int dst_pitch, int src_pitch);

#ifdef DC

#define PLATE_DIR	"/cd/AlephOne/UI/"

static SDL_Surface *plate[2];		/* indexed by dc_plate_kind */
static int plate_tried[2];
static int plate_kind = DC_PLATE_PLAIN;
static int plate_suspended = 0;

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

	/*
	 *	Refuse to load while a level is running. Freeing the plates on the way in
	 *	is not enough on its own: something repaints through the plate during the
	 *	transition and pulled a fresh 1.2MB copy straight back in, which is
	 *	exactly the memory the GL renderer had just been given.
	 *
	 *	Callers all cope with a NULL plate already -- it is the same path a
	 *	missing file takes, and they fall back to the flat fill nobody sees
	 *	because the world is drawn over it.
	 */
	if (plate_suspended)
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

/*
 *	Drop the cached plates.
 *
 *	They are menu artwork and nothing draws them while a level is running, but
 *	they are the single largest thing this port keeps alive: SDL_DisplayFormat
 *	converts to the *display* format, so each plate is 614KB at 16 bits and
 *	1.2MB under SDL_OPENGLBLIT, whose shadow surface is 32-bit because GLdc's
 *	gl.h does not define GL_VERSION_1_2 and SDL's 16-bit branch is therefore
 *	compiled out. Two plates, so up to 2.4MB held for a screen nobody is
 *	looking at.
 *
 *	plate_tried is cleared as well, so returning to the menu reloads them from
 *	the disc rather than falling back to the flat fill.
 */
void dc_plate_release(void)
{
	int k;

	plate_suspended = 1;

	for (k = 0; k < 2; k++) {
		if (plate[k]) {
			SDL_FreeSurface(plate[k]);
			plate[k] = NULL;
		}
		plate_tried[k] = 0;
	}
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
		SDL_UpdateRect(video, 0, 0, 0, 0);
		return;
	}

	/*
	 *	A row copy, not SDL_BlitSurface.
	 *
	 *	The video surface here is SDL_HWSURFACE and its pixels are video RAM at
	 *	0xa5000000. SDL's blit into it does not land -- verified by reading a
	 *	pixel straight back afterwards and finding black where the plate has a
	 *	bright letterform. It fails silently, which is why the menu drew over the
	 *	stock artwork instead of over the plate, and why the design looked
	 *	ignored.
	 *
	 *	This is the same path screen_sdl.cpp already uses for the rendered world,
	 *	for the same reason. Both surfaces are display format and 640 wide, so a
	 *	row is 1280 bytes -- forty whole store-queue chunks.
	 */
	{
		int rows = (p->h < video->h) ? p->h : video->h;
		int bytes = ((p->w < video->w) ? p->w : video->w) *
		            video->format->BytesPerPixel;

		if (SDL_MUSTLOCK(video) && SDL_LockSurface(video) < 0)
			return;

		dc_blit_rows(video->pixels, p->pixels, bytes, rows,
		             video->pitch, p->pitch);

		if (SDL_MUSTLOCK(video))
			SDL_UnlockSurface(video);
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

/*
 *	Allow the plates back. Called when a level is left, so the menus get their
 *	artwork again; the next dc_plate_to_screen reloads from the disc.
 */
void dc_plate_resume(void)
{
	plate_suspended = 0;
}
