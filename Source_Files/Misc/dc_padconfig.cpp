/*
 *	dc_padconfig.cpp -- CONFIGURE CONTROLLER.
 *
 *	Replaces CONFIGURE KEYBOARD, which opened a screen for a device the console
 *	does not have. Every engine action is bindable to any pad button, over two
 *	pages, in two columns.
 *
 *	WHY A GRID WIDGET AND NOT TWENTY ROWS. A dialog stacks its widgets
 *	vertically: each one reports a height and the next goes below it. Thirteen
 *	rows at a size readable across a room is 442px, and the safe area is 400. Two
 *	columns is the answer, and rather than teach dialog::layout about columns --
 *	which every other screen would then have to be checked against -- the whole
 *	grid is one widget that owns its own cursor.
 *
 *	That has one consequence worth stating plainly: a widget that swallows UP and
 *	DOWN can trap the highlight, which is exactly the bug that made the save list
 *	unreachable for weeks. So the grid releases focus at its edges -- UP on the
 *	top row and DOWN on the bottom row are passed through to the dialog, and the
 *	buttons below stay reachable.
 *
 *	Buttons, not keys. The player rebinds which button does a thing; which key
 *	that action sends is still the engine's own keycodes[], so nothing else in
 *	the game has to know this screen exists.
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
#include "preferences.h"

#include <stdio.h>
#include <string.h>

#ifdef DC

#include "dc_padconfig.h"
#include "dc_plate.h"

extern "C" void dc_trace(int slot, const char *fmt, ...);

/*
 *	The engine's twenty actions, in its own order. Indices into
 *	input_preferences->dc_pad_bindings[] and keycodes[].
 */
static const char *action_name[20] = {
	"Move Forward", "Move Backward", "Turn Left", "Turn Right",
	"Sidestep Left", "Sidestep Right",
	"Glance Left", "Glance Right",
	"Look Up", "Look Down", "Look Ahead",
	"Previous Weapon", "Next Weapon", "Trigger", "2nd Trigger",
	"Sidestep", "Run/Swim", "Look",
	"Action", "Auto Map"
};

/*
 *	Two pages. The split is not cosmetic: with ten buttons for twenty actions, a
 *	screen listing all twenty as equals invites the player to spend a button on
 *	Glance Left before Trigger.
 *
 *	Look Up / Down / Ahead are on the main page because they are digital key
 *	actions in the engine, so a button gives exactly what the original keyboard
 *	gave -- and because in Move mode they are the only way to aim vertically.
 *	Turn Left / Right are on ADVANCED because the stick owns turning in both
 *	modes, so a button for it is a preference rather than a necessity.
 */
static const int page_main[] = { 0, 1, 4, 5, 8, 9, 10, 11, 12, 13, 14, 18, 19 };
static const int page_adv[]  = { 2, 3, 6, 7, 15, 16, 17 };

#define N_MAIN	(int)(sizeof(page_main) / sizeof(page_main[0]))
#define N_ADV	(int)(sizeof(page_adv) / sizeof(page_adv[0]))

/* An action on neither page could never be bound and nothing would say so. */
typedef char pad_pages_cover_every_action[(N_MAIN + N_ADV == 20) ? 1 : -1];

/*
 *	Ten buttons a capture can actually return: ids 1 to 10. Id 11 is Start, which
 *	capture reads as cancel and never hands back, because it is the one button
 *	that must always mean "get me out of here".
 */
#define ASSIGNABLE_BUTTONS	10

/* Edited here, so CANCEL on either page really does cancel. */
static int16 pad_work[NUMBER_OF_KEYS];

/* Set by the grid or by the buttons below it; read by run_page. */
static int page_flip_wanted;
static int page_defaults_wanted;

/*
 *	The grid.
 */
class w_pad_grid : public widget {
public:
	w_pad_grid(const int *actions, int count, dialog *owner)
		: widget(ITEM_FONT), acts(actions), n(count), cursor(0),
		  capturing(false), d(owner)
	{
		rows = (n + 1) / 2;
	}

	int layout(void)
	{
		line_h = font->get_line_height() + 4;

		rect.w = 520;
		rect.x = -rect.w / 2;
		rect.h = rows * line_h;

		return rect.h;
	}

