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
/* Triggers drive fire on the analog path in game; in menus they are digital and
   send TAB, which is the only way out of a focused list widget. */
#define DCK_LTRIG	(1 << 28)
#define DCK_RTRIG	(1 << 29)

struct dc_binding {
	int mask;
	SDLKey sym;
};

/*
 *	In game. Movement on the face buttons, actions on the D-pad. Turning,
 *	looking and both triggers are handled on the analog path instead and so do
 *	not appear here.
 */
/*
 *	Player-configured bindings.
 *
 *	Preferences store a small button id per engine action, not a raw button
 *	mask. Two reasons. The DCK_ codes above are 1<<28 and 1<<29, which do not
 *	fit the int16 the preferences file uses; and an id is a stable name, so the
 *	meaning of a saved card does not change if these masks ever move.
 *
 *	Only the button moves. Which key an action sends is still the engine's own
 *	input_preferences->keycodes[], so rebinding the pad never disturbs the key
 *	mapping the rest of the game agrees on.
 *
 *	The menu table below is deliberately NOT configurable. Dialog navigation
 *	must not depend on player bindings, or a bad configuration leaves no way
 *	back to fix itself.
 */

static const struct {
	int mask;
	const char *name;
} dc_buttons[] = {
	{ 0,                "---"       },	/* DC_PAD_NONE */
	{ CONT_A,           "A"         },
	{ CONT_B,           "B"         },
	{ CONT_X,           "X"         },
	{ CONT_Y,           "Y"         },
	{ CONT_DPAD_UP,     "D-Pad Up"  },
	{ CONT_DPAD_DOWN,   "D-Pad Down"},
	{ CONT_DPAD_LEFT,   "D-Pad Left"},
	{ CONT_DPAD_RIGHT,  "D-Pad Right"},
	{ DCK_LTRIG,        "L Trigger" },
	{ DCK_RTRIG,        "R Trigger" },
	{ CONT_START,       "Start"     },
};

#define NUM_DC_BUTTONS	(int)(sizeof(dc_buttons) / sizeof(dc_buttons[0]))

/* Engine action indices, matching the order preferences_sdl.cpp lists them in.
   Only the four the analog stick can drive in Move mode are needed here. */
#define ACT_MOVE_FORWARD	0
#define ACT_MOVE_BACKWARD	1
#define ACT_TURN_LEFT		2
#define ACT_TURN_RIGHT		3

#define DC_MAX_ACTIONS	32

static short cfg_button[DC_MAX_ACTIONS];	/* button id per engine action */
static short cfg_sym[DC_MAX_ACTIONS];		/* SDL key that action sends */
static int   cfg_count = 0;
static int   cfg_stick_move = 0;

int dc_input_num_buttons(void)
{
	return NUM_DC_BUTTONS;
}

const char *dc_input_button_name(int id)
{
	if (id < 0 || id >= NUM_DC_BUTTONS)
		return "?";

	return dc_buttons[id].name;
}

/*
 *	The defaults, and what DEFAULTS in the dialog restores to. Indexed by engine
 *	action; anything not named here is left unbound, which is honest -- there
 *	are twenty actions and eleven buttons, so some of them have to be.
 */
void dc_input_default_bindings(short *out, int count)
{
	static const struct { int action; int id; } defaults[] = {
		{ 0,  4 },	/* Move Forward   -> Y */
		{ 1,  1 },	/* Move Backward  -> A */
		{ 4,  3 },	/* Sidestep Left  -> X */
		{ 5,  2 },	/* Sidestep Right -> B */
		{ 12, 8 },	/* Next Weapon    -> D-Pad Right */
		{ 13, 10 },	/* Trigger        -> R Trigger */
		{ 14, 9 },	/* 2nd Trigger    -> L Trigger */
		{ 16, 7 },	/* Run/Swim       -> D-Pad Left */
		{ 18, 5 },	/* Action         -> D-Pad Up */
		{ 19, 6 },	/* Auto Map       -> D-Pad Down */
	};
	unsigned i;

	for (i = 0; i < (unsigned)count; i++)
		out[i] = 0;

	for (i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++)
		if (defaults[i].action < count)
			out[defaults[i].action] = (short)defaults[i].id;
}

void dc_input_set_bindings(const short *buttons, const short *syms, int count,
                           int stick_move)
{
	int i;

	if (count > DC_MAX_ACTIONS)
		count = DC_MAX_ACTIONS;

	for (i = 0; i < count; i++) {
		cfg_button[i] = buttons[i];
		cfg_sym[i] = syms[i];
	}

	cfg_count = count;
	cfg_stick_move = stick_move;
}

