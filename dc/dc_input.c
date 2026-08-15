/*
 *	dc_input.c -- Dreamcast controller support for Aleph One.
 *
 *	SDL 1.2's Dreamcast video driver polls only MAPLE_FUNC_MOUSE and
 *	MAPLE_FUNC_KEYBOARD (see SDL_dcevents.c), and its joystick backend is the
 *	"dummy" one. MAPLE_FUNC_CONTROLLER is never read at all, which is why BERO's
 *	2002 README says a keyboard is required to play.
 *
 *	So we read the controller ourselves and feed the result into SDL as key
 *	events. The injection goes through SDL_PrivateKeyboard rather than
 *	SDL_PushEvent, and that choice matters: SDL_PrivateKeyboard updates SDL's
 *	internal key-state array as well as queueing the event, and Aleph One reads
 *	gameplay input by polling SDL_GetKeyState (vbl_sdl.cpp:98). A pushed event
 *	alone would drive the menus but leave the player standing still.
 *
 *	The mapping targets the arrow-key layout, which is the Dreamcast default
 *	(see preferences.cpp), so the D-pad drives both menu navigation and
 *	movement without any rebinding.
 *
 *		D-pad        arrow keys      menu navigation / move + turn
 *		analog stick arrow keys      same, with a deadzone
 *		A            RETURN + SPACE  menu select / primary trigger
 *		B            LALT            secondary trigger
 *		X            TAB             action, i.e. use switches and terminals
 *		Y            M               toggle overhead map
 *		Start        ESCAPE          pause and abort
 *		L trigger    Z               sidestep left
 *		R trigger    X               sidestep right
 *
 *	A sends RETURN and SPACE together. Neither is harmful in the other context:
 *	the menu handler in shell_sdl.cpp only inspects UP, DOWN and RETURN, and
 *	RETURN is unbound during play.
 */

#include <string.h>
#include <SDL/SDL.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>

/* Internal to SDL, but a global symbol in libSDL.a. Declared here because
   SDL_events_c.h is not installed with the library headers. */
extern int SDL_PrivateKeyboard(Uint8 state, SDL_keysym *key);

/* dc/dc_compat.c -- prints over serial and to the framebuffer. */
extern void dc_trace(int slot, const char *fmt, ...);

/* Analog stick centres near 128 and the plastic rarely returns exactly there,
   so ignore small deflections. Triggers rest at 0 and read up to 255. */
#define STICK_DEADZONE	40
#define TRIGGER_ON		64

struct dc_binding {
	int mask;		/* controller button, or one of the pseudo masks below */
	SDLKey sym;
};

/* Pseudo-buttons for the analog axes and triggers, so they can live in the
   same table as the real buttons and share the edge-detection below. */
#define DCK_STICK_LEFT	(1 << 24)
#define DCK_STICK_RIGHT	(1 << 25)
#define DCK_STICK_UP	(1 << 26)
#define DCK_STICK_DOWN	(1 << 27)
#define DCK_LTRIG		(1 << 28)
#define DCK_RTRIG		(1 << 29)

static const struct dc_binding bindings[] = {
	{ CONT_DPAD_UP,     SDLK_UP },
	{ CONT_DPAD_DOWN,   SDLK_DOWN },
	{ CONT_DPAD_LEFT,   SDLK_LEFT },
	{ CONT_DPAD_RIGHT,  SDLK_RIGHT },
	{ DCK_STICK_UP,     SDLK_UP },
	{ DCK_STICK_DOWN,   SDLK_DOWN },
	{ DCK_STICK_LEFT,   SDLK_LEFT },
	{ DCK_STICK_RIGHT,  SDLK_RIGHT },
	{ CONT_A,           SDLK_RETURN },
	{ CONT_A,           SDLK_SPACE },
	{ CONT_B,           SDLK_LALT },
	{ CONT_X,           SDLK_TAB },
	{ CONT_Y,           SDLK_m },
	{ CONT_START,       SDLK_ESCAPE },
	{ DCK_LTRIG,        SDLK_z },
	{ DCK_RTRIG,        SDLK_x },
};

#define NUM_BINDINGS (sizeof(bindings) / sizeof(bindings[0]))

static void send_key(SDLKey sym, int pressed)
{
	SDL_keysym keysym;

	memset(&keysym, 0, sizeof keysym);
	keysym.sym = sym;
	keysym.mod = KMOD_NONE;

	SDL_PrivateKeyboard(pressed ? SDL_PRESSED : SDL_RELEASED, &keysym);
}

/*
 *	Read the first controller and turn any change since the last call into key
 *	presses and releases. Safe to call when nothing is plugged in.
 */
void dc_input_poll(void)
{
	static int previous = 0;
	static int have_previous = 0;
	static int reported_present = 0, reported_absent = 0;

	maple_device_t *dev;
	cont_state_t *st;
	int current = 0, changed;
	unsigned int i;

	dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
	if (!dev) {
		if (!reported_absent) {
			reported_absent = 1;
			dc_trace(13, "controller: none found on the maple bus");
		}
		return;
	}

	st = (cont_state_t *)maple_dev_status(dev);
	if (!st)
		return;

	if (!reported_present) {
		reported_present = 1;
		dc_trace(13, "controller: found, polling");
	}

	current = st->buttons;

	if (st->joyx < -STICK_DEADZONE) current |= DCK_STICK_LEFT;
	if (st->joyx >  STICK_DEADZONE) current |= DCK_STICK_RIGHT;
	if (st->joyy < -STICK_DEADZONE) current |= DCK_STICK_UP;
	if (st->joyy >  STICK_DEADZONE) current |= DCK_STICK_DOWN;
	if (st->ltrig > TRIGGER_ON)     current |= DCK_LTRIG;
	if (st->rtrig > TRIGGER_ON)     current |= DCK_RTRIG;

	/* First poll establishes a baseline; do not report whatever happens to be
	   held at startup as a fresh press. */
	if (!have_previous) {
		have_previous = 1;
		previous = current;
		return;
	}

	changed = current ^ previous;
	if (!changed) {
		previous = current;
		return;
	}

	dc_trace(14, "controller buttons %08x -> %08x", (unsigned)previous, (unsigned)current);

	for (i = 0; i < NUM_BINDINGS; i++) {
		int mask = bindings[i].mask;

		if (!(changed & mask))
			continue;

		send_key(bindings[i].sym, (current & mask) != 0);
	}

	previous = current;
}
