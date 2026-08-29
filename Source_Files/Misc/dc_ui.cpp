/*
 *	dc_ui.cpp -- the drawing vocabulary the prototype's screens are built from.
 *
 *	mockups/prototype is the design, and it is a working prototype rather than a
 *	picture: app.css defines a panel, a caption bar, a row, a caret, a rule and a
 *	hint bar, and every screen is those pieces arranged. This is those pieces.
 *
 *	Written because the first cut of the main menu was built from UI-HANDOFF.md's
 *	prose -- the tokens, the row height, the three-way selection cue -- without
 *	opening the prototype's markup, and so got the plate exactly right and the
 *	foreground entirely wrong. The prose describes the design; the CSS *is* the
 *	design.
 *
 *	Three things here are not obvious:
 *
 *	RULES ARE NOT ONE PIXEL. UI-HANDOFF section 1 measured this: the output is
 *	480i, so a one-pixel horizontal line lives on one field only and buzzes at
 *	30Hz. Every rule is a 2px core with 1px flanks at 35%, which lands on both
 *	fields. The flanks are a real blend against whatever is underneath, because
 *	the plate is a gradient and a precomputed colour would band against it.
 *
 *	LETTER-SPACING HAS TO BE DRAWN. The design tracks every row at .16em, which
 *	at these sizes is around 2px a character and is a large part of why the
 *	prototype reads as Marathon rather than as a dialog box. SDL's draw_text has
 *	no notion of it, so tracked text is drawn a character at a time.
 *
 *	THE CARET IS A TRIANGLE, not a ">" -- border-left:9px solid with transparent
 *	top and bottom, which is the CSS idiom for one. Drawn as spans.
 */

#include "cseries.h"

#include "sdl_dialogs.h"
#include "sdl_fonts.h"
#include "sdl_widgets.h"
#include "shape_descriptors.h"
#include "screen_drawing.h"
#include "shell.h"
#include "world.h"

#include <string.h>

#ifdef DC

#include "dc_ui.h"

/*
 *	Chrome colours from app.css that have no slot in the dialog theme. The theme
 *	enum covers text; these are the surfaces the text sits on.
 */
static const SDL_Color col_panel   = { 0x10, 0x1a, 0x1b, 0 };	/* --panel    */
static const SDL_Color col_rule    = { 0x2d, 0x46, 0x40, 0 };	/* --rule     */
static const SDL_Color col_rulehot = { 0x4d, 0x6f, 0x63, 0 };	/* --rule-hot */
static const SDL_Color col_hot     = { 0xff, 0xc0, 0x00, 0 };	/* --hot      */
static const SDL_Color col_hotbar  = { 0x2a, 0x1e, 0x04, 0 };	/* --hot-bar  */
static const SDL_Color col_item    = { 0x6f, 0x8a, 0x7e, 0 };	/* --item     */
static const SDL_Color col_label   = { 0x4e, 0x64, 0x59, 0 };	/* --label    */
static const SDL_Color col_off     = { 0x3d, 0x4c, 0x46, 0 };	/* .row.off   */
static const SDL_Color col_face    = { 0xe2, 0xef, 0xe8, 0 };	/* --face     */

/*
 *	These are design tokens, not dialog theme colours, and they deliberately do
 *	NOT go through get_dialog_color().
 *
 *	disc-AlephOne/Themes/Default/theme.mml is an imitation of Marathon Infinity's
 *	menus and sets its own item and label colours -- red and bright green. It is
 *	loaded from the disc at start-up and overwrites the defaults compiled into
 *	sdl_dialogs.cpp, so every screen drawn from those constants came out in the
 *	wrong palette no matter what the defaults said. Measured on screen: rows at
 *	(115,172,83) where the design asks for (111,138,126), labels at (140,223,177)
 *	where it asks for (78,100,89).
 *
 *	The theme still governs the stock dialogs. It has no business governing this.
 */

static uint32 map(SDL_Surface *s, const SDL_Color &c)
{
	return SDL_MapRGB(s->format, c.r, c.g, c.b);
}

