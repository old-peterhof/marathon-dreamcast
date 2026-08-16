/*
 *	dc_slots.cpp -- the four-slot save and load screens.
 *
 *	Replaces two dialogs that cannot work on a console. SAVE GAME asked for a
 *	typed filename, which is why every save this port has ever made is called
 *	"Untitled Game". CONTINUE SAVED GAME listed a directory, which the pad could
 *	not select from at all until the Return-in-list fix came back.
 *
 *	Four fixed slots answer both. The name is generated, so nothing has to be
 *	typed; the list is four rows, so nothing has to be scrolled; and every slot
 *	shows what is in it, which a filename never did.
 *
 *	These are built from buttons rather than w_list. Four rows is too few for a
 *	list widget to earn its keep, and it keeps the screens working even if the
 *	Return-in-list fix is ever lost again -- which has happened once already.
 *
 *	The card layer underneath is dc/dc_vmu.c. It hands over a dc_save_info_t per
 *	slot, read from a 128-byte header, so drawing this screen costs four small
 *	reads rather than four decompressions.
 */

#include "cseries.h"

// Include order matters here: screen_drawing.h needs shape_descriptors.h, and
// world.h ahead of both. This is the same order sdl_widgets.cpp uses.
#include "sdl_dialogs.h"
#include "sdl_fonts.h"
#include "sdl_widgets.h"
#include "shape_descriptors.h"
#include "screen_drawing.h"
#include "shell.h"
#include "world.h"
#include "screen.h"
#include "FileHandler.h"

#include <stdio.h>
#include <string.h>

#ifdef DC

#include "dc_vmu.h"
#include "dc_plate.h"

extern bool load_and_start_game(FileSpecifier& File);
extern "C" void dc_alert_text(const char *title, const char *l1, const char *l2);
extern void display_main_menu(void);

/*
 *	X is the destructive action and it must never be a player binding, so it
 *	lives in the fixed menu table in dc_input.c and arrives here as SDLK_DELETE.
 *	Nothing else in the dialog machinery looks at that key.
 */
#define SLOT_DELETE_KEY	SDLK_DELETE

/*
 *	Elapsed play time, from the tick count the header carries.
 *
 *	TICKS_PER_SECOND is 30 and is the engine's own tick, not a frame rate, so
 *	this is real elapsed game time and does not drift with how well the console
 *	is keeping up.
 */
static void format_elapsed(char *out, size_t len, unsigned int ticks)
{
	unsigned int secs = ticks / 30;
	unsigned int hours = secs / 3600;
	unsigned int mins = (secs % 3600) / 60;

	if (hours)
		snprintf(out, len, "%uh %02um", hours, mins);
	else
		snprintf(out, len, "%um", mins);
}

/*
 *	One row.
 *
 *	Draws the slot number, then what is in it: the level name if the save was
 *	written by a build that records one, the level number if it came from an
 *	older card, and the elapsed time on the right. An empty slot says so rather
 *	than being blank, because a blank row on a television reads as a rendering
 *	fault rather than as an offer.
 */
class w_save_slot : public widget {
public:
	w_save_slot(const dc_save_info_t &info, bool allow_delete, dialog *owner)
		: widget(ITEM_FONT), slot(info.slot), used(info.used != 0),
		  deletable(allow_delete), chosen(false), delete_asked(false), d(owner)
	{
		if (!used) {
			snprintf(line, sizeof line, "%d   - empty -", slot);
			right[0] = 0;
			return;
		}

		if (info.level_name[0])
			snprintf(line, sizeof line, "%d   %s", slot, info.level_name);
		else
			snprintf(line, sizeof line, "%d   Level %d", slot, info.level);

		format_elapsed(right, sizeof right, info.ticks);
	}

	int layout(void)
	{
		int w = text_width(line, font, style);
		int rw = text_width(right, font, style);

		// Fixed width so the four rows line up whatever is in them, and so the
		// elapsed column has somewhere constant to sit.
		rect.w = 400;
		rect.x = -rect.w / 2;
		rect.h = font->get_line_height() + 6;

		(void)w;
		(void)rw;

		return rect.h;
	}

