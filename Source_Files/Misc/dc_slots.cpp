/*
 *	dc_slots.cpp -- the four-slot save and load screens.
 *
 *	Replaces two dialogs that cannot work on a console. SAVE GAME asked for a
 *	typed filename, which is why every save this port made before b59 was called
 *	"Untitled Game". CONTINUE SAVED GAME listed a directory, which a pad could
 *	not select from at all.
 *
 *	Drawn by dc_screen, so these look like every other screen and have the same
 *	way out. A save row is three facts -- what the level was, how long the run
 *	is, and what it costs on the card -- which is why it has three columns rather
 *	than a label and a value.
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
#include "FileHandler.h"

#include <stdio.h>
#include <string.h>

#ifdef DC

#include "dc_vmu.h"
#include "dc_plate.h"
#include "dc_screen.h"
#include "dc_slots.h"

extern bool load_and_start_game(FileSpecifier& File);
extern "C" void dc_alert_text(const char *title, const char *l1, const char *l2);

/*
 *	Elapsed play time, from the tick count the card header carries.
 *
 *	TICKS_PER_SECOND is 30 and is the engine's own tick, not a frame rate, so
 *	this is real elapsed game time and does not drift with how well the console
 *	is keeping up. The design shows it as hours:minutes:seconds.
 */
static void format_elapsed(char *out, size_t len, unsigned int ticks)
{
	unsigned int secs = ticks / 30;

	snprintf(out, len, "%02u:%02u:%02u",
	         secs / 3600, (secs % 3600) / 60, secs % 60);
}

/* Which card, how many slots, how much of it -- the kicker line. */
static void card_summary(char *out, size_t len)
{
	dc_save_info_t info[DC_SAVE_SLOTS];
	int i, used = 0, blocks = 0;
	const char *unit = "";

	dc_vmu_list_saves(info, DC_SAVE_SLOTS);

	for (i = 0; i < DC_SAVE_SLOTS; i++) {
		if (!info[i].used)
			continue;

		used++;
		blocks += info[i].blocks;

		if (!unit[0])
			unit = dc_vmu_slot_unit(i + 1);
	}

	if (!used)
		snprintf(out, len, "NO SAVED GAMES");
	else
		snprintf(out, len, "VMU %s  %d OF 4 USED  %d BLK", unit, used, blocks);
}

/*
 *	Fill a screen's rows from the card.
 *
 *	The strings live in the caller's buffers because dc_row only borrows
 *	pointers, and the screen redraws from them for as long as it runs.
 */
static int fill_slot_rows(struct dc_row *rows,
                          char names[DC_SAVE_SLOTS][48],
                          char times[DC_SAVE_SLOTS][16],
                          char cards[DC_SAVE_SLOTS][24],
                          bool empties_selectable)
{
	dc_save_info_t info[DC_SAVE_SLOTS];
	int i, used;

	used = dc_vmu_list_saves(info, DC_SAVE_SLOTS);

	for (i = 0; i < DC_SAVE_SLOTS; i++) {
		memset(&rows[i], 0, sizeof rows[i]);

		rows[i].kind = DC_ROW_SAVE;
		rows[i].id = i + 1;
		rows[i].label = names[i];
		rows[i].col2 = times[i];
		rows[i].col3 = cards[i];

		if (!info[i].used) {
			snprintf(names[i], 48, "EMPTY SLOT");
			times[i][0] = 0;
			cards[i][0] = 0;

			/* Dimmed either way. Whether it can be chosen is what differs
			   between saving into one and loading from one. */
			rows[i].disabled = !empties_selectable;
			continue;
		}

		if (info[i].level_name[0])
			snprintf(names[i], 48, "%s", info[i].level_name);
		else
			snprintf(names[i], 48, "LEVEL %d", info[i].level);

		format_elapsed(times[i], 16, info[i].ticks);
		snprintf(cards[i], 24, "%s %d BLK", dc_vmu_slot_unit(i + 1),
		         info[i].blocks);
	}

	return used;
}

/*
 *	A yes/no the player has to travel to.
 *
 *	"Leave it" is the first row, so it is where the highlight starts and what a
 *	reflexive press of A does. Deleting a save is the only irreversible thing in
 *	this interface and it should cost one deliberate movement.
 */
static bool confirm(const char *title, const char *question)
{
	static const dc_ui_hint hints[] = {
		{ "A", "CHOOSE", true },
		{ "B", "BACK",   true }
	};
	struct dc_row rows[2];
	struct dc_screen sc;

	memset(rows, 0, sizeof rows);
	memset(&sc, 0, sizeof sc);

	rows[0].label = "NO, LEAVE IT ALONE";
	rows[0].kind = DC_ROW_ACTION;
	rows[0].id = 1;

	rows[1].label = "YES, DELETE IT";
	rows[1].kind = DC_ROW_ACTION;
	rows[1].id = 2;

	sc.title   = title;
	sc.kicker  = "THIS CANNOT BE UNDONE";
	sc.cap     = question;
	sc.panel_y = 190;
	sc.panel_w = 560;
	sc.row_h   = DC_UI_ROW_H;
	sc.rows    = rows;
	sc.nrows   = 2;
	sc.hints   = hints;
	sc.nhints  = 2;

	return dc_screen_run(&sc) == 2;
}

