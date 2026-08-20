/*
 *	dc_screen.cpp -- one screen, drawn and driven.
 *
 *	Every screen in mockups/prototype is the same arrangement: a title and a
 *	kicker, a rule, a panel of rows, sometimes an explainer or a state column,
 *	a rule, and a hint bar. This draws that and runs it, so each screen is a
 *	table of rows rather than a hand-built dialog.
 *
 *	WHY ITS OWN LOOP RATHER THAN A dialog.
 *
 *	Max hit a case where the pad stopped selecting anything after leaving a
 *	preferences screen. Nested dialogs make that class of fault easy: a focus
 *	callback outliving the widget it points at, a child clearing state its parent
 *	still depends on, a game starting underneath a dialog that is still running.
 *	Every one of those is a lifetime bug, and a modal function with one loop, one
 *	exit and no widget objects does not have lifetimes to get wrong.
 *
 *	It also means the design can be drawn exactly. dialog::draw paints a frame
 *	from theme bitmaps and stacks widgets vertically; this design has neither a
 *	frame nor a vertical stack.
 *
 *	THERE IS ALWAYS A WAY OUT. B and Start both return DC_SCREEN_BACK, from
 *	anywhere, unconditionally, before anything else is considered. No row, no
 *	value, no state can suppress them.
 */

#include "cseries.h"

#include "sdl_dialogs.h"
#include "sdl_fonts.h"
#include "sdl_widgets.h"
#include "shape_descriptors.h"
#include "screen_drawing.h"
#include "shell.h"
#include "world.h"
#include "screen.h"

#include <string.h>

#ifdef DC

#include "dc_screen.h"
#include "dc_plate.h"
#include "build_id.h"

extern "C" {
	void dc_input_poll(void);
	void dc_input_set_ingame(int yes);
	void dc_trace(int slot, const char *fmt, ...);
}

/* From app.css: .title top:60, .hr top:92, .explain bottom:82, .hint bottom:40. */
#define SCR_TITLE_Y		60
#define SCR_RULE_TOP	92
#define SCR_RULE_BOT	412
#define SCR_HINT_Y		422
#define SCR_EXPLAIN_Y	372		/* bottom:82, min-height 26 */

static int row_top(const struct dc_screen *sc)
{
	return sc->panel_y + 2 + DC_UI_CAP_H + 2;
}

/* How many rows each panel holds. Column 1 is empty when there is no split. */
static int col_count(const struct dc_screen *sc, int col)
{
	if (sc->split <= 0)
		return col == 0 ? sc->nrows : 0;

	return col == 0 ? sc->split : sc->nrows - sc->split;
}

static int panel_h(const struct dc_screen *sc, int col)
{
	return 2 + DC_UI_CAP_H + 2 + col_count(sc, col) * sc->row_h + 2;
}

/* Which panel a row is in, and how far down it. */
static int row_col(const struct dc_screen *sc, int i)
{
	return (sc->split > 0 && i >= sc->split) ? 1 : 0;
}

static int row_index_in_col(const struct dc_screen *sc, int i)
{
	return (sc->split > 0 && i >= sc->split) ? i - sc->split : i;
}

/* Rows that cannot take the highlight are skipped rather than landed on. */
static bool selectable(const struct dc_screen *sc, int i)
{
	return i >= 0 && i < sc->nrows && !sc->rows[i].disabled;
}

/*
 *	Up and down stay inside a column when there are two, wrapping within it.
 *	Crossing columns is what Left and Right are for, and a highlight that slid
 *	sideways on its own would make a two-panel screen impossible to reason about.
 */
static int step(const struct dc_screen *sc, int from, int delta)
{
	int col = row_col(sc, from);
	int base = (col == 0) ? 0 : sc->split;
	int n = col_count(sc, col);
	int i = row_index_in_col(sc, from);
	int tries;

	if (n < 1)
		return from;

	for (tries = 0; tries < n; tries++) {
		i += delta;

		if (i < 0)
			i = n - 1;
		else if (i >= n)
			i = 0;

		if (selectable(sc, base + i))
			return base + i;
	}

	return from;
}

/* Left and Right across the split, keeping the same depth where possible. */
static int step_col(const struct dc_screen *sc, int from, int delta)
{
	int col = row_col(sc, from);
	int want = col + delta;
	int depth, n, base, i;

	if (sc->split <= 0 || want < 0 || want > 1)
		return from;

	depth = row_index_in_col(sc, from);
	n = col_count(sc, want);
	base = (want == 0) ? 0 : sc->split;

	if (n < 1)
		return from;

	if (depth >= n)
		depth = n - 1;

	for (i = 0; i < n; i++) {
		int d = depth + i;

		if (d < n && selectable(sc, base + d))
			return base + d;

		d = depth - i;
		if (d >= 0 && selectable(sc, base + d))
			return base + d;
	}

	return from;
}

