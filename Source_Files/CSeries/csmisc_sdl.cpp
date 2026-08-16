/*
 *  csmisc_sdl.cpp - Miscellaneous routines, SDL implementation
 *
 *  Written in 2000 by Christian Bauer
 */

#include "cseries.h"

#ifdef DC
extern "C" void dc_input_poll(void);
#endif


/*
 *  Return tick counter
 */

uint32 machine_tick_count(void)
{
	return SDL_GetTicks();
}


/*
 *  Wait for mouse click or keypress
 */

bool wait_for_click_or_keypress(uint32 ticks)
{
	uint32 start = SDL_GetTicks();
	SDL_Event event;
	while (SDL_GetTicks() - start < ticks) {
		event.type = SDL_NOEVENT;
#ifdef DC
		// Read the pad, so the ten-second chapter screen can be skipped from a
		// console. Proven on hardware in b48.
		dc_input_poll();
#endif
		SDL_PollEvent(&event);
		if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN)
			return true;
		SDL_Delay(10);
	}
	return false;
}
