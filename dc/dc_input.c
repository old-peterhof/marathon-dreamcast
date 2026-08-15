/*
 *	dc_input.c -- Dreamcast controller support for Aleph One.
 *
 *	SDL 1.2's Dreamcast video driver polls only MAPLE_FUNC_MOUSE and
 *	MAPLE_FUNC_KEYBOARD (see SDL_dcevents.c), and its joystick backend is the
 *	"dummy" one. MAPLE_FUNC_CONTROLLER is never read at all, which is why BERO's
 *	2002 README says a keyboard is required to play.
 *
 *	Two paths out of here:
 *
 *	  - Digital buttons become key presses, injected with SDL_PrivateKeyboard.
 *	    That call updates SDL's internal key-state array as well as queueing an
 *	    event, and Aleph One polls SDL_GetKeyState (vbl_sdl.cpp:98) during play.
 *	    SDL_PushEvent alone would drive menus and leave the player standing
 *	    still.
 *
 *	  - The analog stick and triggers are read directly by mouse_sdl.cpp, which
 *	    feeds them into Aleph One's _fixed delta_yaw/delta_pitch path. Turning
 *	    and looking therefore stay properly analog rather than being quantised
 *	    into synthetic arrow-key presses.
 *
 *	CONTROL SCHEME
 *
 *	  Analog stick    turn left/right, look up/down     (analog, continuous)
 *	  Y               move forward
 *	  A               move backward
 *	  X               strafe left
 *	  B               strafe right
 *	  R trigger       primary fire                      (analog)
 *	  L trigger       alt fire / secondary trigger      (analog)
 *	  D-pad Up        action / use
 *	  D-pad Down      toggle map
 *	  D-pad Left      cycle weapon forward
 *	  D-pad Right     spare
 *	  Start           pause / menu
 *
 *	The stick drives aim rather than movement because Marathon's aiming benefits
 *	most from analog precision, while walk speed is effectively fixed. The D-pad
 *	shares a thumb with the stick, so nothing time-critical during combat lives
 *	there -- only stationary actions.
 *
 *	Menus are a separate context. There the D-pad and stick navigate with the
 *	arrow keys and A confirms, because BERO's menu handler in shell_sdl.cpp only
 *	understands UP, DOWN and RETURN. shell_sdl.cpp calls dc_input_set_ingame()
 *	as the game state changes.
 */

#include <string.h>
#include <unistd.h>
#include <SDL/SDL.h>
#include <SDL/SDL_dreamcast.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>

/* Internal to SDL, but a global symbol in libSDL.a. Declared here because
   SDL_events_c.h is not installed with the library headers. */
extern int SDL_PrivateKeyboard(Uint8 state, SDL_keysym *key);

/* dc/dc_compat.c -- prints over serial and to the framebuffer. */
extern void dc_trace(int slot, const char *fmt, ...);

/* Stick centres at 0 on KOS and the plastic rarely returns exactly there. */
#define STICK_DEADZONE	20
#define TRIGGER_ON		64

/* Pseudo-buttons so the stick can drive menu navigation through the same
   edge-detected table as the real buttons. Only used out of game. */
#define DCK_STICK_LEFT	(1 << 24)
#define DCK_STICK_RIGHT	(1 << 25)
#define DCK_STICK_UP	(1 << 26)
#define DCK_STICK_DOWN	(1 << 27)

struct dc_binding {
	int mask;
	SDLKey sym;
};

/*
 *	In game. Movement on the face buttons, actions on the D-pad. Turning,
 *	looking and both triggers are handled on the analog path instead and so do
 *	not appear here.
 */
static const struct dc_binding game_bindings[] = {
	{ CONT_Y,           SDLK_UP },        /* move forward     */
	{ CONT_A,           SDLK_DOWN },      /* move backward    */
	{ CONT_X,           SDLK_z },         /* strafe left      */
	{ CONT_B,           SDLK_x },         /* strafe right     */
	{ CONT_DPAD_UP,     SDLK_TAB },       /* action / use     */
	{ CONT_DPAD_DOWN,   SDLK_m },         /* toggle map       */
	{ CONT_DPAD_LEFT,   SDLK_QUOTE },     /* cycle weapon fwd */
	{ CONT_START,       SDLK_ESCAPE },    /* pause            */
};

/*
 *	Menus. The handler in shell_sdl.cpp only looks at UP, DOWN and RETURN, so
 *	the D-pad and the stick both navigate and A confirms.
 */
static const struct dc_binding menu_bindings[] = {
	{ CONT_DPAD_UP,     SDLK_UP },
	{ CONT_DPAD_DOWN,   SDLK_DOWN },
	{ CONT_DPAD_LEFT,   SDLK_LEFT },
	{ CONT_DPAD_RIGHT,  SDLK_RIGHT },
	{ DCK_STICK_UP,     SDLK_UP },
	{ DCK_STICK_DOWN,   SDLK_DOWN },
	{ DCK_STICK_LEFT,   SDLK_LEFT },
	{ DCK_STICK_RIGHT,  SDLK_RIGHT },
	{ CONT_A,           SDLK_RETURN },
	{ CONT_START,       SDLK_ESCAPE },
};