/*
 *	Left and Right on a row that has a value.
 *
 *	Toggles and selects wrap, because a pad has no way to know it has reached the
 *	end otherwise. Sliders clamp, because a volume that jumps from silent to
 *	loudest on one press is a genuinely bad surprise.
 */
static void adjust(struct dc_row *r, int delta)
{
	int n;

	switch (r->kind) {
	case DC_ROW_TOGGLE:
		r->value = r->value ? 0 : 1;
		break;

	case DC_ROW_SELECT:
		n = 0;
		while (r->opts && r->opts[n])
			n++;
		if (n < 1)
			break;
		r->value = (r->value + delta + n) % n;
		break;

	case DC_ROW_SLIDER:
		r->value += delta;
		if (r->value < 0)
			r->value = 0;
		if (r->value > r->max)
			r->value = r->max;
		break;

	default:
		break;
	}
}

/* What the right-hand side of a row reads, for the kinds that show text. */
static const char *row_value(const struct dc_row *r)
{
	int n;

	switch (r->kind) {
	case DC_ROW_BIND:
		/* The button name, already formatted by the binding screen. */
		return r->col2;

	case DC_ROW_TOGGLE:
	case DC_ROW_SELECT:
		n = 0;
		while (r->opts && r->opts[n])
			n++;
		if (r->value >= 0 && r->value < n)
			return r->opts[r->value];
		return NULL;

	default:
		return NULL;
	}
}

/*
 *	The blocks a slider is drawn as: five wide, twelve tall, two apart, filled to
 *	the value and dim beyond it. Right-aligned so the meter ends where a text
 *	value would.
 */
static void draw_slider(SDL_Surface *s, int right, int cy, int value, int max,
                        bool selected)
{
	uint32 on = selected ? dc_ui_colour(DC_UI_HOT, s)
	                     : dc_ui_colour(DC_UI_ITEM, s);
	uint32 off = dc_ui_colour(DC_UI_OFF, s);
	int n = max + 1;
	int w = n * 5 + (n - 1) * 2;
	int x = right - w;
	int i;

	for (i = 0; i < n; i++)
		dc_ui_fill(s, x + i * 7, cy - 6, 5, 12, i <= value ? on : off);
}

