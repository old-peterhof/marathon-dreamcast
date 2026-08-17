/*
 *  interface_sdl.cpp - Top-level game interface, SDL implementation
 *
 *  Written in 2000 by Christian Bauer
 */

#include "cseries.h"
#ifdef DC
#include "dc_mainmenu.h"
#include "dc_prefs.h"
#endif

#include "map.h"
#include "shell.h"
#include "interface.h"
#include "player.h"

#include "network.h"
#include "network_sound.h"

#include "screen_drawing.h"
#include "mysound.h"
#include "preferences.h"
#include "fades.h"
#include "game_window.h"
#include "game_errors.h"
#include "screen.h"

#include "images.h"

#include "interface_menus.h"


/*
 *  Set up and handle preferences menu
 */

void do_preferences(void)
{
	struct screen_mode_data mode = graphics_preferences->screen_mode;

#ifdef DC
	/*
	 *	The prototype's Preferences, not the stock dialogs. See dc_prefs.cpp.
	 *	handle_preferences() and everything under it stays compiled for other
	 *	platforms and is simply not reached here.
	 *
	 *	It also does NOT end by calling display_main_menu(), which the stock
	 *	handle_preferences() does unconditionally. That is why opening
	 *	Preferences from the pause menu threw the running level away: the state
	 *	became _display_main_menu, so Resume had nothing to resume, and loading a
	 *	save on top of that half-dismantled state locked the machine up.
	 *
	 *	Which means redrawing is this function's job now, and what to redraw
	 *	depends on where the player was.
	 */
	dc_preferences();

	if (get_game_state() == _display_main_menu)
		display_main_menu();
	else
		update_game_window();
#else
	handle_preferences();
#endif

	if (mode.bit_depth != graphics_preferences->screen_mode.bit_depth) {
		paint_window_black();
		initialize_screen(&graphics_preferences->screen_mode);

		/* Re fade in, so that we get the proper colortable loaded.. */
		display_main_menu();
	} else if (memcmp(&mode, &graphics_preferences->screen_mode, sizeof(struct screen_mode_data)))
		change_screen_mode(&graphics_preferences->screen_mode, false);
}


/*
 *  Toggle system hotkeys
 */

void toggle_menus(bool game_started)
{
	// nothing to do
}


/*
 *  Update game window
 */

void update_game_window(void)
{
	switch(get_game_state()) {
		case _game_in_progress:
			update_screen_window();
			break;
			
		case _display_quit_screens:
		case _display_intro_screens_for_demo:
		case _display_intro_screens:
		case _display_chapter_heading:
		case _display_prologue:
		case _display_epilogue:
		case _display_credits:
			update_interface_display();
			break;

		case _display_main_menu:
#ifdef DC
			/*
			 *	NOT update_interface_display(). That repaints
			 *	MAIN_MENU_BASE -- the original painted artwork, with its own
			 *	logo and its own black background -- straight over the drawn
			 *	menu. Anything causing a redraw here therefore replaced the new
			 *	interface with the old one, which is exactly what it looked
			 *	like: the plate and the panel gone, the stock logo back, and
			 *	the whole design apparently ignored.
			 *
			 *	display_main_menu() was guarded and this was not, and the two
			 *	have to agree about what the main menu is.
			 */
			{
				extern int last_menu;
				dc_main_menu_draw(last_menu);
			}
#else
			update_interface_display();
#endif
			break;
			
		default:
			break;
	}
}


/*
 *  Network microphone handling
 */

void network_speaker_idle_proc(void)
{
	// nothing to do
}

void install_network_microphone(void)
{
	// nothing to do
}

void remove_network_microphone(void)
{
	// nothing to do
}


/*
 *  Exit networking
 */

void exit_networking(void)
{
	NetExit();
}


/*
 *  Show movie
 */

void show_movie(short index)
{
	// unused by official scenarios
}