uint32 dc_ui_colour(int which, SDL_Surface *s)
{
	switch (which) {
	case DC_UI_PANEL:    return map(s, col_panel);
	case DC_UI_RULE:     return map(s, col_rule);
	case DC_UI_RULE_HOT: return map(s, col_rulehot);
	case DC_UI_HOT:      return map(s, col_hot);
	case DC_UI_HOT_BAR:  return map(s, col_hotbar);
	case DC_UI_ITEM:     return map(s, col_item);
	case DC_UI_LABEL:    return map(s, col_label);
	case DC_UI_OFF:      return map(s, col_off);
	case DC_UI_FACE:     return map(s, col_face);
	}

	return map(s, col_rule);
}

void dc_ui_fill(SDL_Surface *s, int x, int y, int w, int h, uint32 colour)
{
	SDL_Rect r;

	if (w <= 0 || h <= 0)
		return;

	r.x = (Sint16)x;
	r.y = (Sint16)y;
	r.w = (Uint16)w;
	r.h = (Uint16)h;

	SDL_FillRect(s, &r, colour);
}

/*
 *	Blend a colour over what is already there, at `alpha` in 0..255.
 *
 *	Only used for the 1px rule flanks, so it is a straightforward per-pixel
 *	read-modify-write on a 16-bit surface rather than anything clever. A rule is
 *	560 pixels wide and there are two flanks, so this is about 1100 pixels.
 */
void dc_ui_blend(SDL_Surface *s, int x, int y, int w, int h,
                 const SDL_Color &c, int alpha)
{
	int px, py;

	/*
	 *	This reads and writes Uint16 directly. dc_copy_to_screen guards the same
	 *	assumption and falls back to SDL when it does not hold; this had no such
	 *	guard, and at 8bpp `px` steps twice the pixel size and runs off the end
	 *	of the row. The Dreamcast SDL driver rejects 8-bit outright, so this has
	 *	never been reachable -- it is here so it cannot become reachable quietly.
	 */
	if (!s || s->format->BytesPerPixel != 2)
		return;

	if (SDL_MUSTLOCK(s) && SDL_LockSurface(s) < 0)
		return;

	for (py = y; py < y + h; py++) {
		if (py < 0 || py >= s->h)
			continue;

		for (px = x; px < x + w; px++) {
			Uint8 dr, dg, db;
			Uint16 *p;

			if (px < 0 || px >= s->w)
				continue;

			p = (Uint16 *)((Uint8 *)s->pixels + py * s->pitch) + px;

			SDL_GetRGB(*p, s->format, &dr, &dg, &db);

			dr = (Uint8)((c.r * alpha + dr * (255 - alpha)) / 255);
			dg = (Uint8)((c.g * alpha + dg * (255 - alpha)) / 255);
			db = (Uint8)((c.b * alpha + db * (255 - alpha)) / 255);

			*p = (Uint16)SDL_MapRGB(s->format, dr, dg, db);
		}
	}

	if (SDL_MUSTLOCK(s))
		SDL_UnlockSurface(s);
}

/*
 *	A rule: 2px core, 1px flanks at 35%.
 *
 *	`y` is the top of the core, matching how the prototype positions .hr.
 */
void dc_ui_rule(SDL_Surface *s, int x, int y, int w, bool hot)
{
	const SDL_Color &c = hot ? col_rulehot : col_rule;

	dc_ui_blend(s, x, y - 1, w, 1, c, 89);		/* .35 of 255 */
	dc_ui_fill(s, x, y, w, 2, map(s, c));
	dc_ui_blend(s, x, y + 2, w, 1, c, 89);
}

/*
 *	A panel: --panel fill, 1px left and right borders, 2px top and bottom.
 *
 *	The asymmetry is the prototype's and is deliberate -- it reads as a horizontal
 *	band rather than a box, and the 2px horizontals are the interlace rule again.
 */
void dc_ui_panel(SDL_Surface *s, int x, int y, int w, int h)
{
	uint32 fill = map(s, col_panel);
	uint32 rule = map(s, col_rule);

	dc_ui_fill(s, x, y, w, h, fill);

	dc_ui_fill(s, x, y, w, 2, rule);				/* top    */
	dc_ui_fill(s, x, y + h - 2, w, 2, rule);		/* bottom */
	dc_ui_fill(s, x, y, 1, h, rule);				/* left   */
	dc_ui_fill(s, x + w - 1, y, 1, h, rule);		/* right  */
}

/*
 *	The caret: a right-pointing triangle 9 wide and 12 tall, vertically centred
 *	on `cy`. CSS builds it out of borders; here it is six spans.
 */