static void draw_screen(struct dc_screen *sc)
{
	SDL_Surface *v = SDL_GetVideoSurface();
	const sdl_font_info *tf, *itf, *lf, *bf;
	uint16 ts, its, ls, bs;
	int i, px, pw, ph;

	if (!v)
		return;

	/*
	 *	ITEM_FONT for the title, not TITLE_FONT. app.css sets .title to 15px --
	 *	the same size as a row, just tracked much wider at .30em -- while the
	 *	theme's title font is 32px. Using the theme's made the heading twice the
	 *	size the design asks for and threw the whole screen's balance out.
	 */
	tf  = get_dialog_font(ITEM_FONT, ts);
	itf = get_dialog_font(ITEM_FONT, its);
	/*
	 *	MESSAGE_FONT for the small text, not LABEL_FONT.
	 *
	 *	The theme sets label and item to the same 16px face, so every caption,
	 *	kicker, value and hint was being drawn at the size of a menu row -- which
	 *	is why the screens read as chunkier than the design, where those are 11px
	 *	against 18. MESSAGE_FONT is the theme's small face, id 1402 at 10.
	 *
	 *	BUTTON_FONT is 14, which is exactly what the design sets a binding row to.
	 */
	lf  = get_dialog_font(MESSAGE_FONT, ls);
	bf  = get_dialog_font(BUTTON_FONT, bs);

	if (!tf || !itf || !lf)
		return;

	if (!bf) {
		bf = itf;
		bs = its;
	}

	if (sc->over_game) {
		/* .scrim: rgba(2,6,8,.90) over the running game. */
		SDL_Color ink = { 0x02, 0x06, 0x08, 0 };
		dc_ui_blend(v, 0, 0, v->w, v->h, ink, 230);
	} else {
		dc_plate_select(DC_PLATE_PLAIN);
		dc_plate_to_screen();
	}

	/* Title, kicker, rule. The rule tracks the title, 32px below it, which is
	   the relationship the prototype keeps on every screen. */
	dc_ui_tracked_text(v, sc->title, DC_UI_EDGE,
	                   sc->title_y + tf->get_ascent(),
	                   dc_ui_colour(DC_UI_FACE, v), tf, ts, DC_UI_TRACK_TITLE);

	if (sc->kicker) {
		int kw = dc_ui_tracked_width(sc->kicker, lf, ls, DC_UI_TRACK_CAP);

		dc_ui_tracked_text(v, sc->kicker, DC_UI_SAFE_R - kw,
		                   sc->title_y + 4 + lf->get_ascent(),
		                   dc_ui_colour(DC_UI_LABEL, v), lf, ls,
		                   DC_UI_TRACK_CAP);
	}

	dc_ui_rule(v, DC_UI_EDGE, sc->title_y + 32, 560, true);

	/*
	 *	One panel, or two side by side. The prototype's binding screen puts them
	 *	at x=40 and x=332, both 270 wide, and gives the right one a caption bar
	 *	with no text so the two line up.
	 */
	px = DC_UI_EDGE;
	pw = sc->panel_w;
	ph = panel_h(sc, 0);

	dc_ui_panel(v, px, sc->panel_y, pw, ph);
	dc_ui_caption(v, px + 1, sc->panel_y + 2, pw - 2, sc->cap,
	              sc->cap_right, sc->cap_right_hot, lf, ls);

	if (sc->split > 0) {
		int px2 = DC_UI_SAFE_R - pw;

		dc_ui_panel(v, px2, sc->panel_y, pw, panel_h(sc, 1));
		dc_ui_caption(v, px2 + 1, sc->panel_y + 2, pw - 2,
		              sc->cap2 ? sc->cap2 : "", NULL, false, lf, ls);
	}

	for (i = 0; i < sc->nrows; i++) {
		struct dc_row *r = &sc->rows[i];
		int col = row_col(sc, i);
		int px_r = (col == 0) ? px : DC_UI_SAFE_R - pw;
		int y = row_top(sc) + row_index_in_col(sc, i) * sc->row_h;
		bool on = (i == sc->cursor);
		int cy = y + sc->row_h / 2;

		if (r->kind == DC_ROW_SAVE) {
			dc_ui_save_row(v, px_r + 1, y, pw - 2, sc->row_h,
			               r->label, r->col2, r->col3,
			               on, r->disabled, itf, its, lf, ls);
			continue;
		}

		/*
		 *	A binding row carries two pieces of text in 270px, so the design sets
		 *	it smaller than an ordinary row -- 14px against 18. The theme offers
		 *	16 and 10 and nothing between, so a bind row takes the smaller of the
		 *	two. At 16 the widest pair, "2ND TRIGGER" against "L TRIGGER", does
		 *	not fit at any tracking, and shrinking the label to make it fit is
		 *	how you end up displaying "2ND TRIGG".
		 */
		if (r->kind == DC_ROW_BIND)
			dc_ui_row(v, px_r + 1, y, pw - 2, sc->row_h,
			          r->label, row_value(r), on, r->disabled,
			          bf, bs, lf, ls);
		else
			dc_ui_row(v, px_r + 1, y, pw - 2, sc->row_h,
			          r->label, row_value(r), on, r->disabled,
			          itf, its, lf, ls);

		if (r->kind == DC_ROW_SLIDER)
			draw_slider(v, px_r + pw - 13, cy, r->value, r->max, on);

		if (r->kind == DC_ROW_SUBMENU)
			dc_ui_caret(v, px_r + pw - 22, cy,
			            on ? dc_ui_colour(DC_UI_HOT, v)
			               : dc_ui_colour(DC_UI_LABEL, v), 10);
	}

	/* The state column, opposite the panel: dim heading, brighter value. */
	for (i = 0; i < sc->nstate; i++) {
		int w = dc_ui_tracked_width(sc->state[i], lf, ls, DC_UI_TRACK_LABEL);
		uint32 c = (i & 1) ? dc_ui_colour(DC_UI_ITEM, v)
		                   : dc_ui_colour(DC_UI_LABEL, v);

		dc_ui_tracked_text(v, sc->state[i], DC_UI_SAFE_R - w,
		                   sc->panel_y + 4 + i * 21 + lf->get_ascent(),
		                   c, lf, ls, DC_UI_TRACK_LABEL);
	}

	/* A banner wins the explainer's place: it is telling the player something
	   about the whole screen, which outranks help for one row. */
	if (sc->banner) {
		dc_ui_tracked_text(v, sc->banner, DC_UI_EDGE,
		                   SCR_EXPLAIN_Y + lf->get_ascent(),
		                   dc_ui_colour(DC_UI_HOT, v), lf, ls, DC_UI_TRACK_CAP);
	} else if (sc->explain && selectable(sc, sc->cursor) &&
	           sc->rows[sc->cursor].note) {
		dc_ui_wrapped_text(v, sc->rows[sc->cursor].note,
		                   DC_UI_EDGE, SCR_EXPLAIN_Y, 560, 2,
		                   dc_ui_colour(DC_UI_LABEL, v), lf, ls);
	}

	dc_ui_rule(v, DC_UI_EDGE, SCR_RULE_BOT, 560, false);
	dc_ui_hints(v, SCR_HINT_Y, sc->hints, sc->nhints, "b" DC_BUILD_NUM, lf, ls);

	SDL_UpdateRect(v, 0, 0, 0, 0);
}