	void draw(SDL_Surface *s) const
	{
		int y = rect.y + font->get_ascent() + 3;
		uint32 colour;

		if (active)
			colour = get_dialog_color(ITEM_ACTIVE_COLOR);
		else if (used)
			colour = get_dialog_color(ITEM_COLOR);
		else
			colour = get_dialog_color(LABEL_COLOR);

		// Selection reads three ways at once -- a bar, the colour, and a caret.
		// One cue is not enough at this frame rate on a composite television,
		// which is the finding UI-HANDOFF section 2 settled.
		if (active) {
			SDL_Rect bar = { rect.x, rect.y, rect.w, rect.h };
			SDL_FillRect(s, &bar, get_dialog_color(KEY_BINDING_COLOR));
		}

		draw_text(s, active ? ">" : " ", rect.x + 6, y, colour, font, style);
		draw_text(s, line, rect.x + 24, y, colour, font, style);

		if (right[0]) {
			int rw = text_width(right, font, style);
			draw_text(s, right, rect.x + rect.w - rw - 10, y, colour, font, style);
		}
	}

	/*
	 *	Acting on a row ends the screen. The widget needs the dialog to do that:
	 *	recording the choice and leaving the dialog running looks to the player
	 *	like the button did nothing, and leaves BACK as the only way out.
	 */
	void click(int, int)
	{
		chosen = true;
		if (d)
			d->quit(0);
	}

	void event(SDL_Event &e)
	{
		if (!deletable || !used)
			return;

		if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SLOT_DELETE_KEY) {
			delete_asked = true;
			e.type = SDL_NOEVENT;
			if (d)
				d->quit(0);
		}
	}

	bool was_chosen(void) const { return chosen; }
	bool wants_delete(void) const { return delete_asked; }
	void clear(void) { chosen = false; delete_asked = false; }

	int get_slot(void) const { return slot; }
	bool is_used(void) const { return used; }

private:
	int slot;
	bool used;
	bool deletable;
	bool chosen;
	bool delete_asked;

	dialog *d;

	char line[80];
	char right[16];
};

/*
 *	Both screens are the same four rows with a different caption and a different
 *	answer, so they share one runner.
 *
 *	Returns the slot the player acted on, 0 if they backed out. `deleted` is set
 *	when the action was a delete rather than a choice, so the caller knows to
 *	redraw rather than to proceed.
 */