void dc_ui_caret(SDL_Surface *s, int x, int cy, uint32 colour, int size)
{
	int half = size / 2;
	int row;

	for (row = 0; row < size; row++) {
		int dy = row - half;
		int len = size - (dy < 0 ? -dy : dy) * 2;

		if (len > 0)
			dc_ui_fill(s, x, cy - half + row, (len * 3) / 4, 1, colour);
	}
}

/*
 *	Tracked text.
 *
 *	Drawn a character at a time so the design's letter-spacing survives. The
 *	tracking is in whole pixels because everything here is, and .16em at the
 *	sizes in use rounds to 2.
 */
/*
 *	The width tracked text will occupy.
 *
 *	Measured on the whole string, not by summing its characters. A glyph's
 *	advance is wider than its ink, so per-character summing underestimates -- by
 *	enough that a binding label which measured as fitting still ran into the
 *	button name beside it.
 */
int dc_ui_tracked_width(const char *text, const sdl_font_info *font,
                        uint16 style, int tracking)
{
	int n = 0;
	const char *p;

	if (!text || !font)
		return 0;

	for (p = text; *p; p++)
		n++;

	if (n < 1)
		return 0;

	return text_width(text, font, style) + tracking * (n - 1);
}

/*
 *	Tracked text that physically stops at `max_x`.
 *
 *	Measuring a string by summing its characters' widths turned out to
 *	underestimate what actually gets drawn, so a label that measured as fitting
 *	still ran into the value beside it. Rather than chase the discrepancy, the
 *	draw itself refuses to cross the line: the glyph that would overrun is not
 *	drawn, and neither is anything after it.
 */
void dc_ui_tracked_text_clipped(SDL_Surface *s, const char *text, int x, int y,
                                uint32 colour, const sdl_font_info *font,
                                uint16 style, int tracking, int max_x)
{
	const char *p;

	if (!text || !font)
		return;

	for (p = text; *p; p++) {
		char ch[2];
		int cw;

		ch[0] = *p;
		ch[1] = 0;

		cw = text_width(ch, font, style);

		if (max_x > 0 && x + cw > max_x)
			return;

		draw_text(s, ch, x, y, colour, font, style);
		x += cw + tracking;
	}
}

void dc_ui_tracked_text(SDL_Surface *s, const char *text, int x, int y,
                        uint32 colour, const sdl_font_info *font,
                        uint16 style, int tracking)
{
	const char *p;

	if (!text || !font)
		return;

	if (tracking <= 0) {
		draw_text(s, text, x, y, colour, font, style);
		return;
	}

	for (p = text; *p; p++) {
		char ch[2];

		ch[0] = *p;
		ch[1] = 0;

		draw_text(s, ch, x, y, colour, font, style);
		x += text_width(ch, font, style) + tracking;
	}
}

/*
 *	One row of a panel list.
 *
 *	Selection is signalled three ways at once, which is the prototype's own
 *	decision and the reason it survives a composite television: the amber text,
 *	the dim amber bar, and the caret. The 3px amber edge on the left is a fourth,
 *	and is what makes the selected row readable even where the bar itself is too
 *	dark to see.
 */
