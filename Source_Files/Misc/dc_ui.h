/*
 *	dc_ui.h -- the drawing vocabulary from mockups/prototype/app.css.
 *	See dc_ui.cpp.
 */

#ifndef DC_UI_H
#define DC_UI_H

#ifdef DC

#include <SDL.h>

/* Geometry, from app.css. 640x480 with 40px clear on every edge. */
#define DC_UI_EDGE		40
#define DC_UI_SAFE_R	600
#define DC_UI_ROW_H		34		/* --row   */
#define DC_UI_CAP_H		20		/* .cap    */
#define DC_UI_HINT_H	18		/* .hint   */

/*
 *	Letter-spacing, in whole pixels. The design tracks rows at .16em, captions at
 *	.20em and labels at .10em; at the sizes in use those round to these.
 */
#define DC_UI_TRACK_ITEM	2
#define DC_UI_TRACK_CAP		2
#define DC_UI_TRACK_LABEL	1

enum {
	DC_UI_PANEL,
	DC_UI_RULE,
	DC_UI_RULE_HOT,
	DC_UI_HOT,
	DC_UI_HOT_BAR,
	DC_UI_ITEM,
	DC_UI_LABEL,
	DC_UI_OFF
};

struct dc_ui_hint {
	const char *glyph;
	const char *label;
	bool round;
};

uint32 dc_ui_colour(int which, SDL_Surface *s);

void dc_ui_fill(SDL_Surface *s, int x, int y, int w, int h, uint32 colour);
void dc_ui_blend(SDL_Surface *s, int x, int y, int w, int h,
                 const SDL_Color &c, int alpha);

void dc_ui_rule(SDL_Surface *s, int x, int y, int w, bool hot);
void dc_ui_panel(SDL_Surface *s, int x, int y, int w, int h);
void dc_ui_caret(SDL_Surface *s, int x, int cy, uint32 colour, int size);

int  dc_ui_tracked_width(const char *text, const sdl_font_info *font,
                         uint16 style, int tracking);
void dc_ui_tracked_text(SDL_Surface *s, const char *text, int x, int y,
                        uint32 colour, const sdl_font_info *font,
                        uint16 style, int tracking);

void dc_ui_row(SDL_Surface *s, int x, int y, int w, int h,
               const char *label, const char *value,
               bool selected, bool disabled,
               const sdl_font_info *item_font, uint16 item_style,
               const sdl_font_info *label_font, uint16 label_style);

void dc_ui_caption(SDL_Surface *s, int x, int y, int w,
                   const char *text, const char *right, bool right_hot,
                   const sdl_font_info *font, uint16 style);

void dc_ui_hints(SDL_Surface *s, int y,
                 const dc_ui_hint *hints, int count, const char *right,
                 const sdl_font_info *font, uint16 style);

#endif	/* DC */

#endif	/* DC_UI_H */
