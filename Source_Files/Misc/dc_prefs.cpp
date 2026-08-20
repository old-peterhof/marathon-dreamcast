/*
 *	dc_prefs.cpp -- Preferences, as the prototype draws it.
 *
 *	Three screens: the root, Sound, and Controls. Each is a table of rows handed
 *	to dc_screen_run, so the layout, the navigation and the way out are all one
 *	implementation rather than three.
 *
 *	The labels, the option names and every explainer line are copied verbatim
 *	from mockups/prototype/index.html's data-note attributes. They were written
 *	as part of the design and are better than anything reworded here -- "Adds
 *	about 7.5 seconds to every level load" says the thing that matters about More
 *	Sounds in nine words.
 *
 *	MANAGE SAVES IS NOT HERE. It sat under Preferences as a temporary home while
 *	the main menu was still the stock bitmap, and it stayed there after the main
 *	menu grew its own entry -- so the game had two doors to it, one of which made
 *	no sense. Worse, loading a game from inside Preferences started a level with
 *	the preferences dialog still on the stack, which is the likeliest cause of
 *	the pad going dead after backing out of a settings screen.
 *
 *	Values are applied when the screen is left, not as they change: the engine
 *	wants set_sound_manager_parameters and change_screen_mode called once, and a
 *	slider dragged across eight positions should not call them eight times.
 */

#include "cseries.h"

#include "sdl_dialogs.h"
#include "sdl_fonts.h"
#include "sdl_widgets.h"
#include "shape_descriptors.h"
#include "screen_drawing.h"
#include "shell.h"
#include "world.h"
#include "screen.h"
#include "preferences.h"
#include "interface.h"
#include "mysound.h"

#include <string.h>

#ifdef DC

#include "dc_screen.h"
#include "dc_prefs.h"
#include "dc_padconfig.h"

/* Row ids. Positive, and distinct from DC_SCREEN_BACK. */
enum {
	idBrightness = 1,
	idSound,
	idControls,
	idConfigureController
};

static const char *opt_gamma[] = {
	"DARKEST", "DARKER", "DARK", "NORMAL",
	"LIGHT", "REALLY LIGHT", "EVEN LIGHTER", "LIGHTEST", NULL
};

static const char *opt_off_on[]  = { "OFF", "ON", NULL };
static const char *opt_quality[] = { "8 BIT", "16 BIT", NULL };
static const char *opt_stick[]   = { "LOOK", "MOVE", NULL };

static const dc_ui_hint hints_root[] = {
	{ "+",  "MOVE",   false },
	{ "<>", "ADJUST", false },
	{ "A",  "OPEN",   true  },
	{ "B",  "BACK",   true  }
};

static const dc_ui_hint hints_values[] = {
	{ "+",  "MOVE",   false },
	{ "<>", "ADJUST", false },
	{ "B",  "BACK",   true  }
};

/*
 *	Sound.
 */
static void sound_screen(void)
{
	struct dc_row rows[6];
	struct dc_screen sc;
	uint16 flags = sound_preferences->flags;

	memset(rows, 0, sizeof rows);
	memset(&sc, 0, sizeof sc);

	rows[0].label = "VOLUME";
	rows[0].kind  = DC_ROW_SLIDER;
	rows[0].max   = NUMBER_OF_SOUND_VOLUME_LEVELS - 1;
	rows[0].value = sound_preferences->volume;
	rows[0].note  = "Sound effects and music together.";

	rows[1].label = "QUALITY";
	rows[1].kind  = DC_ROW_TOGGLE;
	rows[1].opts  = opt_quality;
	rows[1].value = (flags & _16bit_sound_flag) ? 1 : 0;
	rows[1].note  = "8-bit uses half the memory and sounds worse. Applies next launch.";

	rows[2].label = "STEREO";
	rows[2].kind  = DC_ROW_TOGGLE;
	rows[2].opts  = opt_off_on;
	rows[2].value = (flags & _stereo_flag) ? 1 : 0;
	rows[2].note  = "Turn this off if the TV has one speaker. Applies next launch.";

	rows[3].label = "ACTIVE PANNING";
	rows[3].kind  = DC_ROW_TOGGLE;
	rows[3].opts  = opt_off_on;
	rows[3].value = (flags & _dynamic_tracking_flag) ? 1 : 0;
	rows[3].note  = "Sounds move between the speakers as you turn.";

	rows[4].label = "AMBIENT SOUNDS";
	rows[4].kind  = DC_ROW_TOGGLE;
	rows[4].opts  = opt_off_on;
	rows[4].value = (flags & _ambient_sound_flag) ? 1 : 0;
	rows[4].note  = "Fans, water and machinery running in the background.";

	rows[5].label = "MORE SOUNDS";
	rows[5].kind  = DC_ROW_TOGGLE;
	rows[5].opts  = opt_off_on;
	rows[5].value = (flags & _more_sounds_flag) ? 1 : 0;
	rows[5].note  = "Adds about 7.5 seconds to every level load.";

	sc.title   = "SOUND";
	sc.kicker  = "PREFERENCES";
	sc.cap     = "SOUND";
	sc.panel_y = 140;
	sc.panel_w = 560;
	sc.row_h   = 30;
	sc.rows    = rows;
	sc.nrows   = 6;
	sc.hints   = hints_values;
	sc.nhints  = 3;
	sc.explain = true;

	dc_screen_run(&sc);

	/* Apply once, on the way out. */
	{
		uint16 out = 0;
		bool changed = false;

		if (rows[1].value) out |= _16bit_sound_flag;
		if (rows[2].value) out |= _stereo_flag;
		if (rows[3].value) out |= _dynamic_tracking_flag;
		if (rows[4].value) out |= _ambient_sound_flag;
		if (rows[5].value) out |= _more_sounds_flag;

		if (out != sound_preferences->flags) {
			sound_preferences->flags = out;
			changed = true;
		}

		if (rows[0].value != sound_preferences->volume) {
			sound_preferences->volume = rows[0].value;
			changed = true;
		}

		if (changed) {
			set_sound_manager_parameters(sound_preferences);
			write_preferences();
		}
	}
}