void dc_ui_row(SDL_Surface *s, int x, int y, int w, int h,
               const char *label, const char *value,
               bool selected, bool disabled,
               const sdl_font_info *item_font, uint16 item_style,
               const sdl_font_info *label_font, uint16 label_style)
{
	uint32 text_colour;
	int baseline = y + (h - item_font->get_line_height()) / 2 + item_font->get_ascent();

	if (selected) {
		dc_ui_fill(s, x, y, w, h, map(s, col_hotbar));
		dc_ui_fill(s, x, y, 3, h, map(s, col_hot));			/* inset edge */
		dc_ui_caret(s, x + 13, y + h / 2, map(s, col_hot), 12);
	}

	if (disabled)
		text_colour = map(s, col_off);
	else if (selected)
		text_colour = map(s, col_hot);
	else
		text_colour = map(s, col_item);

	/*
	 *	Fit the label to whatever the value has left.
	 *
	 *	A binding row is 270px wide and holds an action and a button name, and
	 *	the worst pair -- "2ND TRIGGER" against "L TRIGGER" -- ran into each
	 *	other at full tracking. Rather than tune each screen, the row gives up
	 *	tracking first and then font size, in that order, because losing the
	 *	letter-spacing is much less visible than two words colliding.
	 */
	{
		int label_x = x + 34;
		int limit = x + w - 12;
		int track = DC_UI_TRACK_ITEM;
		const sdl_font_info *lfont = item_font;
		uint16 lstyle = item_style;

		if (value && value[0])
			/* 20px of clear air, not 10: at 10 the widest pair on the binding
			   screen sat flush against each other and read as one word. */
			limit -= dc_ui_tracked_width(value, label_font, label_style,
			                             DC_UI_TRACK_LABEL) + 20;

		if (label_x + dc_ui_tracked_width(label, lfont, lstyle, track) > limit)
			track = 0;

		if (label_x + dc_ui_tracked_width(label, lfont, lstyle, track) > limit) {
			lfont = label_font;
			lstyle = label_style;
			baseline = y + (h - lfont->get_line_height()) / 2 + lfont->get_ascent();
		}

		dc_ui_tracked_text_clipped(s, label, label_x, baseline, text_colour,
		                           lfont, lstyle, track, limit);
	}

	/* .row .val -- right-aligned, label font, and amber when the row is. */
	if (value && value[0]) {
		int vw = dc_ui_tracked_width(value, label_font, label_style,
		                             DC_UI_TRACK_LABEL);
		int vy = y + (h - label_font->get_line_height()) / 2 +
		         label_font->get_ascent();

		dc_ui_tracked_text(s, value, x + w - 12 - vw, vy,
		                   selected ? map(s, col_hot) : map(s, col_label),
		                   label_font, label_style, DC_UI_TRACK_LABEL);
	}
}

/*
 *	The caption bar across the top of a panel: 20px, tracked wide, with an
 *	optional right-aligned extra (the binding screen's buttons-spent counter).
 */
void dc_ui_caption(SDL_Surface *s, int x, int y, int w,
                   const char *text, const char *right, bool right_hot,
                   const sdl_font_info *font, uint16 style)
{
	int baseline = y + (DC_UI_CAP_H - font->get_line_height()) / 2 +
	               font->get_ascent();

	dc_ui_tracked_text(s, text, x + 10, baseline, map(s, col_label), font, style,
	                   DC_UI_TRACK_CAP);

	if (right && right[0]) {
		int rw = dc_ui_tracked_width(right, font, style, DC_UI_TRACK_CAP);

		dc_ui_tracked_text(s, right, x + w - 10 - rw, baseline,
		                   right_hot ? map(s, col_hot) : map(s, col_item),
		                   font, style, DC_UI_TRACK_CAP);
	}

	dc_ui_fill(s, x, y + DC_UI_CAP_H, w, 2, map(s, col_rule));
}

/*
 *	A glyph box: the bordered square the hint bar puts a button letter in.
 */
static void dc_ui_glyph(SDL_Surface *s, int x, int cy, const char *ch,
                        const sdl_font_info *font, uint16 style, bool round)
{
	int w = 15, h = 15;
	int y = cy - h / 2;
	uint32 border = map(s, col_rulehot);
	int tw = text_width(ch, font, style);

	(void)round;

	dc_ui_fill(s, x, y, w, 2, border);
	dc_ui_fill(s, x, y + h - 2, w, 2, border);
	dc_ui_fill(s, x, y, 2, h, border);
	dc_ui_fill(s, x + w - 2, y, 2, h, border);

	draw_text(s, ch, x + (w - tw) / 2,
	          y + (h - font->get_line_height()) / 2 + font->get_ascent(),
	          map(s, col_item), font, style);
}

/*
 *	The hint bar along the bottom: glyph-and-label pairs from the left, and one
 *	right-aligned string, which on the main menu is the build tag.
 *
 *	That placement matters. dc_build_stamp() writes the build tag straight into
 *	VRAM at row 452, which UI-BRIEF measured as inside the overscan a television
 *	eats -- so the number identifying the build has been unreadable on the only
 *	device where it matters. Here it is inside the safe area.
 */
