/*
 *	dc_mainmenu.cpp -- the main menu, drawn rather than painted.
 *
 *	The stock menu is two full-screen PICTs and a table of eighteen hardcoded
 *	rectangles. Highlighting an item means clipping to its rectangle and
 *	re-blitting the second picture, in which every button is lit. The buttons are
 *	part of the artwork, so changing which items exist means opening a paint
 *	program, and four separate files have to agree on their order -- the
 *	mInterface enum, the rect enum, the rect table, and the walk order in
 *	shell_sdl.cpp -- through the identity rect = item - 1 + _new_game_button_rect.
 *
 *	UI-HANDOFF.md section 5.1 replaces all of that with a static plate and text
 *	over it. Which means the item list is now just an array in this file, greying
 *	an item out is a colour rather than a third bitmap, and none of the rectangle
 *	machinery is involved at all.
 *
 *	The old path is untouched and still compiled: nothing was deleted from the
 *	enum or the rect table, so every dead menu arm and every non-DC build keeps
 *	working exactly as before. This simply draws something else on Dreamcast.
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
#include "interface.h"
#include "interface_menus.h"
#include "preferences.h"

#include <stdio.h>
#include <string.h>

#ifdef DC

#include "dc_plate.h"
#include "dc_slots.h"
#include "dc_vmu.h"

extern "C" void dc_trace(int slot, const char *fmt, ...);

/*
 *	The five items, in the order the D-pad walks them.
 *
 *	iLoadGame keeps its name and becomes Continue Game. Reusing the existing id
 *	rather than minting a new one means do_menu_item_command already has an arm
 *	for it and nothing else in the engine has to learn a new number.
 */
struct dc_menu_item {
	short item;
	const char *label;
};

static const struct dc_menu_item menu_items[] = {
	{ iNewGame,      "NEW GAME"     },
	{ iLoadGame,     "CONTINUE GAME" },
	{ iManageSaves,  "MANAGE SAVES" },
	{ iPreferences,  "PREFERENCES"  },
	{ iCredits,      "CREDITS"      },
};

#define NUM_MENU_ITEMS	(int)(sizeof(menu_items) / sizeof(menu_items[0]))

/*
 *	Geometry, from UI-HANDOFF section 2: 640x480 with 40px clear on every edge,
 *	so nothing readable leaves the 560x400 centre. The wordmark is baked into the
 *	plate at (40,46) and is 102 tall, so the rows start below it.
 */
#define MENU_X		76		/* the caret sits 20px left of this, inside the margin */
#define MENU_TOP	212
#define MENU_ROW	34
#define MENU_W		300

/*
 *	The last-save note is right-aligned to the safe margin rather than placed at
 *	a fixed left edge. Level names run to 66 characters, and anything
 *	left-aligned in the space that is left runs off the side of a television --
 *	which is invisible in an emulator and obvious on Max's set, the failure this
 *	whole layout is written around.
 */
#define SAFE_L		40
#define SAFE_R		600

int dc_main_menu_count(void)
{
	return NUM_MENU_ITEMS;
}

short dc_main_menu_item(int index)
{
	if (index < 0 || index >= NUM_MENU_ITEMS)
		return NONE;

	return menu_items[index].item;
}

int dc_main_menu_index_of(short item)
{
	int i;

	for (i = 0; i < NUM_MENU_ITEMS; i++)
		if (menu_items[i].item == item)
			return i;

	return -1;
}

/*
 *	Continue Game is the only item that can be unavailable, and it is unavailable
 *	exactly when there is nothing to continue.
 */
bool dc_main_menu_enabled(short item)
{
	if (item == iLoadGame)
		return dc_have_any_save();

	return true;
}