/*
 *	Controls.
 */
static void controls_screen(void)
{
	struct dc_row rows[8];
	struct dc_screen sc;
	uint16 mods = input_preferences->modifiers;

	for (;;) {
		int chosen;

		memset(rows, 0, sizeof rows);
		memset(&sc, 0, sizeof sc);

		rows[0].label = "ANALOG STICK";
		rows[0].kind  = DC_ROW_SELECT;
		rows[0].opts  = opt_stick;
		rows[0].value = (input_preferences->dc_stick_mode == DC_STICK_MOVE) ? 1 : 0;
		rows[0].note  = "In Look the stick turns and aims. In Move it walks and "
		                "turns, and you aim with the D-pad.";

		rows[1].label = "TURN SENSITIVITY";
		rows[1].kind  = DC_ROW_SLIDER;
		rows[1].max   = NUMBER_OF_SENS_LEVELS - 1;
		rows[1].value = (input_preferences->sens_horizontal - SENS_MINIMUM) / SENS_STEP;
		rows[1].note  = "How fast the stick turns you.";

		rows[2].label = "LOOK SENSITIVITY";
		rows[2].kind  = DC_ROW_SLIDER;
		rows[2].max   = NUMBER_OF_SENS_LEVELS - 1;
		rows[2].value = (input_preferences->sens_vertical - SENS_MINIMUM) / SENS_STEP;
		rows[2].note  = "Aiming up and down covers a small range, so it wants a "
		                "slower speed than turning.";

		rows[3].label = "INVERT LOOK";
		rows[3].kind  = DC_ROW_TOGGLE;
		rows[3].opts  = opt_off_on;
		rows[3].value = (mods & _inputmod_invert_mouse) ? 1 : 0;
		rows[3].note  = "Push the stick forward to look down.";

		rows[4].label = "ALWAYS RUN";
		rows[4].kind  = DC_ROW_TOGGLE;
		rows[4].opts  = opt_off_on;
		rows[4].value = (mods & _inputmod_interchange_run_walk) ? 1 : 0;
		rows[4].note  = "Run without holding Run/Swim.";

		rows[5].label = "ALWAYS SWIM";
		rows[5].kind  = DC_ROW_TOGGLE;
		rows[5].opts  = opt_off_on;
		rows[5].value = (mods & _inputmod_interchange_swim_sink) ? 1 : 0;
		rows[5].note  = "The same, for swimming.";

		rows[6].label = "AUTO-SWITCH WEAPONS";
		rows[6].kind  = DC_ROW_TOGGLE;
		rows[6].opts  = opt_off_on;
		rows[6].value = (mods & _inputmod_dont_switch_to_new_weapon) ? 0 : 1;
		rows[6].note  = "Draw a new weapon the moment you pick it up.";

		rows[7].label = "CONFIGURE CONTROLLER";
		rows[7].kind  = DC_ROW_ACTION;
		rows[7].id    = idConfigureController;
		rows[7].note  = "Twenty actions and ten buttons to put them on.";

		sc.title   = "CONTROLS";
		sc.kicker  = "PREFERENCES";
		sc.cap     = "CONTROLS";
		/*
		 *	Eight rows plus a caption is the tallest panel in the interface, and
		 *	at row_h 30 from panel_y 140 it reached y=406 -- 34px past the
		 *	explainer line at 372, which drew straight through CONFIGURE
		 *	CONTROLLER. Confirmed on hardware. 26 from 128 ends at 362.
		 *	dc_screen_run now warns if any screen gets this wrong again.
		 */
		sc.panel_y = 128;
		sc.panel_w = 560;
		sc.row_h   = 26;
		sc.rows    = rows;
		sc.nrows   = 8;
		sc.hints   = hints_root;
		sc.nhints  = 4;
		sc.explain = true;

		chosen = dc_screen_run(&sc);

		/* Commit whatever the screen was left holding, whichever way it was
		   left -- so opening the binding screen does not discard a sensitivity
		   the player just set. */
		{
			int mode = rows[0].value ? DC_STICK_MOVE : DC_STICK_LOOK;
			int device = (mode == DC_STICK_MOVE) ? _keyboard_or_game_pad
			                                     : _mouse_yaw_pitch;
			int sh = SENS_MINIMUM + rows[1].value * SENS_STEP;
			int sv = SENS_MINIMUM + rows[2].value * SENS_STEP;
			uint16 out = 0;
			bool changed = false;

			if (rows[3].value) out |= _inputmod_invert_mouse;
			if (rows[4].value) out |= _inputmod_interchange_run_walk;
			if (rows[5].value) out |= _inputmod_interchange_swim_sink;
			if (!rows[6].value) out |= _inputmod_dont_switch_to_new_weapon;

			if (out != input_preferences->modifiers) {
				input_preferences->modifiers = out;
				mods = out;
				changed = true;
			}
			if (mode != input_preferences->dc_stick_mode) {
				input_preferences->dc_stick_mode = mode;
				changed = true;
			}
			if (device != input_preferences->input_device) {
				input_preferences->input_device = device;
				changed = true;
			}
			if (sh != input_preferences->sens_horizontal) {
				input_preferences->sens_horizontal = sh;
				changed = true;
			}
			if (sv != input_preferences->sens_vertical) {
				input_preferences->sens_vertical = sv;
				changed = true;
			}

			/* Always push: the stick mode reaches the pad driver only here. */
			dc_apply_pad_bindings();

			if (changed)
				write_preferences();
		}

		if (chosen != idConfigureController)
			return;

		dc_pad_config();
		/* and round again, so the screen comes back rather than dumping the
		   player two levels up for having visited it */
	}
}

