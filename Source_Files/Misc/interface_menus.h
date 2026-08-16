#ifndef __INTERFACE_MENUS_H
#define __INTERFACE_MENUS_H

/*

	interface_menus.h
	Thursday, September 28, 1995 7:02:23 PM- rdm created.

*/

enum { /* Menus available during the game */
	mGame= 128,
	iPause= 1,
	iSave,
	iRevert,
	iCloseGame,
	iQuitGame
};

enum { /* Menu interface... */
	mInterface= 129,
	iNewGame= 1,
	iLoadGame,
	iGatherGame,
	iJoinGame,
	iPreferences,
	iReplayLastFilm,
	iSaveLastFilm,
	iReplaySavedFilm,
	iCredits,
	iQuit,
	iCenterButton,
	/* Appended deliberately. Every id above keeps its value, so the identity
	   rect = item - 1 + _new_game_button_rect still holds for the mouse path
	   and for non-DC builds, and nothing had to be renumbered to add an item.
	   The Dreamcast menu is drawn rather than painted and has no rectangle. */
	iManageSaves
};

/* This is the menu with nothing in the title, so that it doesn't show up */
/* when the menu bar is drawn atexit.. */
enum {
	mFakeEmptyMenu= 130
};

#endif

