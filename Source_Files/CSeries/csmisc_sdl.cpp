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
		// event is only written when SDL_PollEvent finds something, so clear it
		// first -- otherwise the first pass tests uninitialised stack and later
		// passes re-test the previous event.
		event.type = SDL_NOEVENT;
#ifdef DC
		// Read the pad.
		//
		// This loop only ever looked at SDL's event queue, and nothing puts a
		// Dreamcast controller into that queue except dc_input_poll(). So the
		// ten-second hold on the chapter screen could not be skipped from a
		// console at all, and every level load carried the whole delay --
		// measured at 22.5 seconds for the chapter screen, of which ten are
		// this wait. A player could see the screen and press every button on
		// the pad with no effect.
		dc_input_poll();
#endif
		SDL_PollEvent(&event);
		if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN)
			return true;
		SDL_Delay(10);
	}
	return false;
}