	void draw(SDL_Surface *s) const
	{
		int col_w = rect.w / 2;
		int i;

		for (i = 0; i < n; i++) {
			int col = i / rows;
			int row = i % rows;
			int x = rect.x + col * col_w;
			int y = rect.y + row * line_h;
			bool on = active && i == cursor;
			uint32 colour = on ? get_dialog_color(ITEM_ACTIVE_COLOR)
			                   : get_dialog_color(ITEM_COLOR);

			if (on) {
				SDL_Rect bar = { x, y, col_w - 8, line_h };
				SDL_FillRect(s, &bar, get_dialog_color(KEY_BINDING_COLOR));
			}

			draw_text(s, action_name[acts[i]], x + 6, y + font->get_ascent(),
			          on ? colour : get_dialog_color(LABEL_COLOR), font, style);

			/* The button, right-aligned in its column. */
			{
				const char *bn;
				int bw;

				if (on && capturing)
					bn = "press...";
				else
					bn = dc_input_button_name(pad_work[acts[i]]);

				bw = text_width(bn, font, style);

				draw_text(s, bn, x + col_w - 14 - bw, y + font->get_ascent(),
				          colour, font, style);
			}
		}
	}

	/*
	 *	Cursor movement, capture, and releasing focus at the edges.
	 *
	 *	Returning early rather than swallowing the event is what releases focus:
	 *	dialog::event then moves to the next widget exactly as it would for any
	 *	other. That is the whole mechanism, and it is what keeps ACCEPT and the
	 *	page buttons reachable from inside the grid.
	 */
	void event(SDL_Event &e)
	{
		if (e.type != SDL_KEYDOWN)
			return;

		if (capturing) {
			int id = dc_input_take_capture();

			if (id >= 0) {
				if (id > 0)
					assign(cursor, id);

				capturing = false;
				dc_input_end_capture();
				dirty = true;
			}

			e.type = SDL_NOEVENT;
			return;
		}

		switch (e.key.keysym.sym) {
		case SDLK_UP:
			if (cursor % rows == 0)
				return;			/* top of a column: let the dialog have it */
			cursor--;
			break;

		case SDLK_DOWN:
			if (cursor % rows == rows - 1 || cursor == n - 1)
				return;			/* bottom of a column: same */
			cursor++;
			break;

		case SDLK_LEFT:
			if (cursor >= rows)
				cursor -= rows;
			break;

		case SDLK_RIGHT:
			if (cursor + rows < n)
				cursor += rows;
			break;

		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			capturing = true;
			dc_input_begin_capture();
			break;

		/*
		 *	X and Y, from the fixed menu table. The buttons below the grid do the
		 *	same things and stay reachable by navigation -- these are the
		 *	shortcut, not the only way, because a screen whose only exits are
		 *	shortcuts is a screen someone gets stuck on.
		 */
		case SDLK_DELETE:
			page_flip_wanted = 1;
			if (d)
				d->quit(0);
			break;

		case SDLK_INSERT:
			page_defaults_wanted = 1;
			if (d)
				d->quit(0);
			break;

		default:
			return;
		}

		dirty = true;
		e.type = SDL_NOEVENT;
	}

	void click(int, int)
	{
		if (!capturing) {
			capturing = true;
			dc_input_begin_capture();
			dirty = true;
		}
	}

	bool is_selectable(void) const { return true; }

	/* How many distinct buttons are spent, across both pages. */
	static int spent(void)
	{
		int used[16];
		int i, count = 0;

		memset(used, 0, sizeof used);

		for (i = 0; i < NUMBER_OF_KEYS; i++) {
			int id = pad_work[i];

			if (id > 0 && id < 16 && !used[id]) {
				used[id] = 1;
				count++;
			}
		}

		return count;
	}

private:
	/*
	 *	Take a button for this action, releasing it from whoever had it.
	 *
	 *	The scan is over pad_work rather than over the visible rows, so it
	 *	reaches actions listed on the other page. A button doing two things at
	 *	once is never what was meant.
	 */
	void assign(int index, int id)
	{
		int action = acts[index];
		int i;

		for (i = 0; i < NUMBER_OF_KEYS; i++)
			if (i != action && pad_work[i] == id)
				pad_work[i] = 0;

		pad_work[action] = (int16)id;
	}

