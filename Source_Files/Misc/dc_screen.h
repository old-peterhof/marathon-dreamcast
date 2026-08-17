/*
 *	dc_screen.h -- one screen, described as data. See dc_screen.cpp.
 *
 *	Every screen in mockups/prototype is the same arrangement with different
 *	contents: a title and a kicker, a rule, a panel of rows, sometimes an
 *	explainer line or a state column, a rule, and a hint bar. So it is described
 *	once here and each screen is a table.
 */

#ifndef DC_SCREEN_H
#define DC_SCREEN_H

#ifdef DC

#include "dc_ui.h"

enum {
	DC_ROW_ACTION,		/* A activates it                                 */
	DC_ROW_SUBMENU,		/* A opens it; draws a chevron on the right       */
	DC_ROW_TOGGLE,		/* Left/Right flips it; opts[0] off, opts[1] on   */
	DC_ROW_SELECT,		/* Left/Right cycles opts[]                       */
	DC_ROW_SLIDER,		/* Left/Right adjusts; drawn as blocks            */
	DC_ROW_SAVE			/* name, elapsed and card, in three columns       */
};

#define DC_SCREEN_MAX_ROWS	24
#define DC_SCREEN_MAX_STATE	6

struct dc_row {
	const char *label;
	int kind;

	const char *note;		/* the explainer line, straight from the design */
	const char **opts;		/* TOGGLE and SELECT labels, NULL-terminated     */
	int max;				/* SLIDER: how many blocks                       */
	int value;				/* live; adjusted in place, read back by caller  */
	int id;					/* returned when the row is activated            */
	bool disabled;

	const char *col2;		/* SAVE: elapsed        */
	const char *col3;		/* SAVE: card and size  */
};

struct dc_screen {
	const char *title;
	const char *kicker;

	const char *cap;
	const char *cap_right;
	bool cap_right_hot;

	int title_y;			/* 60 on most screens, 96 on the pause menu */
	int panel_y;
	int panel_w;
	int row_h;

	struct dc_row *rows;
	int nrows;

	const struct dc_ui_hint *hints;
	int nhints;

	bool explain;			/* draw the focused row's note along the bottom */
	bool over_game;			/* dim the running game rather than draw a plate */

	const char *state[DC_SCREEN_MAX_STATE];	/* right-hand column, dim/bright */
	int nstate;

	const char *banner;		/* one amber line where the explainer would go   */

	int cursor;				/* live; where the highlight is, kept across runs */
};

/*
 *	Results. A row's own id is returned when it is activated, so ids must be
 *	positive and must not collide with these.
 */
#define DC_SCREEN_BACK		0		/* B, or Start */
#define DC_SCREEN_X			0x4000	/* ORed with the row id when X was pressed */

int dc_screen_run(struct dc_screen *sc);

#endif	/* DC */

#endif	/* DC_SCREEN_H */