int dc_screen_run(struct dc_screen *sc)
{
	int was_ingame = 0;
	int result = DC_SCREEN_BACK;
	bool done = false;
	bool dirty = true;

	if (!sc || sc->nrows < 1)
		return DC_SCREEN_BACK;

	if (sc->row_h < 1)
		sc->row_h = DC_UI_ROW_H;
	if (sc->panel_w < 1)
		sc->panel_w = 560;
	if (sc->panel_y < 1)
		sc->panel_y = 150;
	if (sc->title_y < 1)
		sc->title_y = SCR_TITLE_Y;

	/*
	 *	A panel that reaches past the explainer line draws over it, or is drawn
	 *	over -- which is how CONFIGURE CONTROLLER ended up with a sentence
	 *	through it on hardware. The geometry is static per screen, so this can
	 *	only fire on a screen nobody has looked at; say so rather than let it
	 *	ship silently.
	 */
	if (sc->explain || sc->banner) {
		int bottom = sc->panel_y + panel_h(sc, 0);

		if (bottom > SCR_EXPLAIN_Y)
			dc_trace(18, "screen '%s': panel ends at %d, explainer at %d",
			         sc->title ? sc->title : "?", bottom, SCR_EXPLAIN_Y);
	}

	/* Start somewhere the highlight is allowed to be. */
	if (!selectable(sc, sc->cursor)) {
		sc->cursor = 0;
		if (!selectable(sc, 0))
			sc->cursor = step(sc, 0, 1);
	}

	/*
	 *	The menu binding table, whatever the caller was doing. The pause menu
	 *	opens from gameplay, where the pad is mapped for movement, and without
	 *	this its buttons would do nothing at all.
	 */
	was_ingame = 0;
	dc_input_set_ingame(0);

	while (!done) {
		SDL_Event e;

		if (dirty) {
			draw_screen(sc);
			dirty = false;
		}

		dc_input_poll();

		e.type = SDL_NOEVENT;
		SDL_PollEvent(&e);

		if (e.type != SDL_KEYDOWN) {
			SDL_Delay(10);
			continue;
		}

		switch (e.key.keysym.sym) {
		/*
		 *	Out, first and unconditionally. Nothing below can shadow this, and
		 *	no row state can suppress it -- which is the whole guarantee.
		 */
		case SDLK_BACKSPACE:		/* B     */
		case SDLK_ESCAPE:			/* Start */
			result = DC_SCREEN_BACK;
			done = true;
			break;

		case SDLK_UP:
			sc->cursor = step(sc, sc->cursor, -1);
			dirty = true;
			break;

		case SDLK_DOWN:
			sc->cursor = step(sc, sc->cursor, +1);
			dirty = true;
			break;

		case SDLK_LEFT:
			if (sc->split > 0)
				sc->cursor = step_col(sc, sc->cursor, -1);
			else
				adjust(&sc->rows[sc->cursor], -1);
			dirty = true;
			break;

		case SDLK_RIGHT:
			if (sc->split > 0)
				sc->cursor = step_col(sc, sc->cursor, +1);
			else
				adjust(&sc->rows[sc->cursor], +1);
			dirty = true;
			break;

		case SDLK_RETURN:
		case SDLK_KP_ENTER: {		/* A */
			struct dc_row *r = &sc->rows[sc->cursor];

			if (r->disabled)
				break;

			if (r->kind == DC_ROW_TOGGLE || r->kind == DC_ROW_SELECT ||
			    r->kind == DC_ROW_SLIDER) {
				/* A on a value cycles it, so a player who never discovers
				   Left/Right is not stuck with the default forever. */
				adjust(r, +1);
				dirty = true;
				break;
			}

			if (r->id) {
				result = r->id;
				done = true;
			}
			break;
		}

		case SDLK_INSERT: {			/* Y */
			struct dc_row *r = &sc->rows[sc->cursor];

			if (r->id) {
				result = DC_SCREEN_Y | r->id;
				done = true;
			}
			break;
		}

		case SDLK_DELETE: {			/* X */
			struct dc_row *r = &sc->rows[sc->cursor];

			if (!r->disabled && r->id) {
				result = DC_SCREEN_X | r->id;
				done = true;
			}
			break;
		}

		default:
			break;
		}
	}

	dc_input_set_ingame(was_ingame);

	return result;
}

#endif	/* DC */