	const int *acts;
	int n;
	int rows;
	int line_h;
	int cursor;
	bool capturing;
	dialog *d;
};

/*
 *	The header, which says how many buttons are left.
 *
 *	The shortfall is put on screen rather than hidden: ten buttons for twenty
 *	actions means some actions cannot have one, and a screen that pretends
 *	otherwise leaves the player to discover it by running out.
 */
class w_spent : public widget {
public:
	w_spent() : widget(LABEL_FONT) {}

	int layout(void)
	{
		rect.w = 520;
		rect.x = -rect.w / 2;
		rect.h = font->get_line_height();

		return rect.h;
	}

	void draw(SDL_Surface *s) const
	{
		char line[64];
		int used = w_pad_grid::spent();
		uint32 colour = (used >= ASSIGNABLE_BUTTONS)
		                    ? get_dialog_color(ITEM_ACTIVE_COLOR)
		                    : get_dialog_color(LABEL_COLOR);

		snprintf(line, sizeof line, "%d OF %d BUTTONS SPENT",
		         used, ASSIGNABLE_BUTTONS);

		draw_text(s, line, rect.x + 6, rect.y + font->get_ascent(),
		          colour, font, style);
	}

	bool is_selectable(void) const { return false; }
};

/*
 *	One page. Both are the same screen with a different action list.
 *
 *	Returns 1 if the player asked for the other page, 0 if they left.
 */
static void want_flip(void *) { page_flip_wanted = 1; }
static void want_defaults(void *) { page_defaults_wanted = 1; }

static int run_page(const char *title, const int *acts, int count)
{
	dialog d;

	page_flip_wanted = 0;
	page_defaults_wanted = 0;

	d.add(new w_static_text(title, TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
	d.add(new w_spent());
	d.add(new w_spacer());
	d.add(new w_pad_grid(acts, count, &d));
	d.add(new w_spacer());
	d.add(new w_static_text("A binds    X other page    Y defaults    Start done",
	                        LABEL_FONT, LABEL_COLOR));
	d.add(new w_spacer());
	d.add(new w_button("OTHER PAGE", want_flip, &d));
	d.add(new w_button("DEFAULTS", want_defaults, &d));
	d.add(new w_spacer());
	d.add(new w_left_button("ACCEPT", dialog_ok, &d));
	d.add(new w_right_button("CANCEL", dialog_cancel, &d));

	clear_screen();

	int result = d.run();

	if (page_defaults_wanted) {
		dc_input_default_bindings(pad_work, NUMBER_OF_KEYS);
		return 2;			/* redraw this page */
	}

	if (page_flip_wanted)
		return 1;

	return result == 0 ? 0 : -1;
}

/*
 *	CONFIGURE CONTROLLER.
 *
 *	Edits a working copy and only commits on ACCEPT, so backing out of either
 *	page leaves the bindings alone.
 */
void dc_pad_config(void)
{
	bool on_main = true;
	int i;

	for (i = 0; i < NUMBER_OF_KEYS; i++)
		pad_work[i] = input_preferences->dc_pad_bindings[i];

	for (;;) {
		int r = on_main ? run_page("CONFIGURE CONTROLLER", page_main, N_MAIN)
		                : run_page("ADVANCED CONTROLS", page_adv, N_ADV);

		if (r == 2)
			continue;			/* DEFAULTS: same page, new values */

		if (r == 1) {
			on_main = !on_main;
			continue;
		}

		if (r < 0)
			return;				/* CANCEL: pad_work is discarded */

		break;					/* ACCEPT */
	}

	{
		bool changed = false;

		for (i = 0; i < NUMBER_OF_KEYS; i++)
			if (pad_work[i] != input_preferences->dc_pad_bindings[i]) {
				input_preferences->dc_pad_bindings[i] = pad_work[i];
				changed = true;
			}

		/* Push regardless: the stick mode may have moved even if no button did. */
		dc_apply_pad_bindings();

		if (changed)
			write_preferences();
	}
}

#endif	/* DC */