/* What Continue Game would open, drawn beside it so the choice is informed. */
static void newest_save_note(char *out, size_t len)
{
	dc_save_info_t info[DC_SAVE_SLOTS];
	int slot = dc_vmu_newest_slot();

	out[0] = 0;

	if (!slot)
		return;

	dc_vmu_list_saves(info, DC_SAVE_SLOTS);

	if (!info[slot - 1].used)
		return;

	if (info[slot - 1].level_name[0])
		snprintf(out, len, "%s", info[slot - 1].level_name);
	else
		snprintf(out, len, "Level %d", info[slot - 1].level);
}

/*
 *	Draw the whole menu.
 *
 *	Everything is repainted each time rather than only the row that changed. The
 *	stock code went to some trouble to repaint one rectangle because each repaint
 *	decoded a 640x480 PICT; here it is one blit from a surface already in the
 *	display's own format, so the simple thing is also the fast thing.
 */
void dc_main_menu_draw(short selected)
{
	SDL_Surface *video = SDL_GetVideoSurface();
	const sdl_font_info *font;
	uint16 style;
	char note[64];
	int i, line_h;

	if (!video)
		return;

	dc_plate_select(DC_PLATE_MAIN);
	dc_plate_to_screen();

	font = get_dialog_font(ITEM_FONT, style);
	if (!font)
		return;

	line_h = font->get_line_height();

	newest_save_note(note, sizeof note);

	for (i = 0; i < NUM_MENU_ITEMS; i++) {
		int y = MENU_TOP + i * MENU_ROW;
		bool on = menu_items[i].item == selected;
		bool live = dc_main_menu_enabled(menu_items[i].item);
		uint32 colour;

		if (!live)
			colour = get_dialog_color(LABEL_COLOR);
		else if (on)
			colour = get_dialog_color(ITEM_ACTIVE_COLOR);
		else
			colour = get_dialog_color(ITEM_COLOR);

		/*
		 *	Three cues for selection at once -- a bar, the colour, and a caret.
		 *	One is not enough at this frame rate on a composite television, which
		 *	is what UI-HANDOFF section 2 settled after measuring it.
		 */
		if (on) {
			SDL_Rect bar;

			bar.x = MENU_X - 32;
			bar.y = y - 4;
			bar.w = MENU_W;
			bar.h = line_h + 8;

			SDL_FillRect(video, &bar, get_dialog_color(KEY_BINDING_COLOR));

			draw_text(video, ">", MENU_X - 20, y + font->get_ascent(),
			          colour, font, style);
		}

		draw_text(video, menu_items[i].label, MENU_X, y + font->get_ascent(),
		          colour, font, style);

		/*
		 *	The last-save column, beside the item it describes. Right-aligned to
		 *	the safe margin, and dropped entirely if it would reach back into the
		 *	label -- a name colliding with "CONTINUE GAME" reads as corruption,
		 *	where its absence reads as nothing to say.
		 */
		if (menu_items[i].item == iLoadGame && note[0]) {
			int nw = text_width(note, font, style);
			int nx = SAFE_R - nw;
			int label_end = MENU_X + text_width(menu_items[i].label, font, style);

			if (nx > label_end + 20)
				draw_text(video, note, nx, y + font->get_ascent(),
				          get_dialog_color(LABEL_COLOR), font, style);
		}
	}

	SDL_UpdateRect(video, 0, 0, 0, 0);
}

/*
 *	Move the highlight, skipping anything unavailable.
 *
 *	The stock walk never consulted enabled_item(), so it would happily land on a
 *	dead row and let Return be pressed on it. With Continue Game greyed out on a
 *	fresh card that would be the second item in the list, so skipping is not a
 *	refinement here, it is the difference between the menu working and not.
 *
 *	The guard counts steps rather than trusting the list: if every item were
 *	somehow disabled this would otherwise spin forever.
 */
short dc_main_menu_step(short current, int delta)
{
	int i = dc_main_menu_index_of(current);
	int tries;

	if (i < 0)
		i = 0;

	for (tries = 0; tries < NUM_MENU_ITEMS; tries++) {
		i += delta;

		if (i < 0)
			i = NUM_MENU_ITEMS - 1;
		else if (i >= NUM_MENU_ITEMS)
			i = 0;

		if (dc_main_menu_enabled(menu_items[i].item))
			return menu_items[i].item;
	}

	return current;
}