/*
 *	Capture mode, for the binding dialog.
 *
 *	While capturing, the poll records the first newly-pressed button and injects
 *	no keys at all -- otherwise the same press that names the binding would also
 *	activate whatever the dialog has focus on. The dialog is responsible for
 *	ending capture; it also times out, so a mode entered by accident cannot
 *	strand the interface with a pad that does nothing.
 */
#define CAPTURE_TIMEOUT_POLLS	1800	/* about a minute at 30fps */

static int capturing = 0;
static int captured = -1;
static int capture_polls = 0;

void dc_input_begin_capture(void)
{
	capturing = 1;
	captured = -1;
	capture_polls = 0;
}

void dc_input_end_capture(void)
{
	capturing = 0;
	captured = -1;
}

/*
 *	Returns the button id pressed, 0 if the player cancelled with Start, or -1 if
 *	nothing has happened yet. Capture ends itself on any of those.
 */
int dc_input_take_capture(void)
{
	int id = captured;

	if (id >= 0) {
		capturing = 0;
		captured = -1;
	}

	return id;
}

int dc_input_capturing(void)
{
	return capturing;
}

static const struct dc_binding game_bindings[] = {
	{ CONT_Y,           SDLK_UP },        /* move forward     */
	{ CONT_A,           SDLK_DOWN },      /* move backward    */
	{ CONT_X,           SDLK_z },         /* strafe left      */
	{ CONT_B,           SDLK_x },         /* strafe right     */
	{ CONT_DPAD_UP,     SDLK_TAB },       /* action / use     */
	{ CONT_DPAD_DOWN,   SDLK_m },         /* toggle map       */
	{ CONT_DPAD_RIGHT,  SDLK_QUOTE },     /* next weapon      */
	{ CONT_DPAD_LEFT,   SDLK_LCTRL },     /* swim, see below  */
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
	/* X is the secondary action -- delete, on the saves screen. It is in the
	   fixed table rather than being bindable for the same reason Start is: the
	   one destructive action in the interface must not be something a player can
	   accidentally rebind away, or move somewhere they will hit it. */
	{ CONT_X,           SDLK_DELETE },
	/* w_list_base::event swallows UP and DOWN on purpose -- "Prevent selection
	   of previous/next widget" -- so once a list has focus the D-pad can never
	   leave it, which stranded the save dialog with no way to reach "new save
	   game". TAB is how a keyboard escapes a list, so the triggers do that. */
	{ DCK_LTRIG,        SDLK_TAB },
	{ DCK_RTRIG,        SDLK_TAB },
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
/*
 *	List everything on the maple bus once, at startup. A peripheral in an
 *	expansion slot -- a rumble pack, say -- is invisible to the code otherwise,
 *	so if the presence of one changes behaviour this is what shows it.
 */
void dc_input_dump_maple(void)
{
	int port, unit, slot = 0;

	for (port = 0; port < 4; port++) {
		for (unit = 0; unit < 6; unit++) {
			maple_device_t *d = maple_enum_dev(port, unit);

			if (!d || !d->valid)
				continue;

			dc_trace(20 + (slot++ % 3), "maple %c%d: %s func=%08lx",
			         'A' + port, unit, d->info.product_name,
			         (unsigned long)d->info.functions);
		}
	}

	if (!slot)
		dc_trace(20, "maple: nothing enumerated");
}

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

static void dc_input_poll_body(void);

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

/*
 *	Re-entry guard.
 *
 *	This function keeps static state to detect button edges, so calling it from
 *	inside itself loses or invents presses. That is not hypothetical: b48 added a
 *	call in wait_for_click_or_keypress so the chapter screen could be skipped,
 *	b49 added one inside the main event loop's idle wait so a menu would feel
 *	responsive, and each worked on hardware alone. Together they crashed on level
 *	load, because a chapter screen is drawn from inside process_event, which the
 *	main loop calls with its own poll already in progress.
 *
 *	Re-entering is now simply ignored. The outer poll is already reading the pad;
 *	a second read a few microseconds later has nothing to add.
 */
void dc_input_poll(void)
{
	static int busy = 0;

	if (busy)
		return;

	busy = 1;
	dc_input_poll_body();
	busy = 0;
}

static void dc_input_poll_body(void)
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
		/* Pulse rather than hold: the first poll establishes the baseline, so a
		   permanently-deflected stick never produces an edge and never injects
		   a key. Two seconds on, two off, which is slow enough to read on
		   screen and still gives a measurable turn rate while held. */
		padtest_frames++;
		if ((padtest_frames % 120) < 60)
			analog_x = 127;
	}

	/* Deadzone, then treat the stick as a d-pad for menu navigation only. */
	if (analog_x > -STICK_DEADZONE && analog_x < STICK_DEADZONE) analog_x = 0;
	if (analog_y > -STICK_DEADZONE && analog_y < STICK_DEADZONE) analog_y = 0;

	/*
	 *	Synthetic codes for the triggers and the stick.
	 *
	 *	Menus always want them. Gameplay wants them only in Move mode, where the
	 *	stick drives forward, back and turning; in Look mode the stick belongs to
	 *	the analog path in mouse_sdl.cpp and turning it into keypresses here
	 *	would fight it.
	 */
	/*
	 *	The triggers are always available as buttons. In Look mode test_mouse()
	 *	in mouse_sdl.cpp also reads them for primary and alt fire, so the default
	 *	bindings fire twice -- both set the same action flag, so that is
	 *	harmless. In Move mode input_device is off, test_mouse() never runs, and
	 *	these bindings are the only thing that fires the weapon.
	 */
	if (trig_l > TRIGGER_ON) current |= DCK_LTRIG;
	if (trig_r > TRIGGER_ON) current |= DCK_RTRIG;

	/*
	 *	The stick is a D-pad for menus always, and in gameplay only in Move mode.
	 *	In Look mode it belongs to the analog path in mouse_sdl.cpp, and turning
	 *	it into keypresses here would fight it.
	 */
	if (!in_game || cfg_stick_move) {
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

	/*
	 *	Capture swallows the frame entirely: it names the button that was
	 *	pressed and injects nothing, so the press cannot also drive the dialog
	 *	it is being typed into. Start reports id 0, which the dialog reads as
	 *	cancel, and the timeout is the backstop if nothing at all is pressed.
	 */
	if (capturing) {
		int pressed = changed & current;

		previous = current;

		if (pressed) {
			int b;

			for (b = 1; b < NUM_DC_BUTTONS; b++)
				if (pressed & dc_buttons[b].mask) {
					captured = (dc_buttons[b].mask == CONT_START) ? 0 : b;
					break;
				}
		} else if (++capture_polls > CAPTURE_TIMEOUT_POLLS) {
			captured = 0;
		}

		/*
		 *	The dialog loop is a poll, not a wait, but a widget's event() only
		 *	runs when there is an event to hand it. Capture injects no keys, so
		 *	without this the binding would be recorded and never collected.
		 *	SDLK_UNKNOWN is the wake-up: the widget consumes it, and the dialog's
		 *	own key handling has no case for it.
		 */
		if (captured >= 0)
			send_key(SDLK_UNKNOWN, 1);

		return;
	}

	if (!changed) {
		previous = current;
		return;
	}

	{
		static int shown = 0;
		if (shown < 6) {
			shown++;
			dc_trace(17, "btn %08x->%08x ingame=%d", (unsigned)previous,
			         (unsigned)current, in_game);
		}
	}

	/*
	 *	Configured bindings replace the static table for gameplay only.
	 *
	 *	In Move mode the stick's four directions are ORed into the movement
	 *	actions' masks, so the stick and any button bound to the same action are
	 *	simply two ways to press it.
	 */
	if (in_game && cfg_count > 0) {
		/*
		 *	Start is not one of the engine's twenty actions, so it cannot be a
		 *	row in the binding screen, and the configured table would drop it.
		 *	It stays wired here for the same reason the menu table is fixed:
		 *	whatever the player has done to the other buttons, this one always
		 *	gets them out. Capture reads Start as cancel, so it can never be
		 *	bound to something else either.
		 */
		if (changed & CONT_START)
			send_key(SDLK_ESCAPE, (current & CONT_START) != 0);

		for (i = 0; i < (unsigned)cfg_count; i++) {
			int id = cfg_button[i];
			int mask = (id > 0 && id < NUM_DC_BUTTONS) ? dc_buttons[id].mask : 0;

			if (cfg_stick_move) {
				if (i == ACT_MOVE_FORWARD)  mask |= DCK_STICK_UP;
				if (i == ACT_MOVE_BACKWARD) mask |= DCK_STICK_DOWN;
				if (i == ACT_TURN_LEFT)     mask |= DCK_STICK_LEFT;
				if (i == ACT_TURN_RIGHT)    mask |= DCK_STICK_RIGHT;
			}

			if (!mask || !(changed & mask))
				continue;

			send_key((SDLKey)cfg_sym[i], (current & mask) != 0);
		}

		previous = current;
		return;
	}

	for (i = 0; i < count; i++) {
		int mask = table[i].mask;

		if (!(changed & mask))
			continue;

		{
			static int keys_shown = 0;
			if (keys_shown < 6) {
				keys_shown++;
				dc_trace(18, "key sym=%d %s", (int)table[i].sym,
				         (current & mask) ? "down" : "up");
			}
		}

		send_key(table[i].sym, (current & mask) != 0);
	}

	previous = current;
}