/*
 *	SAVE GAME, from a save terminal or the pause menu.
 *
 *	Every save shows this, empty slots included -- Max's call over the handoff's
 *	"only ask when the card is full". One more press buys a look at the whole
 *	card before a slot is spent, and makes overwriting always deliberate.
 */
int dc_choose_save_slot(void)
{
	static const dc_ui_hint hints[] = {
		{ "A", "SAVE HERE", true  },
		{ "B", "CANCEL",    true  },
		{ "+", "MOVE",      false }
	};
	char names[DC_SAVE_SLOTS][48], times[DC_SAVE_SLOTS][16];
	char cards[DC_SAVE_SLOTS][24], kicker[64];
	struct dc_row rows[DC_SAVE_SLOTS];
	struct dc_screen sc;
	int used, slot;

	used = fill_slot_rows(rows, names, times, cards, true);
	card_summary(kicker, sizeof kicker);

	memset(&sc, 0, sizeof sc);
	sc.title     = "SAVE GAME";
	sc.kicker    = kicker;
	sc.cap       = "SAVED GAMES";
	sc.cap_right = "4 SLOTS";
	sc.panel_y   = 150;
	sc.panel_w   = 560;
	sc.row_h     = DC_UI_ROW_H;
	sc.rows      = rows;
	sc.nrows     = DC_SAVE_SLOTS;
	sc.hints     = hints;
	sc.nhints    = 3;

	if (used >= DC_SAVE_SLOTS)
		sc.banner = "ALL FOUR SLOTS FULL - CHOOSE ONE TO OVERWRITE";

	slot = dc_screen_run(&sc);
	slot &= ~DC_SCREEN_X;		/* X does nothing here */

	if (slot < 1 || slot > DC_SAVE_SLOTS)
		return 0;

	/* Overwriting is confirmed; taking an empty slot is not, because there is
	   nothing to lose and a confirm on the common path is just a second press. */
	{
		dc_save_info_t info[DC_SAVE_SLOTS];
		char q[96];

		dc_vmu_list_saves(info, DC_SAVE_SLOTS);

		if (!info[slot - 1].used)
			return slot;

		snprintf(q, sizeof q, "OVERWRITE %s?", names[slot - 1]);

		if (!confirm("SAVE GAME", q))
			return 0;
	}

	return slot;
}

/*
 *	MANAGE SAVES. A loads, X deletes, B goes back.
 *
 *	Loading lives here as well as on Continue Game because Continue Game only
 *	ever opens the newest save. Without a load here the other three slots would
 *	be a history the player could see and never reach.
 */
void dc_manage_saves(void)
{
	static const dc_ui_hint hints[] = {
		{ "A", "LOAD",   true },
		{ "X", "DELETE", true },
		{ "B", "BACK",   true }
	};
	char names[DC_SAVE_SLOTS][48], times[DC_SAVE_SLOTS][16];
	char cards[DC_SAVE_SLOTS][24], kicker[64];
	struct dc_row rows[DC_SAVE_SLOTS];
	struct dc_screen sc;

	memset(&sc, 0, sizeof sc);

	for (;;) {
		int chosen, slot;

		fill_slot_rows(rows, names, times, cards, false);
		card_summary(kicker, sizeof kicker);

		sc.title     = "MANAGE SAVES";
		sc.kicker    = kicker;
		sc.cap       = "SAVED GAMES";
		sc.cap_right = "4 SLOTS";
		sc.panel_y   = 150;
		sc.panel_w   = 560;
		sc.row_h     = DC_UI_ROW_H;
		sc.rows      = rows;
		sc.nrows     = DC_SAVE_SLOTS;
		sc.hints     = hints;
		sc.nhints    = 3;

		chosen = dc_screen_run(&sc);

		if (chosen == DC_SCREEN_BACK)
			return;

		if (chosen & DC_SCREEN_X) {
			char q[96];

			slot = chosen & ~DC_SCREEN_X;

			if (slot < 1 || slot > DC_SAVE_SLOTS)
				continue;

			snprintf(q, sizeof q, "DELETE %s?", names[slot - 1]);

			if (confirm("MANAGE SAVES", q))
				dc_vmu_delete_save(slot);

			continue;	/* back to the list, which has changed */
		}

		slot = chosen;

		if (slot < 1 || slot > DC_SAVE_SLOTS)
			continue;

		/* Load it. */
		{
			dc_save_info_t info[DC_SAVE_SLOTS];
			FileSpecifier file;
			char path[128];

			dc_vmu_list_saves(info, DC_SAVE_SLOTS);

			if (!info[slot - 1].used)
				continue;

			/* Absolute already: saved_games_dir is "/ram" on this port, because
			   the KOS ramdisk is flat and cannot hold a subdirectory. */
			snprintf(path, sizeof path, "/ram/%s", info[slot - 1].ram_name);
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
	file = path;

	return load_and_start_game(file);
}

bool dc_have_any_save(void)
{
	return dc_vmu_newest_slot() != 0;
}

#endif	/* DC */