static int run_slot_screen(const char *title, const char *help,
                           bool allow_delete, bool allow_empty, bool *deleted)
{
	dc_save_info_t info[DC_SAVE_SLOTS];
	w_save_slot *rows[DC_SAVE_SLOTS];
	int i, answer = 0;

	if (deleted)
		*deleted = false;

	dc_vmu_list_saves(info, DC_SAVE_SLOTS);

	dialog d;
	d.add(new w_static_text(title, TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());

	for (i = 0; i < DC_SAVE_SLOTS; i++) {
		rows[i] = new w_save_slot(info[i], allow_delete, &d);
		d.add(rows[i]);
	}

	d.add(new w_spacer());
	d.add(new w_static_text(help, LABEL_FONT, LABEL_COLOR));
	d.add(new w_spacer());
	d.add(new w_right_button("BACK", dialog_cancel, &d));

	dc_plate_select(DC_PLATE_PLAIN);
	clear_screen();

	if (d.run() != 0)
		return 0;			/* BACK, or Start */

	for (i = 0; i < DC_SAVE_SLOTS; i++) {
		if (rows[i]->wants_delete()) {
			answer = rows[i]->get_slot();
			if (deleted)
				*deleted = true;
			break;
		}

		if (rows[i]->was_chosen()) {
			/* An empty row is a valid target when saving and a no-op when
			   loading, so the screen does not have to make it unselectable and
			   the highlight can move over all four. */
			if (rows[i]->is_used() || allow_empty)
				answer = rows[i]->get_slot();
			break;
		}
	}

	return answer;
}

/*
 *	A yes/no the player has to travel to.
 *
 *	CANCEL is the first widget, so it is what the highlight starts on and what a
 *	reflexive press of A does. Deleting a save is the only irreversible thing in
 *	this interface and it should cost one deliberate movement.
 */
static bool confirm(const char *title, const char *line1, const char *line2)
{
	dialog d;

	d.add(new w_static_text(title, TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
	d.add(new w_static_text(line1));
	if (line2)
		d.add(new w_static_text(line2));
	d.add(new w_spacer());
	d.add(new w_left_button("CANCEL", dialog_cancel, &d));
	d.add(new w_right_button("YES", dialog_ok, &d));

	clear_screen();

	return d.run() == 0;
}

/*
 *	SAVE GAME. Called from save_game() in preprocess_map_sdl.cpp.
 *
 *	Every save shows this, empty slots included -- Max's call over the handoff's
 *	"only ask when the card is full". One more press each time buys the player a
 *	look at the whole card before they spend a slot, and makes overwriting always
 *	deliberate.
 *
 *	Returns the slot, or 0 if they backed out.
 */
int dc_choose_save_slot(void)
{
	int slot = run_slot_screen("SAVE GAME",
	                           "A saves here    X nothing    Start cancels",
	                           false, true, NULL);

	if (!slot)
		return 0;

	// Overwriting is confirmed; taking an empty slot is not, because there is
	// nothing to lose and a confirm on the common path is just a second press.
	{
		dc_save_info_t info[DC_SAVE_SLOTS];

		dc_vmu_list_saves(info, DC_SAVE_SLOTS);

		if (info[slot - 1].used) {
			char line[96];

			if (info[slot - 1].level_name[0])
				snprintf(line, sizeof line, "Slot %d holds %s.",
				         slot, info[slot - 1].level_name);
			else
				snprintf(line, sizeof line, "Slot %d holds a saved game.", slot);

			if (!confirm("OVERWRITE?", line, "This cannot be undone."))
				return 0;
		}
	}

	return slot;
}

/*
 *	MANAGE SAVES. A loads, X deletes, Start backs out.
 *
 *	Loading lives here as well as on Continue Game because Continue Game only
 *	ever opens the newest save. Without a load here the other three slots would
 *	be a history the player could see and never reach.
 */
void dc_manage_saves(void)
{
	while (true) {
		bool deleting = false;
		int slot = run_slot_screen("MANAGE SAVES",
		                           "A loads    X deletes    Start goes back",
		                           true, false, &deleting);

		if (!slot)
			return;

		if (deleting) {
			dc_save_info_t info[DC_SAVE_SLOTS];
			char line[96];

			dc_vmu_list_saves(info, DC_SAVE_SLOTS);

			if (info[slot - 1].level_name[0])
				snprintf(line, sizeof line, "Delete slot %d, %s?",
				         slot, info[slot - 1].level_name);
			else
				snprintf(line, sizeof line, "Delete slot %d?", slot);

			if (confirm("DELETE SAVE", line, "This cannot be undone."))
				dc_vmu_delete_save(slot);

			continue;	/* back to the list, which will have changed */
		}

		/* Load it. */
		{
			dc_save_info_t info[DC_SAVE_SLOTS];
			FileSpecifier file;
			char path[128];

			dc_vmu_list_saves(info, DC_SAVE_SLOTS);

			if (!info[slot - 1].used)
				continue;

			snprintf(path, sizeof path, "/ram/%s", info[slot - 1].ram_name);
			file.SetToSavedGamesDir();
			file = path;

			if (!load_and_start_game(file)) {
				dc_alert_text("COULD NOT LOAD",
				              "That saved game would not open.",
				              "It may have been written by another build.");
				continue;
			}

			return;		/* the game is running now */
		}
	}
}

/*
 *	Continue Game: open the newest save without asking anything.
 *
 *	Returns false if there is nothing to continue, which is what greys the item
 *	out on the main menu.
 */
bool dc_continue_newest_game(void)
{
	dc_save_info_t info[DC_SAVE_SLOTS];
	FileSpecifier file;
	char path[128];
	int slot = dc_vmu_newest_slot();

	if (!slot)
		return false;

	dc_vmu_list_saves(info, DC_SAVE_SLOTS);

	if (!info[slot - 1].used)
		return false;

	snprintf(path, sizeof path, "/ram/%s", info[slot - 1].ram_name);
	file.SetToSavedGamesDir();
	file = path;

	return load_and_start_game(file);
}

bool dc_have_any_save(void)
{
	return dc_vmu_newest_slot() != 0;
}

#endif	/* DC */
