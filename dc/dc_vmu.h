/*
 *	dc_vmu.h -- saved games on the memory card.
 *
 *	Until now every consumer re-declared these extern "C" at its own call site,
 *	which was survivable while they were four void functions taking strings. The
 *	four-slot interface needs a struct, so it needs a header.
 *
 *	THE SLOT MODEL. The player sees four slots. The card has eight, and slots are
 *	addressed on the card by number, but the two are not the same thing: a card
 *	written before this change may hold games in any of its eight, and moving
 *	them would mean rewriting data this code has no business rewriting. So the
 *	player's slot number is an index into a table built at start-up, and each
 *	entry remembers which card and which card slot it actually lives in. Nothing
 *	is ever moved to make the numbering tidy.
 */

#ifndef DC_VMU_H
#define DC_VMU_H

#ifdef __cplusplus
extern "C" {
#endif

/* What the player sees. The card is scanned deeper than this; see dc_vmu.c. */
#define DC_SAVE_SLOTS		4

#define DC_SAVE_NAME_LEN	48
#define DC_SAVE_LVLNAME_LEN	42

typedef struct {
	int          used;			/* 0 for an empty slot; everything else is then undefined */
	int          slot;			/* 1..DC_SAVE_SLOTS, what the player calls it */

	char         ram_name[DC_SAVE_NAME_LEN];		/* filename under /ram */
	char         level_name[DC_SAVE_LVLNAME_LEN];	/* empty for a card written before v2 */

	int          level;			/* level number, always present */
	unsigned int save_time;		/* RTC seconds; 0 if unknown */
	unsigned int save_seq;		/* monotonic; 0 if unknown */
	unsigned int ticks;			/* elapsed play time, 30 per second; 0 if unknown */
	int          difficulty;	/* -1 if unknown */
	int          blocks;		/* what it costs on the card */
} dc_save_info_t;

/* Restore every save on the card into the ramdisk, and build the slot table.
   Called once at start-up, before Aleph One looks at saved_games_dir. */
void dc_vmu_load_saves(const char *ram_dir, const char *map_path);

/* Mirror one freshly written save to the card. Pass target_slot 0 to keep the
   old behaviour of matching by name. */
void dc_vmu_save_game(const char *ram_path, const char *map_path, int level,
                      const char *level_name, unsigned int ticks,
                      int difficulty, int target_slot);

/* Fill `out` with DC_SAVE_SLOTS entries, occupied or not. Returns how many are
   occupied. Cheap -- one header read per slot, nothing is decompressed. */
int dc_vmu_list_saves(dc_save_info_t *out, int max);

/* Delete a player slot from the card and from /ram. Returns non-zero on
   success. Confirm before calling: there is no undo. */
int dc_vmu_delete_save(int slot);

/* The slot the player most recently saved into, or 0 if there are none. This is
   what Continue Game opens. */
int dc_vmu_newest_slot(void);

/* The /ram path a given slot should be written to, so the save dialog and the
   card layer agree on a name without either inventing one. */
const char *dc_vmu_slot_ram_name(int slot);

/*
 *	Which slot the next save goes to.
 *
 *	save_game_file() is deep in the engine and has no idea slots exist, so the
 *	save screen leaves the answer here on its way past and the card layer picks
 *	it up. Taking it clears it, so a save reached by any other route -- the
 *	_tag_save cheat, say -- falls back to matching by name rather than silently
 *	landing in whichever slot was chosen last.
 */
void dc_vmu_set_target_slot(int slot);
int  dc_vmu_take_target_slot(void);

void dc_vmu_load_prefs(const char *ram_path);
void dc_vmu_save_prefs(const char *ram_path);

#ifdef __cplusplus
}
#endif

#endif	/* DC_VMU_H */
