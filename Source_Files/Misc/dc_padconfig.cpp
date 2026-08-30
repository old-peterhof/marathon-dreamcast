/*
 *	dc_padconfig.cpp -- CONFIGURE CONTROLLER.
 *
 *	Replaces CONFIGURE KEYBOARD, which opened a screen for a device this console
 *	does not have. Every engine action is bindable to any pad button, over two
 *	pages, each page two panels side by side.
 *
 *	The layout is the prototype's, measured off a render of it rather than
 *	guessed: panels at x=40 and x=332, both 270 wide, top at 128, a 20px caption
 *	bar, and 26px rows. The main page puts 7 of its 13 actions in the left panel
 *	and the advanced page 4 of its 7; the right panel gets a caption bar with no
 *	text so the two line up.
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
#include "dc_screen.h"
#include "dc_plate.h"

extern "C" {
	void dc_trace(int slot, const char *fmt, ...);
	void dc_input_poll(void);
}

/*
 *	The engine's twenty actions, in its own order. Indices into
 *	input_preferences->dc_pad_bindings[] and keycodes[].
 */
static const char *action_name[20] = {
	"MOVE FORWARD", "MOVE BACKWARD", "TURN LEFT", "TURN RIGHT",
	"SIDESTEP LEFT", "SIDESTEP RIGHT",
	"GLANCE LEFT", "GLANCE RIGHT",
	"LOOK UP", "LOOK DOWN", "LOOK AHEAD",
	"PREVIOUS WEAPON", "NEXT WEAPON", "TRIGGER", "2ND TRIGGER",
	"SIDESTEP", "RUN/SWIM", "LOOK",
	"ACTION", "AUTO MAP"
};

/*
 *	The two pages, in the order and grouping the prototype lays them out.
 *
 *	The split is not cosmetic. With ten buttons for twenty actions, a screen
 *	listing all twenty as equals invites the player to spend a button on Glance
 *	Left before Trigger. Look Up, Look Down and Look Ahead are on the main page
 *	because they are digital key actions in the engine, so a button gives exactly
 *	what the original keyboard gave, and because in Move mode they are the only
 *	way to aim vertically. Turn Left and Turn Right are on ADVANCED because the
 *	stick owns turning in both modes.
 */
static const int page_main[] = { 0, 1, 4, 5, 8, 9, 10, 11, 12, 13, 14, 18, 19 };
static const int page_adv[]  = { 2, 3, 6, 7, 15, 16, 17 };

#define N_MAIN	(int)(sizeof(page_main) / sizeof(page_main[0]))
#define N_ADV	(int)(sizeof(page_adv) / sizeof(page_adv[0]))

#define SPLIT_MAIN	7
#define SPLIT_ADV	4

/* An action on neither page could never be bound and nothing would say so. */
typedef char pad_pages_cover_every_action[(N_MAIN + N_ADV == 20) ? 1 : -1];

/*
 *	Ten buttons a capture can actually return: ids 1 to 10. Id 11 is Start, which
 *	capture reads as cancel and never hands back, because it is the one button
 *	that must always mean "get me out of here".
 */
#define ASSIGNABLE_BUTTONS	10

/* Edited here, so leaving without binding anything changes nothing. */
static int16 pad_work[NUMBER_OF_KEYS];

/*
 *	The capture overlay: a bordered amber box over a dimmed screen, saying what
 *	is being bound. .capture in app.css -- 420 wide at (110,180), amber on all
 *	four sides.
 *
 *	Drawn directly rather than being another dc_screen, because it has no rows
 *	and no navigation. The only thing that can happen on it is a button press.
 */
static void draw_capture(const char *action)
{
	SDL_Surface *v = SDL_GetVideoSurface();
	const sdl_font_info *itf, *lf;
	uint16 its, ls;
	SDL_Color ink = { 0x02, 0x05, 0x07, 0 };
	int x = 110, y = 180, w = 420, h = 104;
	char line[96];

	if (!v)
		return;

	itf = get_dialog_font(ITEM_FONT, its);
	lf  = get_dialog_font(MESSAGE_FONT, ls);

	if (!itf || !lf)
		return;

	dc_ui_blend(v, 0, 0, v->w, v->h, ink, 204);		/* rgba(2,5,7,.80) */

	dc_ui_fill(v, x, y, w, h, dc_ui_colour(DC_UI_PANEL, v));
	dc_ui_fill(v, x, y, w, 2, dc_ui_colour(DC_UI_HOT, v));
	dc_ui_fill(v, x, y + h - 2, w, 2, dc_ui_colour(DC_UI_HOT, v));
	dc_ui_fill(v, x, y, 2, h, dc_ui_colour(DC_UI_HOT, v));
	dc_ui_fill(v, x + w - 2, y, 2, h, dc_ui_colour(DC_UI_HOT, v));

	dc_ui_tracked_text(v, "BIND", x + 12, y + 6 + lf->get_ascent(),
	                   dc_ui_colour(DC_UI_HOT, v), lf, ls, DC_UI_TRACK_CAP);
	dc_ui_fill(v, x, y + 22, w, 2, dc_ui_colour(DC_UI_RULE, v));

	snprintf(line, sizeof line, "PRESS A BUTTON FOR %s", action);
	dc_ui_wrapped_text(v, line, x + 20, y + 36, w - 40, 2,
	                   dc_ui_colour(DC_UI_FACE, v), itf, its);

	dc_ui_wrapped_text(v, "START CANCELS", x + 20, y + h - 26, w - 40, 1,
	                   dc_ui_colour(DC_UI_LABEL, v), lf, ls);

	SDL_UpdateRect(v, 0, 0, 0, 0);
}