void dc_ui_hints(SDL_Surface *s, int y,
                 const dc_ui_hint *hints, int count, const char *right,
                 const sdl_font_info *font, uint16 style)
{
	int x = DC_UI_EDGE;
	int cy = y + DC_UI_HINT_H / 2;
	int baseline = cy - font->get_line_height() / 2 + font->get_ascent();
	int i;

	for (i = 0; i < count; i++) {
		int lw;

		dc_ui_glyph(s, x, cy, hints[i].glyph, font, style, hints[i].round);
		x += 15 + 6;

		lw = dc_ui_tracked_width(hints[i].label, font, style, DC_UI_TRACK_LABEL);

		dc_ui_tracked_text(s, hints[i].label, x, baseline, map(s, col_label),
		                   font, style, DC_UI_TRACK_LABEL);

		x += lw + 22;
	}

	if (right && right[0]) {
		int rw = dc_ui_tracked_width(right, font, style, DC_UI_TRACK_LABEL);

		dc_ui_tracked_text(s, right, DC_UI_SAFE_R - rw, baseline,
		                   map(s, col_label), font, style, DC_UI_TRACK_LABEL);
	}
}

/*
 *	A saved game's row.
 *
 *	.srow in app.css: the name takes the space that is left, then elapsed, then
 *	the card, both in the label font. An empty slot is drawn in the disabled
 *	colour and says so, because a blank row on a television reads as a fault.
 */
void dc_ui_save_row(SDL_Surface *s, int x, int y, int w, int h,
                    const char *name, const char *elapsed, const char *card,
                    bool selected, bool empty,
                    const sdl_font_info *item_font, uint16 item_style,
                    const sdl_font_info *label_font, uint16 label_style)
{
	uint32 name_colour;
	int baseline = y + (h - item_font->get_line_height()) / 2 +
	               item_font->get_ascent();
	int lbase = y + (h - label_font->get_line_height()) / 2 +
	            label_font->get_ascent();
	int right = x + w - 12;

	if (selected) {
		dc_ui_fill(s, x, y, w, h, map(s, col_hotbar));
		dc_ui_fill(s, x, y, 3, h, map(s, col_hot));
		dc_ui_caret(s, x + 11, y + h / 2, map(s, col_hot), 10);
	}

	if (empty)
		name_colour = map(s, col_off);
	else if (selected)
		name_colour = map(s, col_hot);
	else
		name_colour = map(s, col_item);

	/* Card first, from the right, then elapsed beside it, then the name gets
	   whatever is left -- so a long level name is what gets truncated rather
	   than the numbers, which are fixed width and always wanted. */
	if (card && card[0]) {
		int cw = dc_ui_tracked_width(card, label_font, label_style,
		                             DC_UI_TRACK_LABEL);

		dc_ui_tracked_text(s, card, right - cw, lbase,
		                   selected ? map(s, col_hot) : map(s, col_label),
		                   label_font, label_style, DC_UI_TRACK_LABEL);
		right -= cw + 12;
	}

	if (elapsed && elapsed[0]) {
		int ew = dc_ui_tracked_width(elapsed, label_font, label_style,
		                             DC_UI_TRACK_LABEL);

		dc_ui_tracked_text(s, elapsed, right - ew, lbase,
		                   selected ? map(s, col_hot) : map(s, col_label),
		                   label_font, label_style, DC_UI_TRACK_LABEL);
		right -= ew + 12;
	}

	dc_ui_tracked_text(s, name, x + 30, baseline, name_colour,
	                   item_font, item_style, DC_UI_TRACK_ITEM);
}

/*
 *	Greedy word wrap. Text that will not fit in `lines` is dropped rather than
 *	spilling off the side of a television, which is the failure this layout is
 *	written around.
 */
void dc_ui_wrapped_text(SDL_Surface *s, const char *text, int x, int y,
                        int width, int lines, uint32 colour,
                        const sdl_font_info *font, uint16 style)
{
	const char *p = text;
	int line = 0;
	int lh = font->get_line_height() + 3;

	if (!text || !font)
		return;

	while (*p && line < lines) {
		char buf[160];
		int len = 0, last_space = -1;

		while (p[len] && len < (int)sizeof buf - 1) {
			buf[len] = p[len];
			buf[len + 1] = 0;

			if (p[len] == ' ')
				last_space = len;

			if (dc_ui_tracked_width(buf, font, style, DC_UI_TRACK_LABEL) > width) {
				if (last_space > 0)
					len = last_space;
				break;
			}

			len++;
		}

		{
			char out[160];

			memcpy(out, p, len);
			out[len] = 0;

			dc_ui_tracked_text(s, out, x, y + line * lh + font->get_ascent(),
			                   colour, font, style, DC_UI_TRACK_LABEL);
		}

		line++;
		p += len;

		while (*p == ' ')
			p++;
	}
}

#endif	/* DC */