#define NUM_GAME_BINDINGS (sizeof(game_bindings) / sizeof(game_bindings[0]))
#define NUM_MENU_BINDINGS (sizeof(menu_bindings) / sizeof(menu_bindings[0]))

static int in_game = 0;

/* Latest analog readings, refreshed by dc_input_poll and consumed by
   mouse_sdl.cpp on the same frame. */
static int analog_x = 0, analog_y = 0;
static int trig_l = 0, trig_r = 0;

/*
 *	Video setup that must happen before the first SDL_SetVideoMode.
 *
 *	By default SDL's Dreamcast driver puts up a "Press Y for 60Hz" prompt and
 *	blocks inside SDL_SetVideoMode waiting for an answer. On real hardware that
 *	stalls startup on a question with only one sensible response: Marathon runs
 *	at 30 ticks/second and the console is NTSC, so 60Hz is simply correct.
 *
 *	Invisible under Flycast, which never shows the prompt -- this was found on
 *	real hardware in an earlier session and is ported back in here.
 */
void dc_input_init_video(void)
{
	SDL_DC_Default60Hz(SDL_TRUE);
	SDL_DC_ShowAskHz(SDL_FALSE);
}

void dc_input_set_ingame(int yes)
{
	if (in_game == (yes != 0))
		return;

	in_game = (yes != 0);

	/* Releasing everything on a context switch avoids a key left stuck down
	   under the other mapping -- holding Y through a level change would
	   otherwise leave SDLK_UP pressed forever in the menu. */
	{
		SDL_keysym keysym;
		unsigned int i;
		const struct dc_binding *table = in_game ? menu_bindings : game_bindings;
		unsigned int count = in_game ? NUM_MENU_BINDINGS : NUM_GAME_BINDINGS;

		for (i = 0; i < count; i++) {
			memset(&keysym, 0, sizeof keysym);
			keysym.sym = table[i].sym;
			keysym.mod = KMOD_NONE;
			SDL_PrivateKeyboard(SDL_RELEASED, &keysym);
		}
	}
}

int  dc_input_analog_x(void) { return analog_x; }
int  dc_input_analog_y(void) { return analog_y; }
int  dc_input_trigger_l(void) { return trig_l; }
int  dc_input_trigger_r(void) { return trig_r; }

static void send_key(SDLKey sym, int pressed)
{
	SDL_keysym keysym;

	memset(&keysym, 0, sizeof keysym);
	keysym.sym = sym;
	keysym.mod = KMOD_NONE;

	SDL_PrivateKeyboard(pressed ? SDL_PRESSED : SDL_RELEASED, &keysym);
}

/*
 *	Self-test, enabled by a PADTEST file on the disc exactly like AUTOSTART, so
 *	it never reaches the hardware image. Flycast would not route host input to
 *	an emulated pad in any configuration tried, so this synthesises a stick
 *	deflection to exercise the analog path unattended.
 */
static int padtest_wanted(void)
{
	static int checked = 0, wanted = 0;

	if (!checked) {
		checked = 1;
		wanted = (access("/cd/AlephOne/PADTEST", 4) == 0);
		if (wanted)
			dc_trace(15, "PADTEST: synthesising stick right");
	}

	return wanted;
}

void dc_input_poll(void)
{
	static int previous = 0;
	static int have_previous = 0;
	static int reported_present = 0, reported_absent = 0;
	static int padtest_frames = 0;

	const struct dc_binding *table;
	unsigned int count, i;
	maple_device_t *dev;
	cont_state_t *st;
	int current, changed;

	dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
	if (!dev) {
		if (!reported_absent) {
			reported_absent = 1;
			dc_trace(13, "controller: none found on the maple bus");
		}
		analog_x = analog_y = trig_l = trig_r = 0;
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

	analog_x = st->joyx;
	analog_y = st->joyy;
	trig_l = st->ltrig;
	trig_r = st->rtrig;

	if (padtest_wanted()) {
		padtest_frames++;
		analog_x = 127;		/* held hard right, so the turn rate is measurable */
	}

	/* Deadzone, then treat the stick as a d-pad for menu navigation only. */
	if (analog_x > -STICK_DEADZONE && analog_x < STICK_DEADZONE) analog_x = 0;
	if (analog_y > -STICK_DEADZONE && analog_y < STICK_DEADZONE) analog_y = 0;

	if (!in_game) {
		if (analog_x < 0) current |= DCK_STICK_LEFT;
		if (analog_x > 0) current |= DCK_STICK_RIGHT;
		if (analog_y < 0) current |= DCK_STICK_UP;
		if (analog_y > 0) current |= DCK_STICK_DOWN;
	}

	table = in_game ? game_bindings : menu_bindings;
	count = in_game ? NUM_GAME_BINDINGS : NUM_MENU_BINDINGS;

	/* First poll establishes a baseline; whatever is held at startup is not a
	   fresh press. */
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

	for (i = 0; i < count; i++) {
		int mask = table[i].mask;

		if (!(changed & mask))
			continue;

		send_key(table[i].sym, (current & mask) != 0);
	}

	previous = current;
}