/* The first item the highlight may rest on, for when the menu is first shown. */
short dc_main_menu_first(void)
{
	int i;

	for (i = 0; i < NUM_MENU_ITEMS; i++)
		if (dc_main_menu_enabled(menu_items[i].item))
			return menu_items[i].item;

	return iNewGame;
}

/*
 *	DIFFICULTY, shown when a new game starts.
 *
 *	Bungie's five names, nothing added. It writes player_preferences directly
 *	because begin_game() reads the preference at the point it builds the game --
 *	so nothing else needs a new parameter, and a game loaded from a save picks up
 *	the same value the same way.
 *
 *	Handoff section 4 moves this out of Preferences on the grounds that it
 *	belongs to a run rather than to the machine. Max settled the consequence: it
 *	stays a New Game choice and does not appear on the pause menu, so a run's
 *	difficulty is fixed when it starts.
 *
 *	Returns false if the player backed out, in which case no game begins.
 */

class w_difficulty : public widget {
public:
	w_difficulty(const char *name, int value, dialog *owner)
		: widget(ITEM_FONT), label(name), level(value), picked(false), d(owner) {}

	int layout(void)
	{
		rect.w = 300;
		rect.x = -rect.w / 2;
		rect.h = font->get_line_height() + 6;

		return rect.h;
	}

	void draw(SDL_Surface *s) const
	{
		int y = rect.y + font->get_ascent() + 3;
		uint32 colour = active ? get_dialog_color(ITEM_ACTIVE_COLOR)
		                       : get_dialog_color(ITEM_COLOR);

		if (active) {
			SDL_Rect bar = { rect.x, rect.y, rect.w, rect.h };
			SDL_FillRect(s, &bar, get_dialog_color(KEY_BINDING_COLOR));
			draw_text(s, ">", rect.x + 6, y, colour, font, style);
		}

		draw_text(s, label, rect.x + 24, y, colour, font, style);
	}

	void click(int, int)
	{
		picked = true;
		if (d)
			d->quit(0);
	}

	bool was_picked(void) const { return picked; }
	int get_level(void) const { return level; }

private:
	const char *label;
	int level;
	bool picked;
	dialog *d;
};

bool dc_choose_difficulty(void)
{
	static const char *names[] = {
		"KINDERGARTEN", "EASY", "NORMAL", "MAJOR DAMAGE", "TOTAL CARNAGE"
	};
	const int count = (int)(sizeof(names) / sizeof(names[0]));
	w_difficulty *rows[5];
	int i;

	dialog d;
	d.add(new w_static_text("DIFFICULTY", TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());

	for (i = 0; i < count; i++) {
		rows[i] = new w_difficulty(names[i], i, &d);
		d.add(rows[i]);
	}

	d.add(new w_spacer());
	d.add(new w_static_text("A begins    Start goes back", LABEL_FONT, LABEL_COLOR));
	d.add(new w_spacer());
	d.add(new w_right_button("BACK", dialog_cancel, &d));

	dc_plate_select(DC_PLATE_PLAIN);
	clear_screen();

	if (d.run() != 0)
		return false;

	for (i = 0; i < count; i++) {
		if (rows[i]->was_picked()) {
			if (player_preferences->difficulty_level != rows[i]->get_level()) {
				player_preferences->difficulty_level = rows[i]->get_level();
				write_preferences();
			}

			return true;
		}
	}

	return false;
}

/*
 *	The press flash, which the stock menu did by drawing the lit picture for a
 *	twelfth of a second. Same idea, one repaint.
 */
void dc_main_menu_flash(short item)
{
	dc_main_menu_draw(item);
	SDL_Delay(1000 / 12);
}

#endif	/* DC */