/*
 *	Wait for a button and give it to `action`.
 *
 *	The driver stops injecting keys while capturing, so the press that names a
 *	binding cannot also drive the screen underneath. Start comes back as 0 and
 *	means cancel; capture also times out, so a mode entered by accident cannot
 *	strand the interface.
 */
static void capture_for(int action)
{
	draw_capture(action_name[action]);

	dc_input_begin_capture();

	for (;;) {
		SDL_Event e;
		int id;

		dc_input_poll();

		e.type = SDL_NOEVENT;
		SDL_PollEvent(&e);

		id = dc_input_take_capture();

		if (id < 0) {
			SDL_Delay(10);
			continue;
		}

		dc_input_end_capture();

		if (id == 0)
			return;					/* Start: cancelled */

		/*
		 *	Take the button, releasing it from whoever had it. The scan is over
		 *	pad_work rather than the visible rows, so it reaches actions listed
		 *	on the other page. A button doing two things at once is never what
		 *	was meant.
		 */
		for (int i = 0; i < NUMBER_OF_KEYS; i++)
			if (i != action && pad_work[i] == id)
				pad_work[i] = 0;

		pad_work[action] = (int16)id;
		return;
	}
}

/* How many distinct buttons are spent, across both pages. */
static int buttons_spent(void)
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

/*
 *	One page. Returns the action to bind (1-based), or one of these.
 */
#define PAGE_BACK		0
#define PAGE_FLIP		-1
#define PAGE_DEFAULTS	-2

static int run_page(bool advanced, int *cursor)
{
	static const dc_ui_hint hints[] = {
		{ "A", "BIND",     true  },
		{ "X", "PAGE",     true  },
		{ "Y", "DEFAULTS", true  },
		{ "B", "BACK",     true  },
		{ "+", "MOVE",     false }
	};
	const int *acts = advanced ? page_adv : page_main;
	int n = advanced ? N_ADV : N_MAIN;
	struct dc_row rows[N_MAIN];
	char values[N_MAIN][16];
	char notes[N_MAIN][96];
	char spent[24];
	struct dc_screen sc;
	int i, chosen, used;

	memset(rows, 0, sizeof rows);
	memset(&sc, 0, sizeof sc);

	for (i = 0; i < n; i++) {
		int a = acts[i];

		snprintf(values[i], sizeof values[i], "%s",
		         pad_work[a] ? dc_input_button_name(pad_work[a]) : "-");
		snprintf(notes[i], sizeof notes[i],
		         "Press A, then press the button you want for %s.",
		         action_name[a]);

		rows[i].label = action_name[a];
		rows[i].kind  = DC_ROW_BIND;
		rows[i].id    = a + 1;			/* ids are 1-based */
		rows[i].note  = notes[i];
		rows[i].col2  = values[i];		/* the button, right-aligned */
	}

	used = buttons_spent();
	snprintf(spent, sizeof spent, "%d OF %d", used, ASSIGNABLE_BUTTONS);

	sc.title         = "CONFIGURE CONTROLLER";
	sc.kicker        = "20 ACTIONS";
	sc.cap           = advanced ? "ADVANCED" : "ACTIONS";
	sc.cap2          = "";
	sc.cap_right     = spent;
	sc.cap_right_hot = used >= ASSIGNABLE_BUTTONS;
	sc.panel_y       = 128;
	sc.panel_w       = 270;
	sc.row_h         = 26;
	sc.split         = advanced ? SPLIT_ADV : SPLIT_MAIN;
	sc.rows          = rows;
	sc.nrows         = n;
	sc.hints         = hints;
	sc.nhints        = 5;
	sc.explain       = true;
	sc.cursor        = *cursor;

	chosen = dc_screen_run(&sc);

	*cursor = sc.cursor;

	if (chosen == DC_SCREEN_BACK)
		return PAGE_BACK;

	if (chosen & DC_SCREEN_X)
		return PAGE_FLIP;

	if (chosen & DC_SCREEN_Y)
		return PAGE_DEFAULTS;

	return chosen;			/* action + 1 */
}

/*
 *	CONFIGURE CONTROLLER.
 *
 *	A binding is committed the moment it is made. There is no ACCEPT on this
 *	screen -- the design's hint bar offers BIND, PAGE, DEFAULTS, BACK and MOVE --
 *	so a change the player has made and can see on the row has to survive them
 *	backing out, or it would silently evaporate.
 */
void dc_pad_config(void)
{
	bool advanced = false;
	int cursor = 0;
	int i;

	for (i = 0; i < NUMBER_OF_KEYS; i++)
		pad_work[i] = input_preferences->dc_pad_bindings[i];

	for (;;) {
		int r = run_page(advanced, &cursor);

		if (r == PAGE_BACK)
			return;

		if (r == PAGE_FLIP) {
			advanced = !advanced;
			cursor = 0;
			continue;
		}

		if (r == PAGE_DEFAULTS)
			dc_input_default_bindings(pad_work, NUMBER_OF_KEYS);
		else if (r > 0)
			capture_for(r - 1);
		else
			continue;

		for (i = 0; i < NUMBER_OF_KEYS; i++)
			input_preferences->dc_pad_bindings[i] = pad_work[i];

		dc_apply_pad_bindings();
		write_preferences();
	}
}

#endif	/* DC */