/*
 *	The root.
 */
void dc_preferences(void)
{
	struct dc_row rows[3];
	struct dc_screen sc;

	memset(&sc, 0, sizeof sc);

	for (;;) {
		int chosen;

		memset(rows, 0, sizeof rows);

		rows[0].label = "BRIGHTNESS";
		rows[0].kind  = DC_ROW_SELECT;
		rows[0].opts  = opt_gamma;
		rows[0].value = graphics_preferences->screen_mode.gamma_level;
		rows[0].note  = "How bright the picture is. Every CRT is different, so "
		                "set it by eye.";

		rows[1].label = "SOUND";
		rows[1].kind  = DC_ROW_SUBMENU;
		rows[1].id    = idSound;
		rows[1].note  = "Volume, quality, and what gets loaded with each level.";

		rows[2].label = "CONTROLS";
		rows[2].kind  = DC_ROW_SUBMENU;
		rows[2].id    = idControls;
		rows[2].note  = "Stick behaviour, sensitivity, and what each button does.";

		sc.title     = "PREFERENCES";
		sc.kicker    = "WRITTEN TO VMU ON EXIT";
		sc.cap       = "PREFERENCES";
		sc.panel_y   = 150;
		sc.panel_w   = 560;
		sc.row_h     = DC_UI_ROW_H;
		sc.rows      = rows;
		sc.nrows     = 3;
		sc.hints     = hints_root;
		sc.nhints    = 4;
		sc.explain   = true;

		chosen = dc_screen_run(&sc);

		/* Brightness applies on the way out of the root, since it is the root
		   that owns it. */
		if (rows[0].value != graphics_preferences->screen_mode.gamma_level) {
			graphics_preferences->screen_mode.gamma_level = rows[0].value;
			change_screen_mode(&graphics_preferences->screen_mode, false);
			write_preferences();
		}

		if (chosen == idSound)
			sound_screen();
		else if (chosen == idControls)
			controls_screen();
		else
			return;			/* BACK, from the only screen that can leave */
	}
}

#endif	/* DC */
