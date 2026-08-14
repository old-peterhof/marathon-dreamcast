/*
	dc_input.cpp

	Dreamcast controller support.

	Aleph One 0.12.0 has no joystick code whatsoever -- not disabled, absent --
	and BERO's port never added any, which is why his build needed a keyboard.

	Rather than teach the engine about joysticks, this uses the SDL Dreamcast
	driver's keyboard-emulation mode: it translates pad buttons into SDL key
	events underneath SDL_PollEvent. Everything upstream -- menus, dialogs, the
	in-game key bindings -- then works unmodified, because as far as Aleph One
	can tell somebody is typing.

	That single property is what makes the main menu driveable, which the
	engine's own dialog code would otherwise never allow from a pad.

	The matching default bindings live in key_definitions.h under #ifdef DC.
	Change one and you must change the other.
*/

#ifdef DC

#include "cseries.h"
#include "dc_input.h"

#include <SDL.h>
#include <SDL_dreamcast.h>

static SDL_Joystick *dc_joystick = NULL;

/*
	Video setup. Must run before the first SDL_SetVideoMode, i.e. before
	change_screen_mode(), which is why it is called from
	initialize_application() right after SDL_Init rather than from
	screen_sdl.cpp.

	By default the SDL DC driver puts up a "Press Y for 60Hz" prompt and waits.
	Marathon runs at 30 ticks/second and the console is NTSC, so 60Hz is simply
	the right answer -- there is nothing for the player to decide, and the
	prompt just blocks startup on a question with one correct response.
*/
void DC_InitVideo(void)
{
	SDL_DC_Default60Hz(SDL_TRUE);
	SDL_DC_ShowAskHz(SDL_FALSE);
}

/*
	Control scheme
	--------------
	  D-pad up/down     forward / backward      (menu: move selection)
	  D-pad left/right  turn left / right
	  X (hold)          sidestep instead of turn
	  Y (hold)          run instead of walk
	  A                 action -- doors, switches, terminals (menu: select)
	  B                 next weapon
	  L trigger         secondary fire
	  R trigger         primary fire
	  Start             escape / abort

	A is deliberately RETURN rather than TAB: BERO's menu navigation in
	interface.cpp keys off SDLK_RETURN, so binding action to the same key lets
	one button mean "yes" in both contexts instead of needing two.

	Sidestep is a modifier, not its own direction, because a Dreamcast pad has
	four face buttons and Marathon wants more inputs than that. _sidestep_dont_turn
	is a first-class Marathon action built for exactly this trade.

	Not currently bound, for want of buttons: the overhead map, look up/down,
	weapon cycling backwards, and the microphone.
*/

void DC_InitInput(void)
{
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
		fprintf(stderr, "DC_InitInput: joystick subsystem failed: %s\n", SDL_GetError());
		return;
	}

	if (SDL_NumJoysticks() < 1) {
		fprintf(stderr, "DC_InitInput: no controller in port A\n");
		return;
	}

	/* The driver only polls the pad for joysticks that are open, so this has
	   to happen even though we never read the joystick API directly. */
	dc_joystick = SDL_JoystickOpen(0);
	if (dc_joystick == NULL) {
		fprintf(stderr, "DC_InitInput: SDL_JoystickOpen failed: %s\n", SDL_GetError());
		return;
	}

	/* Movement and menu navigation share the d-pad: the DC key definitions
	   bind forward/backward/turn to the arrow keys precisely so that these
	   four mappings serve gameplay and menus at once. */
	SDL_DC_MapKey(0, SDL_DC_UP,    SDLK_UP);
	SDL_DC_MapKey(0, SDL_DC_DOWN,  SDLK_DOWN);
	SDL_DC_MapKey(0, SDL_DC_LEFT,  SDLK_LEFT);
	SDL_DC_MapKey(0, SDL_DC_RIGHT, SDLK_RIGHT);

	SDL_DC_MapKey(0, SDL_DC_A,     SDLK_RETURN);	/* action / confirm */
	SDL_DC_MapKey(0, SDL_DC_B,     SDLK_KP9);		/* next weapon      */
	SDL_DC_MapKey(0, SDL_DC_X,     SDLK_LSHIFT);	/* sidestep modifier */
	SDL_DC_MapKey(0, SDL_DC_Y,     SDLK_LCTRL);		/* run modifier     */

	SDL_DC_MapKey(0, SDL_DC_L,     SDLK_LALT);		/* secondary fire   */
	SDL_DC_MapKey(0, SDL_DC_R,     SDLK_SPACE);		/* primary fire     */

	SDL_DC_MapKey(0, SDL_DC_START, SDLK_ESCAPE);

	/* Mouse emulation stays off. The driver raises a mouse-button event
	   alongside the key event for every face button when it is on, which
	   causes phantom clicks in dialogs. Revisit for analog aiming, which
	   mouse_idle() would handle well -- it already maps X to yaw, Y to pitch. */
	SDL_DC_EmulateMouse(SDL_FALSE);
	SDL_DC_EmulateKeyboard(SDL_TRUE);
}

void DC_ShutdownInput(void)
{
	if (dc_joystick) {
		SDL_JoystickClose(dc_joystick);
		dc_joystick = NULL;
	}
}

#endif /* DC */
