#ifndef __PREFERENCES_H
#define __PREFERENCES_H

/*

	preferences.h
	Tuesday, June 13, 1995 10:07:04 AM- rdm created.

Feb 10, 2000 (Loren Petrich):
	Added stuff for input modifiers: run/walk and swim/sink

Feb 25, 2000 (Loren Petrich):
	Set up persistent stuff for the chase cam and crosshairs

Mar 2, 2000 (Loren Petrich):
	Added chase-cam and crosshairs interfaces

Mar 14, 2000 (Loren Petrich):
	Added OpenGL stuff

Apr 27, 2000 (Loren Petrich):
	Added Josh Elsasser's "don't switch weapons" patch
*/

#include "interface.h"
#include "ChaseCam.h"
#include "Crosshairs.h"
#include "OGL_Setup.h"

/* New preferences junk */
struct graphics_preferences_data
{
	struct screen_mode_data screen_mode;
#ifdef mac
	GDSpec device_spec;
#endif
	// LP change: added OpenGL support
	OGL_ConfigureData OGL_Configure;
};

struct serial_number_data
{
	bool network_only;
	byte long_serial_number[10];
	Str255 user_name;
	Str255 tokenized_serial_number;
};

struct network_preferences_data
{
	bool allow_microphone;
	bool  game_is_untimed;
	int16 type; // look in network_dialogs.c for _ethernet, etc...
	int16 game_type;
	int16 difficulty_level;
	uint16 game_options; // Penalize suicide, etc... see map.h for constants
	int32 time_limit;
	int16 kill_limit;
	int16 entry_point;
};

struct player_preferences_data
{
#ifdef mac
	unsigned char name[PREFERENCES_NAME_LENGTH+1];
#else
	char name[PREFERENCES_NAME_LENGTH+1];
#endif
	int16 color;
	int16 team;
	uint32 last_time_ran;
	int16 difficulty_level;
	bool background_music_on;
	struct ChaseCamData ChaseCam;
	struct CrosshairData Crosshairs;
};

// LP addition: input-modifier flags
// run/walk and swim/sink
// LP addition: Josh Elsasser's dont-switch-weapons patch
enum {
	_inputmod_interchange_run_walk = 0x0001,
	_inputmod_interchange_swim_sink = 0x0002,
	_inputmod_dont_switch_to_new_weapon = 0x0004,
	_inputmod_invert_mouse = 0x0008
};

struct input_preferences_data
{
	int16 input_device;
	int16 keycodes[NUMBER_OF_KEYS];
	// LP addition: input modifiers
	uint16 modifiers;
	// Analog look sensitivity, as a percentage; 100 is the tuned default.
	// Appended rather than inserted so older preferences files still line up.
	int16 sens_horizontal;
	int16 sens_vertical;
	// Dreamcast pad bindings, one per engine action, indexed the same way as
	// keycodes[] above -- so NUMBER_OF_KEYS, which is 21, not the 20 the
	// configuration screen lists. The 21st is the microphone key, which no
	// screen exposes but which still occupies an index.
	//
	// Values are the raw controller button masks from dc/dc_input.c, including
	// its synthetic DCK_* codes for the triggers and stick directions. Zero
	// means unbound.
	int16 dc_pad_bindings[NUMBER_OF_KEYS];
	// What the analog stick does: DC_STICK_LOOK turns and looks through the
	// analog path, DC_STICK_MOVE drives forward, back and turning the way the
	// D-pad does.
	int16 dc_stick_mode;
};

// Analog stick modes, stored in dc_stick_mode above.
#define DC_STICK_LOOK	0
#define DC_STICK_MOVE	1

// dc/dc_input.c. Bindings are pushed to the driver rather than read by it: it
// is C, and preferences.h is not something it can include.
extern "C" {
	void dc_input_default_bindings(short *out, int count);
	void dc_input_set_bindings(const short *buttons, const short *syms,
	                           int count, int stick_move);
	int dc_input_num_buttons(void);
	const char *dc_input_button_name(int id);
	void dc_input_begin_capture(void);
	void dc_input_end_capture(void);
	int dc_input_take_capture(void);
	int dc_input_capturing(void);
}

void dc_apply_pad_bindings(void);

// The slider tops out at 100% deliberately. Aleph One packs delta_yaw into a
// bounded field in the action flags (physics.cpp:278, MAXIMUM_ABSOLUTE_YAW), so
// the turn rate saturates around 67 deg/sec no matter how large a value is fed
// in -- quadrupling the base scale only raised the measured rate by half. Above
// 100% the slider would do nothing, so the useful range is all of it.
#define SENS_MINIMUM   5
#define SENS_MAXIMUM   100
#define SENS_STEP      5
// Reported too sensitive on real hardware at 100% with a linear response.
// With the squared curve now in mouse_sdl.cpp and a 50% default the stick is
// calm around centre; the slider goes to 100% for anyone who wants it faster.
// Reported still too fast at 50% once the scale was corrected, so default low
// and let the player raise it. The slider now spans roughly 4 to 67 deg/sec at
// full deflection, 67 being the engine's own ceiling.
#define SENS_DEFAULT   30
#define NUMBER_OF_SENS_LEVELS ((SENS_MAXIMUM - SENS_MINIMUM) / SENS_STEP + 1)

#define MAXIMUM_PATCHES_PER_ENVIRONMENT (32)

struct environment_preferences_data
{
#ifdef mac
	FSSpec map_file;
	FSSpec physics_file;
	FSSpec shapes_file;
	FSSpec sounds_file;
#else
	char map_file[256];
	char physics_file[256];
	char shapes_file[256];
	char sounds_file[256];
#endif
	uint32 map_checksum;
	uint32 physics_checksum;
	TimeType shapes_mod_date;
	TimeType sounds_mod_date;
	uint32 patches[MAXIMUM_PATCHES_PER_ENVIRONMENT];
#ifdef SDL
	char theme_dir[256];
#endif
};

/* New preferences.. (this sorta defeats the purpose of this system, but not really) */
extern struct graphics_preferences_data *graphics_preferences;
extern struct serial_number_data *serial_preferences;
extern struct network_preferences_data *network_preferences;
extern struct player_preferences_data *player_preferences;
extern struct input_preferences_data *input_preferences;
extern struct sound_manager_parameters *sound_preferences;
extern struct environment_preferences_data *environment_preferences;

/* --------- functions */
void initialize_preferences(void);
void handle_preferences(void);
void write_preferences(void);

#endif
