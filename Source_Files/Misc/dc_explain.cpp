/*
 *	dc_explain.cpp -- the line of plain English under a settings panel.
 *
 *	UI-HANDOFF.md section 4: every settings row carries one line explaining
 *	whatever is focused. More Sounds says it costs about 7.5 seconds a level
 *	load. Look Sensitivity says why it is separate from Turn.
 *
 *	This matters more on a console than it sounds. A desktop player can hover, or
 *	read a manual, or try a setting and put it back in two seconds. Someone
 *	across a room with a pad has none of that, and half these settings describe
 *	engine behaviour that is genuinely not guessable from a six-word label.
 *
 *	The mechanism is the focus callback added in Phase 1: dialog::activate_widget
 *	is the only place the highlight moves, so one hook there covers every screen.
 *
 *	w_static_text cannot do this job -- it holds a const char * and measures
 *	itself once in its constructor, so it can neither change nor keep a stable
 *	width while doing so.
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

#include "dc_explain.h"

/*
 *	Fixed width, so the line does not reflow the dialog every time the highlight
 *	moves -- a panel that changes height under the cursor is far worse than a
 *	sentence that wraps awkwardly.
 */
#define EXPLAIN_W	440

w_explain::w_explain(int max_lines) : widget(LABEL_FONT), lines(max_lines)
{
	if (lines > EXPLAIN_MAX_LINES)
		lines = EXPLAIN_MAX_LINES;

	for (int i = 0; i < EXPLAIN_MAX_LINES; i++)
		wrapped[i][0] = 0;

	rect.w = EXPLAIN_W;
	rect.h = font->get_line_height() * lines;
}

/*
 *	Greedy wrap to the widget width.
 *
 *	Text that will not fit is truncated rather than spilling: the last line it
 *	does fit is kept and the rest dropped. A sentence running off the side of a
 *	television is the failure this whole layout is written to avoid, and a
 *	half-sentence at least reads as a half-sentence.
 */
void w_explain::set(const char *text)
{
	int line = 0;
	const char *p = text;

	for (int i = 0; i < EXPLAIN_MAX_LINES; i++)
		wrapped[i][0] = 0;

	if (!text) {
		dirty = true;
		return;
	}

	while (*p && line < lines) {
		char buf[EXPLAIN_MAX_CHARS];
		int len = 0, last_space = -1;

		while (p[len] && len < EXPLAIN_MAX_CHARS - 1) {
			buf[len] = p[len];
			buf[len + 1] = 0;

			if (p[len] == ' ')
				last_space = len;

			if (text_width(buf, font, style) > rect.w) {
				// Back up to the last word break; if there was none, this is one
				// very long word and it has to be broken somewhere.
				if (last_space > 0)
					len = last_space;
				break;
			}

			len++;
		}

		memcpy(wrapped[line], p, len);
		wrapped[line][len] = 0;
		line++;

		p += len;
		while (*p == ' ')
			p++;
	}

	dirty = true;
}

void w_explain::draw(SDL_Surface *s) const
{
	int lh = font->get_line_height();

	for (int i = 0; i < lines; i++) {
		if (!wrapped[i][0])
			continue;

		draw_text(s, wrapped[i], rect.x, rect.y + i * lh + font->get_ascent(),
		          get_dialog_color(LABEL_COLOR), font, style);
	}
}

/*
 *	The registry.
 *
 *	One table, because only one dialog is ever on screen at a time and these
 *	screens are all modal. Reset by begin(), which every screen calls before
 *	adding its rows.
 */

static struct {
	widget *w;
	const char *text;
} entries[DC_EXPLAIN_MAX];

static int entry_count = 0;
static w_explain *current_line = NULL;

static void explain_focus(dialog *d, widget *w, int index, void *arg)
{
	(void)d;
	(void)index;
	(void)arg;

	if (!current_line)
		return;

	for (int i = 0; i < entry_count; i++) {
		if (entries[i].w == w) {
			current_line->set(entries[i].text);
			return;
		}
	}

	// Nothing registered for this widget -- ACCEPT, a spacer, a title. Clearing
	// rather than leaving the previous row's text is the honest thing: stale
	// help beside a different control is worse than none.
	current_line->set(NULL);
}

void dc_explain_begin(w_explain *line)
{
	entry_count = 0;
	current_line = line;
}

void dc_explain_add(widget *w, const char *text)
{
	if (entry_count >= DC_EXPLAIN_MAX || !w || !text)
		return;

	entries[entry_count].w = w;
	entries[entry_count].text = text;
	entry_count++;
}

void dc_explain_arm(dialog *d)
{
	if (d)
		d->set_focus_proc(explain_focus, NULL);
}

void dc_explain_end(void)
{
	entry_count = 0;
	current_line = NULL;
}

#endif	/* DC */
