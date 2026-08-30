/*
 *	dc_mainmenu.h -- the plate-drawn main menu. See dc_mainmenu.cpp.
 */

#ifndef DC_MAINMENU_H
#define DC_MAINMENU_H

#ifdef DC

void  dc_main_menu_draw(short selected);
void  dc_main_menu_flash(short item);
short dc_main_menu_step(short current, int delta);
short dc_main_menu_first(void);
short dc_main_menu_item(int index);
int   dc_main_menu_index_of(short item);
int   dc_main_menu_count(void);
bool  dc_main_menu_enabled(short item);

/* DIFFICULTY, shown before a new game. False if the player backed out. */
bool  dc_choose_difficulty(void);

#endif	/* DC */

#endif	/* DC_MAINMENU_H */
