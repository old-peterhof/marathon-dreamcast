/*
 *	dc_slots.h -- the four-slot save and load screens. See dc_slots.cpp.
 */

#ifndef DC_SLOTS_H
#define DC_SLOTS_H

#ifdef DC

/* SAVE GAME. Returns the chosen slot, or 0 if the player backed out. */
int dc_choose_save_slot(void);

/* MANAGE SAVES: A loads, X deletes. Returns once a game is running or the
   player backs out. */
void dc_manage_saves(void);

/* Continue Game. False if there is nothing to continue. */
bool dc_continue_newest_game(void);

/* Whether any slot is occupied -- what greys Continue Game out. */
bool dc_have_any_save(void);

#endif	/* DC */

#endif	/* DC_SLOTS_H */
