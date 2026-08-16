/*
 *  preferences_sdl.cpp - Preferences handling, SDL specific stuff
 *
 *  Written in 2000 by Christian Bauer
 */

#ifndef _SDL_PREFERNCES_
#define _SDL_PREFERNCES_

#ifdef __MVCPP__
#include "sdl_cseries.h"
#include "shape_descriptors.h"
#endif

#include "sdl_dialogs.h"
#include "sdl_fonts.h"
#include "sdl_widgets.h"
#include "screen.h"
#ifdef DC
#include "dc_slots.h"
#include "dc_explain.h"
#endif
#include "images.h"
#include "find_files.h"
#include "screen_drawing.h"

#include <string.h>
#include <vector>

#ifdef HAVE_UNISTD_H
#include <unistd.h>	// for getlogin()
#endif

#ifdef __WIN32__
#include <windows.h> // for GetUserName()
#endif

#ifdef __MVCPP__

#include "world.h"
#include "shell.h"
#include "preferences.h"
#include "mysound.h"
#include "wad.h"

#endif


// From shell_sdl.cpp
extern vector<DirectorySpecifier> data_search_path;

// Prototypes
static void player_dialog(void *arg);
static void opengl_dialog(void *arg);
static void graphics_dialog(void *arg);
static void sound_dialog(void *arg);
#ifdef DC
// Opens the CONTROLS dialog without anyone navigating a menu. Used by the
// AUTOSTART=controls marker so the sensitivity sliders can be screenshotted
// unattended -- synthesised keystrokes into Flycast proved unreliable.
// controls_dialog never dereferences its parent argument, so NULL is fine.
static void controls_dialog(void *arg);
extern "C" void dc_open_controls_dialog(void) { controls_dialog(0); }
#endif

static void controls_dialog(void *arg);
static void environment_dialog(void *arg);
static void keyboard_dialog(void *arg);
#ifdef DC
static void dc_manage_saves_proc(void *arg);
#endif
#ifdef DC
static void pad_dialog(void *arg);
static void pad_advanced_dialog(void *arg);
#endif


/*
 *  Get user name
 */

static void get_name_from_system(char *name)
{
#if defined(unix) || defined(__BEOS__) || (defined (__APPLE__) && defined (__MACH__))

	char *login = getlogin();
	strcpy(name, login ? login : "Bob User");

#elif defined(__WIN32__)

	char login[17];
	DWORD len = 17;

	bool hasName = GetUserName((LPSTR)login, &len);
	if (hasName && strpbrk(login, "\\/:*?\"<>|") == NULL) // Ignore illegal names
		strcpy(name, login);
	else
		strcpy(name, "Bob User");

#elif defined(DC)
	strcpy(name,"Bob User");
#else
#error get_name_from_system() not implemented for this platform
#endif
}


/*
 *  Ethernet always available
 */

static bool ethernet_active(void)
{
	return true;
}


/*
 *  Main preferences dialog
 */

static const char *gamma_labels[9] = {
	"Darkest", "Darker", "Dark", "Normal", "Light", "Really Light", "Even Lighter", "Lightest", NULL
};

void handle_preferences(void)
{
	// Save the existing preferences, in case we have to reload them
	write_preferences();

	// Load sensible palette
	if (SDL_GetVideoSurface()->format->BitsPerPixel == 8) {
		struct color_table *system_colors = build_8bit_system_color_table();
		assert_world_color_table(system_colors, system_colors);
		delete system_colors;
	}

	// Create top-level dialog
	dialog d;
	d.add(new w_static_text("PREFERENCES", TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
#ifdef DC
	/*
	 *	Three rows where there were five, per UI-HANDOFF section 4, which is the
	 *	MENU-TREE.md audit applied.
	 *
	 *	PLAYER is gone: difficulty moved to New Game where it belongs to the run,
	 *	the name needs a keyboard, and both colours are network appearance for a
	 *	game with no network. ENVIRONMENT is gone entirely -- all five rows browse
	 *	for replacement data files, and there is nothing to browse on a fixed
	 *	disc. GRAPHICS collapses to its one live row: colour depth, resolution,
	 *	screen size and fullscreen all describe hardware we already know, and one
	 *	of them is the flag that once stuck off and rendered the whole game at
	 *	320x160.
	 *
	 *	Brightness is promoted to the root rather than kept behind a button of
	 *	its own, because a screen with one control on it is not a screen.
	 */
	w_explain *explain = new w_explain;
	dc_explain_begin(explain);

	w_select *gamma_w = new w_select("Brightness",
		graphics_preferences->screen_mode.gamma_level, gamma_labels);
	d.add(gamma_w);
	dc_explain_add(gamma_w, "How bright the picture is. Worth setting on a CRT, "
	                        "where the darkest levels can be hard to read.");

	d.add(new w_spacer());

	w_button *sound_b = new w_button("SOUND", sound_dialog, &d);
	d.add(sound_b);
	dc_explain_add(sound_b, "Volume, quality, and which sounds the game loads.");

	w_button *controls_b = new w_button("CONTROLS", controls_dialog, &d);
	d.add(controls_b);
	dc_explain_add(controls_b, "The analog stick, sensitivity, and which button "
	                           "does what.");

	w_button *saves_b = new w_button("MANAGE SAVES", dc_manage_saves_proc, &d);
	d.add(saves_b);
	dc_explain_add(saves_b, "Load or delete any of the four saved games.");

	d.add(new w_spacer());
	d.add(explain);
	d.add(new w_spacer());
	d.add(new w_button("RETURN", dialog_cancel, &d));

	dc_explain_arm(&d);
#else
	d.add(new w_button("PLAYER", player_dialog, &d));
	d.add(new w_button("GRAPHICS", graphics_dialog, &d));
	d.add(new w_button("SOUND", sound_dialog, &d));
	d.add(new w_button("CONTROLS", controls_dialog, &d));
	d.add(new w_button("ENVIRONMENT", environment_dialog, &d));
	d.add(new w_spacer());
	d.add(new w_button("RETURN", dialog_cancel, &d));
#endif

	// Clear menu screen
	clear_screen();

	// Run dialog
	d.run();

#ifdef DC
	dc_explain_end();

	// Brightness lives on the root screen now, so it is applied here rather than
	// in the graphics dialog that no longer runs.
	{
		int gamma = gamma_w->get_selection();

		if (gamma != graphics_preferences->screen_mode.gamma_level) {
			graphics_preferences->screen_mode.gamma_level = gamma;
			change_screen_mode(&graphics_preferences->screen_mode, false);
			write_preferences();
		}
	}
#endif

	// Redraw main menu
	display_main_menu();
}


/*
 *  Player dialog
 */

static const char *level_labels[6] = {
	"Kindergarten", "Easy", "Normal", "Major Damage", "Total Carnage", NULL
};

static void player_dialog(void *arg)
{
	dialog *parent = (dialog *)arg;

	// Create dialog
	dialog d;
	d.add(new w_static_text("PLAYER SETTINGS", TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
	w_select *level_w = new w_select("Difficulty", player_preferences->difficulty_level, level_labels);
	d.add(level_w);
	d.add(new w_spacer());
	d.add(new w_static_text("Network Appearance"));
	w_text_entry *name_w = new w_text_entry("Name", PREFERENCES_NAME_LENGTH, player_preferences->name);
	d.add(name_w);
	w_player_color *pcolor_w = new w_player_color("Color", player_preferences->color);
	d.add(pcolor_w);
	w_player_color *tcolor_w = new w_player_color("Team Color", player_preferences->team);
	d.add(tcolor_w);
	d.add(new w_spacer());
	d.add(new w_left_button("ACCEPT", dialog_ok, &d));
	d.add(new w_right_button("CANCEL", dialog_cancel, &d));

	// Clear screen
	clear_screen();

	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;

		const char *name = name_w->get_text();
		if (strcmp(name, player_preferences->name)) {
			strcpy(player_preferences->name, name);
			changed = true;
		}

		int level = level_w->get_selection();
		if (level != player_preferences->difficulty_level) {
			player_preferences->difficulty_level = level;
			changed = true;
		}

		int color = pcolor_w->get_selection();
		if (color != player_preferences->color) {
			player_preferences->color = color;
			changed = true;
		}

		int team = tcolor_w->get_selection();
		if (team != player_preferences->team) {
			player_preferences->team = team;
			changed = true;
		}

		if (changed)
			write_preferences();
	}
}


/*
 *  Handle graphics dialog
 */

static const char *depth_labels[3] = {
	"8 Bit", "16 Bit", NULL
};

static const char *resolution_labels[3] = {
	"Low", "High", NULL
};

static const char *size_labels[13] = {
	"320x160", "480x240", "640x320", "640x480 (no HUD)",
	"800x400", "800x600 (no HUD)", "1024x512", "1024x768 (no HUD)",
	"1280x640", "1280x1024 (no HUD)", "1600x800", "1600x1200 (no HUD)", NULL
};


static void graphics_dialog(void *arg)
{
	dialog *parent = (dialog *)arg;

	// Create dialog
	dialog d;
	d.add(new w_static_text("GRAPHICS SETUP", TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
	w_toggle *depth_w = new w_toggle("Color Depth", graphics_preferences->screen_mode.bit_depth == 16, depth_labels);
	w_toggle *resolution_w = new w_toggle("Resolution", graphics_preferences->screen_mode.high_resolution, resolution_labels);
	if (graphics_preferences->screen_mode.acceleration == _no_acceleration) {
		d.add(depth_w);
		d.add(resolution_w);
	}
	w_select *size_w = new w_select("Screen Size", graphics_preferences->screen_mode.size, size_labels);
	d.add(size_w);
	w_toggle *fullscreen_w = new w_toggle("Fullscreen", graphics_preferences->screen_mode.fullscreen);
	d.add(fullscreen_w);
	w_select *gamma_w = new w_select("Brightness", graphics_preferences->screen_mode.gamma_level, gamma_labels);
	d.add(gamma_w);
#ifdef HAVE_OPENGL
	d.add(new w_spacer());
	d.add(new w_button("OPENGL OPTIONS", opengl_dialog, &d));
#endif
	d.add(new w_spacer());
	d.add(new w_left_button("ACCEPT", dialog_ok, &d));
	d.add(new w_right_button("CANCEL", dialog_cancel, &d));

	// Clear screen
	clear_screen();

	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;

		bool fullscreen = fullscreen_w->get_selection();
		if (fullscreen != graphics_preferences->screen_mode.fullscreen) {
			graphics_preferences->screen_mode.fullscreen = fullscreen;
			// This is the only setting that has an immediate effect
			toggle_fullscreen(fullscreen);
			parent->draw();
			changed = true;
		}

		int depth = (depth_w->get_selection() ? 16 : 8);
		if (depth != graphics_preferences->screen_mode.bit_depth) {
			graphics_preferences->screen_mode.bit_depth = depth;
			changed = true;
			// don't change mode now; it will be changed when the game starts
		}

		bool hi_res = resolution_w->get_selection();
		if (hi_res != graphics_preferences->screen_mode.high_resolution) {
			graphics_preferences->screen_mode.high_resolution = hi_res;
			changed = true;
		}

		int size = size_w->get_selection();
		if (size != graphics_preferences->screen_mode.size) {
			graphics_preferences->screen_mode.size = size;
			changed = true;
			// don't change mode now; it will be changed when the game starts
		}

		int gamma = gamma_w->get_selection();
		if (gamma != graphics_preferences->screen_mode.gamma_level) {
			graphics_preferences->screen_mode.gamma_level = gamma;
			changed = true;
		}

		if (changed) {
			write_preferences();
			parent->draw();		// DirectX seems to need this
		}
	}
}


/*
 *  OpenGL dialog
 */

static void opengl_dialog(void *arg)
{
	dialog *parent = (dialog *)arg;
	OGL_ConfigureData &prefs = Get_OGL_ConfigureData();

	// Create dialog
	dialog d;
	d.add(new w_static_text("OPENGL OPTIONS", TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
	w_toggle *zbuffer_w = new w_toggle("Z Buffer", prefs.Flags & OGL_Flag_ZBuffer);
	d.add(zbuffer_w);
	w_toggle *landscape_w = new w_toggle("Landscapes", !(prefs.Flags & OGL_Flag_FlatLand));
	d.add(landscape_w);
	w_toggle *fog_w = new w_toggle("Fog", prefs.Flags & OGL_Flag_Fog);
	d.add(fog_w);
	w_toggle *static_w = new w_toggle("Static Effect", !(prefs.Flags & OGL_Flag_FlatStatic));
	d.add(static_w);
	w_toggle *fader_w = new w_toggle("Color Effects", prefs.Flags & OGL_Flag_Fader);
	d.add(fader_w);
	w_toggle *liq_w = new w_toggle("Transparent Liquids", prefs.Flags & OGL_Flag_LiqSeeThru);
	d.add(liq_w);
	w_toggle *map_w = new w_toggle("OpenGL Overhead Map", prefs.Flags & OGL_Flag_Map);
	d.add(map_w);
	w_toggle *hud_w = new w_toggle("OpenGL HUD", prefs.Flags & OGL_Flag_HUD);
	d.add(hud_w);
	w_toggle *models_w = new w_toggle("3D Models", prefs.Flags & OGL_Flag_3D_Models);
	d.add(models_w);
	d.add(new w_spacer());
	d.add(new w_left_button("ACCEPT", dialog_ok, &d));
	d.add(new w_right_button("CANCEL", dialog_cancel, &d));

	// Clear screen
	clear_screen();

	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;

		uint16 flags = 0;
		if (zbuffer_w->get_selection()) flags |= OGL_Flag_ZBuffer;
		if (!(landscape_w->get_selection())) flags |= OGL_Flag_FlatLand;
		if (fog_w->get_selection()) flags |= OGL_Flag_Fog;
		if (!(static_w->get_selection())) flags |= OGL_Flag_FlatStatic;
		if (fader_w->get_selection()) flags |= OGL_Flag_Fader;
		if (liq_w->get_selection()) flags |= OGL_Flag_LiqSeeThru;
		if (map_w->get_selection()) flags |= OGL_Flag_Map;
		if (hud_w->get_selection()) flags |= OGL_Flag_HUD;
		if (models_w->get_selection()) flags |= OGL_Flag_3D_Models;

		if (flags != prefs.Flags) {
			prefs.Flags = flags;
			changed = true;
		}

		if (changed)
			write_preferences();
	}
}


/*
 *  Sound dialog
 */

class w_toggle *stereo_w, *dynamic_w;

class w_stereo_toggle : public w_toggle {
public:
	w_stereo_toggle(const char *name, bool selection) : w_toggle(name, selection) {}

	void selection_changed(void)
	{
		// Turning off stereo turns off dynamic tracking
		w_toggle::selection_changed();
		if (selection == false)
			dynamic_w->set_selection(false);
	}
};

class w_dynamic_toggle : public w_toggle {
public:
	w_dynamic_toggle(const char *name, bool selection) : w_toggle(name, selection) {}

	void selection_changed(void)
	{
		// Turning on dynamic tracking turns on stereo
		w_toggle::selection_changed();
		if (selection == true)
			stereo_w->set_selection(true);
	}
};

static const char *channel_labels[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", NULL};

class w_volume_slider : public w_slider {
public:
	w_volume_slider(const char *name, int vol) : w_slider(name, NUMBER_OF_SOUND_VOLUME_LEVELS, vol) {}
	~w_volume_slider() {}

	void item_selected(void)
	{
		test_sound_volume(selection, _snd_adjust_volume);
	}
};

static void sound_dialog(void *arg)
{
	dialog *parent = (dialog *)arg;

	// Create dialog
	dialog d;
	d.add(new w_static_text("SOUND SETUP", TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
#ifdef DC
	w_explain *explain = new w_explain;
#endif
	static const char *quality_labels[3] = {"8 Bit", "16 Bit", NULL};
	w_toggle *quality_w = new w_toggle("Quality", sound_preferences->flags & _16bit_sound_flag, quality_labels);
	d.add(quality_w);
	stereo_w = new w_stereo_toggle("Stereo", sound_preferences->flags & _stereo_flag);
	d.add(stereo_w);
	dynamic_w = new w_dynamic_toggle("Active Panning", sound_preferences->flags & _dynamic_tracking_flag);
	d.add(dynamic_w);
	w_toggle *ambient_w = new w_toggle("Ambient Sounds", sound_preferences->flags & _ambient_sound_flag);
	d.add(ambient_w);
	w_toggle *more_w = new w_toggle("More Sounds", sound_preferences->flags & _more_sounds_flag);
	d.add(more_w);
#ifdef DC
	/*
	 *	Channels is dropped: it sets how many sounds can play at once, which is
	 *	a property of the hardware and not a taste. Everything else stays.
	 */
	w_select *channels_w = NULL;
#else
	w_select *channels_w = new w_select("Channels", sound_preferences->channel_count, channel_labels);
	d.add(channels_w);
#endif
	w_volume_slider *volume_w = new w_volume_slider("Volume", sound_preferences->volume);
	d.add(volume_w);
#ifdef DC
	/*
	 *	More Sounds is the only preference in the game with a large measured
	 *	cost, and it sat among six that cost nothing. Saying so is the clearest
	 *	case for these lines existing at all.
	 */
	dc_explain_begin(explain);
	dc_explain_add(quality_w, "8-bit halves the memory sounds take and makes "
	                          "them hissier.");
	dc_explain_add(stereo_w, "Places sounds left and right.");
	dc_explain_add(dynamic_w, "Keeps sounds in place as you turn, rather than "
	                          "fixed to the screen.");
	dc_explain_add(ambient_w, "Background noise: machinery, water, wind.");
	dc_explain_add(more_w, "Loads the full set of monster sounds. Costs about "
	                       "7.5 seconds of every level load.");
	dc_explain_add(volume_w, "Overall loudness.");

	d.add(new w_spacer());
	d.add(explain);
	dc_explain_arm(&d);
#endif
	d.add(new w_spacer());
	d.add(new w_left_button("ACCEPT", dialog_ok, &d));
	d.add(new w_right_button("CANCEL", dialog_cancel, &d));

	// Clear screen
	clear_screen();

	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;

		uint16 flags = 0;
		if (quality_w->get_selection()) flags |= _16bit_sound_flag;
		if (stereo_w->get_selection()) flags |= _stereo_flag;
		if (dynamic_w->get_selection()) flags |= _dynamic_tracking_flag;
		if (ambient_w->get_selection()) flags |= _ambient_sound_flag;
		if (more_w->get_selection()) flags |= _more_sounds_flag;

		if (flags != sound_preferences->flags) {
			sound_preferences->flags = flags;
			changed = true;
		}

		if (channels_w) {
			int channel_count = channels_w->get_selection();
			if (channel_count != sound_preferences->channel_count) {
				sound_preferences->channel_count = channel_count;
				changed = true;
			}
		}

		int volume = volume_w->get_selection();
		if (volume != sound_preferences->volume) {
			sound_preferences->volume = volume;
			changed = true;
		}

		if (changed) {
			set_sound_manager_parameters(sound_preferences);
			write_preferences();
		}
	}

#ifdef DC
	dc_explain_end();
#endif
}


/*
 *  Controls dialog
 */

static w_toggle *mouse_w;
#ifdef DC
static w_select *stick_mode_w;
#endif

#ifdef DC
static void dc_manage_saves_proc(void *arg)
{
	dialog *parent = (dialog *)arg;

	dc_manage_saves();

	parent->draw();
}
#endif

static void controls_dialog(void *arg)
{
	dialog *parent = (dialog *)arg;

	// Create dialog
	dialog d;
	d.add(new w_static_text("CONTROLS", TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
#ifdef DC
	// There is no mouse on a Dreamcast, and input_device is what makes
	// vbl_sdl.cpp call test_mouse() -- the path carrying the stick's yaw and
	// pitch. So the setting is presented as what the stick does, and the two
	// choices are the two halves of that switch.
	static const char *stick_labels[] = {"Look", "Move", NULL};
	stick_mode_w = new w_select("Analog Stick",
		input_preferences->dc_stick_mode == DC_STICK_MOVE ? 1 : 0, stick_labels);
	d.add(stick_mode_w);
#else
	mouse_w = new w_toggle("Mouse Control", input_preferences->input_device);
	d.add(mouse_w);
#endif
#ifdef DC
	w_toggle *invert_mouse_w = new w_toggle("Invert Look", input_preferences->modifiers & _inputmod_invert_mouse);
#else
	w_toggle *invert_mouse_w = new w_toggle("Invert Mouse", input_preferences->modifiers & _inputmod_invert_mouse);
#endif
	d.add(invert_mouse_w);
	w_toggle *always_run_w = new w_toggle("Always Run", input_preferences->modifiers & _inputmod_interchange_run_walk);
	d.add(always_run_w);
	w_toggle *always_swim_w = new w_toggle("Always Swim", input_preferences->modifiers & _inputmod_interchange_swim_sink);
	d.add(always_swim_w);
	w_toggle *weapon_w = new w_toggle("Auto-Switch Weapons", !(input_preferences->modifiers & _inputmod_dont_switch_to_new_weapon));
	d.add(weapon_w);
#ifdef DC
	// Analog stick sensitivity. Two axes because turning and looking want very
	// different rates: Marathon's vertical range is small, so a comfortable
	// yaw speed feels twitchy applied to pitch.
	d.add(new w_spacer());
	w_slider *sens_h_w = new w_slider("Turn Sensitivity",
		NUMBER_OF_SENS_LEVELS,
		(input_preferences->sens_horizontal - SENS_MINIMUM) / SENS_STEP);
	d.add(sens_h_w);
	w_slider *sens_v_w = new w_slider("Look Sensitivity",
		NUMBER_OF_SENS_LEVELS,
		(input_preferences->sens_vertical - SENS_MINIMUM) / SENS_STEP);
	d.add(sens_v_w);
#endif
	d.add(new w_spacer());
#ifdef DC
	w_button *pad_b = new w_button("CONFIGURE CONTROLLER", pad_dialog, &d);
	d.add(pad_b);

	/*
	 *	Half of these describe engine behaviour that is not guessable from the
	 *	label. Always Swim and Run/Swim interact; the two sensitivities are
	 *	separate for a reason; and the stick mode changes which button fires.
	 */
	w_explain *explain = new w_explain;

	dc_explain_begin(explain);
	dc_explain_add(stick_mode_w, "Look: the stick turns and looks. Move: it "
	                             "walks and turns, and Look Up and Look Down "
	                             "aim.");
	dc_explain_add(invert_mouse_w, "Pushing the stick up looks down instead.");
	dc_explain_add(always_run_w, "Run without holding the run button. The button "
	                             "then makes you walk.");
	dc_explain_add(always_swim_w, "The same, in liquid. Run/Swim is one button, "
	                              "so this and Always Run affect each other.");
	dc_explain_add(weapon_w, "Switch to a better weapon when you pick one up.");
	dc_explain_add(sens_h_w, "How fast the stick turns you.");
	dc_explain_add(sens_v_w, "How fast it looks up and down. Separate because "
	                         "Marathon's vertical range is small, so a good turn "
	                         "speed feels twitchy on pitch.");
	dc_explain_add(pad_b, "Bind every action to whichever button you want.");

	d.add(new w_spacer());
	d.add(explain);
	dc_explain_arm(&d);
#else
	d.add(new w_button("CONFIGURE KEYBOARD", keyboard_dialog, &d));
#endif
	d.add(new w_spacer());
	d.add(new w_left_button("ACCEPT", dialog_ok, &d));
	d.add(new w_right_button("CANCEL", dialog_cancel, &d));

	// Clear screen
	clear_screen();

	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;

#ifdef DC
		// One choice, two fields. dc_stick_mode is what the pad driver reads;
		// input_device is what decides whether the analog look path runs at
		// all. They must never disagree, so they are set together here and
		// nowhere else.
		int mode = stick_mode_w->get_selection() ? DC_STICK_MOVE : DC_STICK_LOOK;
		int device = (mode == DC_STICK_MOVE) ? _keyboard_or_game_pad : _mouse_yaw_pitch;

		if (mode != input_preferences->dc_stick_mode) {
			input_preferences->dc_stick_mode = mode;
			changed = true;
		}
#else
		int device = mouse_w->get_selection();
#endif
		if (device != input_preferences->input_device) {
			input_preferences->input_device = device;
			changed = true;
		}

		uint16 flags = 0;
		if (always_run_w->get_selection()) flags |= _inputmod_interchange_run_walk;
		if (always_swim_w->get_selection()) flags |= _inputmod_interchange_swim_sink;
		if (!(weapon_w->get_selection())) flags |= _inputmod_dont_switch_to_new_weapon;
		if (invert_mouse_w->get_selection()) flags |= _inputmod_invert_mouse;

		if (flags != input_preferences->modifiers) {
			input_preferences->modifiers = flags;
			changed = true;
		}

#ifdef DC
		int sh = SENS_MINIMUM + sens_h_w->get_selection() * SENS_STEP;
		int sv = SENS_MINIMUM + sens_v_w->get_selection() * SENS_STEP;
		if (sh != input_preferences->sens_horizontal) {
			input_preferences->sens_horizontal = sh;
			changed = true;
		}
		if (sv != input_preferences->sens_vertical) {
			input_preferences->sens_vertical = sv;
			changed = true;
		}

		// The stick mode above only reaches the pad driver through here.
		dc_apply_pad_bindings();
#endif

		if (changed)
			write_preferences();
	}

#ifdef DC
	dc_explain_end();
#endif
}


/*
 *  Keyboard dialog
 */

const int NUM_KEYS = 20;

static const char *action_name[NUM_KEYS] = {
	"Move Forward", "Move Backward", "Turn Left", "Turn Right", "Sidestep Left", "Sidestep Right",
	"Glance Left", "Glance Right", "Look Up", "Look Down", "Look Ahead",
	"Previous Weapon", "Next Weapon", "Trigger", "2nd Trigger",
	"Sidestep", "Run/Swim", "Look",
	"Action", "Auto Map"
};

static SDLKey default_keys[NUM_KEYS] = {
	SDLK_KP8, SDLK_KP5, SDLK_KP4, SDLK_KP6,		// moving/turning
	SDLK_z, SDLK_x,								// sidestepping
	SDLK_a, SDLK_s,								// horizontal looking
	SDLK_d, SDLK_c, SDLK_v,						// vertical looking
	SDLK_KP7, SDLK_KP9,							// weapon cycling
	SDLK_SPACE, SDLK_LALT,						// weapon trigger
	SDLK_LSHIFT, SDLK_LCTRL, SDLK_LMETA,		// modifiers
	SDLK_TAB,									// action trigger
	SDLK_m										// map
};

static SDLKey default_mouse_keys[NUM_KEYS] = {
	SDLK_w, SDLK_x, SDLK_LEFT, SDLK_RIGHT,		// moving/turning
	SDLK_a, SDLK_d,								// sidestepping
	SDLK_q, SDLK_e,								// horizontal looking
	SDLK_UP, SDLK_DOWN, SDLK_KP0,				// vertical looking
	SDLK_c, SDLK_z,								// weapon cycling
	SDLK_RCTRL, SDLK_SPACE,						// weapon trigger
	SDLK_RSHIFT, SDLK_LSHIFT, SDLK_LCTRL,		// modifiers
	SDLK_s,										// action trigger
	SDLK_TAB									// map
};

class w_prefs_key;

static w_prefs_key *key_w[NUM_KEYS];

class w_prefs_key : public w_key {
public:
	w_prefs_key(const char *name, SDLKey key) : w_key(name, key) {}

	void set_key(SDLKey new_key)
	{
		// Key used for in-game function?
		int error = NONE;
		switch (new_key) {
			case SDLK_PERIOD:		// Sound volume up/down
			case SDLK_COMMA:
				error = keyIsUsedForSound;
				break;

			case SDLK_EQUALS:		// Map zoom
			case SDLK_MINUS:
				error = keyIsUsedForMapZooming;
				break;

			case SDLK_LEFTBRACKET:	// Inventory scrolling
			case SDLK_RIGHTBRACKET:
				error = keyIsUsedForScrolling;
				break;

			case SDLK_BACKSPACE:
			case SDLK_BACKSLASH:
			case SDLK_F1:
			case SDLK_F2:
			case SDLK_F3:
			case SDLK_F4:
			case SDLK_F5:
			case SDLK_F6:
			case SDLK_F7:
			case SDLK_F8:
			case SDLK_F9:
			case SDLK_F10:
			case SDLK_F11:
			case SDLK_F12:
				error = keyIsUsedAlready;
				break;

			default:
				break;
		}
		if (error != NONE) {
			alert_user(infoError, strERRORS, error, 0);
			return;
		}

		w_key::set_key(new_key);
		if (new_key == SDLK_UNKNOWN)
			return;

		// Remove binding to this key from all other widgets
		for (int i=0; i<NUM_KEYS; i++) {
			if (key_w[i] && key_w[i] != this && key_w[i]->get_key() == new_key) {
				key_w[i]->set_key(SDLK_UNKNOWN);
				key_w[i]->dirty = true;
			}
		}
	}
};

static void load_default_keys(void *arg)
{
	// Load default keys, depending on state of "Mouse control" widget
	dialog *d = (dialog *)arg;
	SDLKey *keys = ((mouse_w && mouse_w->get_selection()) ? default_mouse_keys : default_keys);
	for (int i=0; i<NUM_KEYS; i++)
		key_w[i]->set_key(keys[i]);
	d->draw();
}

static void keyboard_dialog(void *arg)
{
	dialog *parent = (dialog *)arg;

	// Clear array of key widgets (because w_prefs_key::set_key() scans it)
	for (int i=0; i<NUM_KEYS; i++)
		key_w[i] = NULL;

	// Create dialog
	dialog d;
	d.add(new w_static_text("CONFIGURE KEYBOARD", TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
	for (int i=0; i<NUM_KEYS; i++) {
		key_w[i] = new w_prefs_key(action_name[i], SDLKey(input_preferences->keycodes[i]));
		d.add(key_w[i]);
	}
	d.add(new w_spacer());
	d.add(new w_button("DEFAULTS", load_default_keys, &d));
	d.add(new w_spacer());
	d.add(new w_left_button("ACCEPT", dialog_ok, &d));
	d.add(new w_right_button("CANCEL", dialog_cancel, &d));

	// Clear screen
	clear_screen();

	// Run dialog
	if (d.run() == 0) {	// Accepted
		bool changed = false;

		for (int i=0; i<NUM_KEYS; i++) {
			SDLKey key = key_w[i]->get_key();
			if (key != input_preferences->keycodes[i]) {
				input_preferences->keycodes[i] = short(key);
				changed = true;
			}
		}

		set_keys(input_preferences->keycodes);
		if (changed)
			write_preferences();
	}
}


#ifdef DC
/*
 *  Configure controller
 *
 *  Takes the place of CONFIGURE KEYBOARD, which offers a device this console
 *  does not have. Every action the engine knows is here, over two pages: the
 *  ones worth having on a pad, and an ADVANCED page for the rest.
 *
 *  The split is not cosmetic. There are twenty actions and eleven buttons, so
 *  some actions cannot have one, and a screen that presents all twenty as
 *  equals invites the player to spend buttons on Glance Left before Trigger.
 *  Turning is on the advanced page for the same reason -- the stick owns it in
 *  both modes, so a button for it is a preference, not a necessity.
 *
 *  Bindings are edited in pad_work[] rather than in preferences directly, so
 *  CANCEL on either page really does cancel, and so a button taken from one
 *  page can be cleared on the other.
 */

static int16 pad_work[NUMBER_OF_KEYS];

class w_prefs_pad_key;
static w_prefs_pad_key *pad_w[NUMBER_OF_KEYS];

// Which actions appear on each page, as engine action indices.
static const int pad_main_actions[] = {
	0, 1,			// Move Forward, Move Backward
	4, 5,			// Sidestep Left, Sidestep Right
	8, 9, 10,		// Look Up, Look Down, Look Ahead
	11, 12,			// Previous Weapon, Next Weapon
	13, 14,			// Trigger, 2nd Trigger
	18, 19			// Action, Auto Map
};

static const int pad_advanced_actions[] = {
	2, 3,			// Turn Left, Turn Right
	6, 7,			// Glance Left, Glance Right
	15, 16, 17		// Sidestep, Run/Swim, Look
};

#define NUM_PAD_MAIN		(int)(sizeof(pad_main_actions) / sizeof(pad_main_actions[0]))
#define NUM_PAD_ADVANCED	(int)(sizeof(pad_advanced_actions) / sizeof(pad_advanced_actions[0]))

/*
 *	An action on neither page can never be bound and nothing would say so, which
 *	is the kind of omission that survives for months. The two lists have no
 *	duplicates, so counting them is enough to prove they cover all twenty.
 *	C++98, so this is the array-size trick rather than static_assert.
 */
typedef char pad_pages_cover_every_action[
	(NUM_PAD_MAIN + NUM_PAD_ADVANCED == NUM_KEYS) ? 1 : -1];

class w_prefs_pad_key : public w_pad_key {
public:
	w_prefs_pad_key(const char *name, int a)
		: w_pad_key(name, pad_work[a]), action(a) {}

	void set_button(int id)
	{
		// A button doing two things at once is never what was meant, so taking
		// it for this action releases it from whichever action had it. The
		// scan is over pad_work rather than over the widgets, so it reaches
		// actions listed on the other page.
		if (id > 0) {
			for (int i = 0; i < NUMBER_OF_KEYS; i++) {
				if (i == action || pad_work[i] != id)
					continue;

				pad_work[i] = 0;
				if (pad_w[i]) {
					pad_w[i]->w_pad_key::set_button(0);
					pad_w[i]->dirty = true;
				}
			}
		}

		pad_work[action] = (int16)id;
		w_pad_key::set_button(id);
	}

private:
	int action;
};

static void load_default_pad(void *arg)
{
	dialog *d = (dialog *)arg;

	dc_input_default_bindings(pad_work, NUMBER_OF_KEYS);

	for (int i = 0; i < NUMBER_OF_KEYS; i++)
		if (pad_w[i])
			pad_w[i]->w_pad_key::set_button(pad_work[i]);

	d->draw();
}

// Both pages are the same screen with a different action list, so they share
// one builder. Only the main page carries ADVANCED and DEFAULTS.
static void pad_page(const char *title, const int *actions, int count, bool main_page)
{
	for (int i = 0; i < NUMBER_OF_KEYS; i++)
		pad_w[i] = NULL;

	dialog d;
	d.add(new w_static_text(title, TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());

	for (int i = 0; i < count; i++) {
		int a = actions[i];
		pad_w[a] = new w_prefs_pad_key(action_name[a], a);
		d.add(pad_w[a]);
	}

	d.add(new w_spacer());
	if (main_page) {
		d.add(new w_button("ADVANCED", pad_advanced_dialog, &d));
		d.add(new w_button("DEFAULTS", load_default_pad, &d));
		d.add(new w_spacer());
	}
	d.add(new w_left_button("ACCEPT", dialog_ok, &d));
	d.add(new w_right_button("CANCEL", dialog_cancel, &d));

	clear_screen();

	// CANCEL abandons this page's edits by putting back what pad_work held on
	// the way in. The other page's edits are not this page's to discard.
	int16 entry[NUMBER_OF_KEYS];
	for (int i = 0; i < NUMBER_OF_KEYS; i++)
		entry[i] = pad_work[i];

	if (d.run() != 0)
		for (int i = 0; i < count; i++)
			pad_work[actions[i]] = entry[actions[i]];

	for (int i = 0; i < NUMBER_OF_KEYS; i++)
		pad_w[i] = NULL;
}

static void pad_advanced_dialog(void *arg)
{
	dialog *parent = (dialog *)arg;

	pad_page("ADVANCED CONTROLS", pad_advanced_actions, NUM_PAD_ADVANCED, false);

	parent->draw();
}

static void pad_dialog(void *arg)
{
	dialog *parent = (dialog *)arg;

	for (int i = 0; i < NUMBER_OF_KEYS; i++)
		pad_work[i] = input_preferences->dc_pad_bindings[i];

	pad_page("CONFIGURE CONTROLLER", pad_main_actions, NUM_PAD_MAIN, true);

	bool changed = false;
	for (int i = 0; i < NUMBER_OF_KEYS; i++)
		if (pad_work[i] != input_preferences->dc_pad_bindings[i]) {
			input_preferences->dc_pad_bindings[i] = pad_work[i];
			changed = true;
		}

	// Push regardless: the stick mode may have moved even if no button did.
	dc_apply_pad_bindings();

	if (changed)
		write_preferences();

	parent->draw();
}
#endif


/*
 *  Environment dialog
 */

// Find available themes in directory and append to vector
class FindThemes : public FileFinder {
public:
	FindThemes(vector<FileSpecifier> &v) : dest_vector(v) {dest_vector.clear();}

private:
	bool found(FileSpecifier &file)
	{
		// Look for "theme.mml" files
		string base, part;
		file.SplitPath(base, part);
		if (part == "theme.mml")
			dest_vector.push_back(base);
		return false;
	}

	vector<FileSpecifier> &dest_vector;
};

// Environment item
class env_item {
public:
	env_item() : indent(0), selectable(false)
	{
		name[0] = 0;
	}

	env_item(const FileSpecifier &fs, int i, bool sel) : spec(fs), indent(i), selectable(sel)
	{
		spec.GetName(name);
	}

	FileSpecifier spec;	// Specifier of associated file
	char name[256];		// Last part of file name
	int indent;			// Indentation level
	bool selectable;	// Flag: item refers to selectable file (otherwise to directory name)
};

// Environment file list widget
class w_env_list : public w_list<env_item> {
public:
	w_env_list(const vector<env_item> &items, const char *selection, dialog *d) : w_list<env_item>(items, 400, 15, 0), parent(d)
	{
		vector<env_item>::const_iterator i, end = items.end();
		int num = 0;
		for (i = items.begin(); i != end; i++, num++) {
			if (strcmp(i->spec.GetPath(), selection) == 0) {
				set_selection(num);
				break;
			}
		}
	}

	bool is_item_selectable(int i)
	{
		return items[i].selectable;
	}

	void item_selected(void)
	{
		parent->quit(0);
	}

	void draw_item(vector<env_item>::const_iterator i, SDL_Surface *s, int x, int y, int width, bool selected) const
	{
		y += font->get_ascent();

		int color;
		if (i->selectable) {
			color = selected ? ITEM_ACTIVE_COLOR : ITEM_COLOR;
		} else
			color = LABEL_COLOR;

		set_drawing_clip_rectangle(0, x, s->h, x + width);
		draw_text(s, i->name, x + i->indent * 8, y, get_dialog_color(color), font, style);
		set_drawing_clip_rectangle(SHRT_MIN, SHRT_MIN, SHRT_MAX, SHRT_MAX);
	}

private:
	dialog *parent;
};

// Environment selection button
class w_env_select : public w_select_button {
public:
	w_env_select(const char *name, const char *path, const char *m, int t, dialog *d) : w_select_button(name, item_name, select_item_callback, this), parent(d), menu_title(m), type(t)
	{
		set_path(path);
	}
	~w_env_select() {}

	void set_path(const char *p)
	{
		item = p;
		item.GetName(item_name);
		set_selection(item_name);
	}

	const char *get_path(void) const
	{
		return item.GetPath();
	}

	FileSpecifier &get_file_specifier(void)
	{
		return item;
	}

private:
	void select_item(dialog *parent);
	static void select_item_callback(void *arg)
	{
		w_env_select *obj = (w_env_select *)arg;
		obj->select_item(obj->parent);
	}

	dialog *parent;
	const char *menu_title;	// Selection menu title

	FileSpecifier item;		// File specification
	int type;				// File type
	char item_name[256];	// File name (excluding directory part)
};

void w_env_select::select_item(dialog *parent)
{
	// Find available files
	vector<FileSpecifier> files;
	if (type == _typecode_theme) {

		// Theme, find by theme script
		FindThemes finder(files);
		vector<DirectorySpecifier>::const_iterator i = data_search_path.begin(), end = data_search_path.end();
		while (i != end) {
			FileSpecifier dir = *i + "Themes";
			finder.Find(dir, WILDCARD_TYPE);
			i++;
		}

	} else {

		// Map/phyics/shapes/sounds, find by type
		FindAllFiles finder(files);
		vector<DirectorySpecifier>::const_iterator i = data_search_path.begin(), end = data_search_path.end();
		while (i != end) {
			FileSpecifier dir = *i;
			finder.Find(dir, type);
			i++;
		}
	}

	// Create structured list of files
	vector<env_item> items;
	vector<FileSpecifier>::const_iterator i = files.begin(), end = files.end();
	string last_base;
	int indent_level = 0;
	for (i = files.begin(); i != end; i++) {
		string base, part;
		i->SplitPath(base, part);
		if (base != last_base) {

			// New directory
			FileSpecifier base_spec = base;
//			if (base_spec != global_dir && base_spec != local_dir) {

				// Subdirectory, insert name as unselectable item, put items on indentation level 1
				items.push_back(env_item(base_spec, 0, false));
				indent_level = 1;

//			} else {
//
//				// Top-level directory, put items on indentation level 0
//				indent_level = 0;
//			}
			last_base = base;
		}
		items.push_back(env_item(*i, indent_level, true));
	}

	// Create dialog
	dialog d;
	d.add(new w_static_text(menu_title, TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
	w_env_list *list_w = new w_env_list(items, item.GetPath(), &d);
	d.add(list_w);
	d.add(new w_spacer());
	d.add(new w_button("CANCEL", dialog_cancel, &d));

	// Clear screen
	clear_screen();

	// Run dialog
	if (d.run() == 0) { // Accepted
		if (items.size())
			set_path(items[list_w->get_selection()].spec.GetPath());
	}
}

static void environment_dialog(void *arg)
{
	dialog *parent = (dialog *)arg;

	// Create dialog
	dialog d;
	d.add(new w_static_text("ENVIRONMENT SETTINGS", TITLE_FONT, TITLE_COLOR));
	d.add(new w_spacer());
	w_env_select *map_w = new w_env_select("Map", environment_preferences->map_file, "AVAILABLE MAPS", _typecode_scenario, &d);
	d.add(map_w);
	w_env_select *physics_w = new w_env_select("Physics", environment_preferences->physics_file, "AVAILABLE PHYSICS MODELS", _typecode_physics, &d);
	d.add(physics_w);
	w_env_select *shapes_w = new w_env_select("Shapes", environment_preferences->shapes_file, "AVAILABLE SHAPES", _typecode_shapes, &d);
	d.add(shapes_w);
	w_env_select *sounds_w = new w_env_select("Sounds", environment_preferences->sounds_file, "AVAILABLE SOUNDS", _typecode_sounds, &d);
	d.add(sounds_w);
	w_env_select *theme_w = new w_env_select("Theme", environment_preferences->theme_dir, "AVAILABLE THEMES", _typecode_theme, &d);
	d.add(theme_w);
	d.add(new w_spacer());
	d.add(new w_left_button("ACCEPT", dialog_ok, &d));
	d.add(new w_right_button("CANCEL", dialog_cancel, &d));

	// Clear screen
	clear_screen();

	// Run dialog
	bool theme_changed = false;
	if (d.run() == 0) {	// Accepted
		bool changed = false;

		const char *path = map_w->get_path();
		if (strcmp(path, environment_preferences->map_file)) {
			strcpy(environment_preferences->map_file, path);
			environment_preferences->map_checksum = read_wad_file_checksum(map_w->get_file_specifier());
			changed = true;
		}

		path = physics_w->get_path();
		if (strcmp(path, environment_preferences->physics_file)) {
			strcpy(environment_preferences->physics_file, path);
			environment_preferences->physics_checksum = read_wad_file_checksum(physics_w->get_file_specifier());
			changed = true;
		}

		path = shapes_w->get_path();
		if (strcmp(path, environment_preferences->shapes_file)) {
			strcpy(environment_preferences->shapes_file, path);
			environment_preferences->shapes_mod_date = shapes_w->get_file_specifier().GetDate();
			changed = true;
		}

		path = sounds_w->get_path();
		if (strcmp(path, environment_preferences->sounds_file)) {
			strcpy(environment_preferences->sounds_file, path);
			environment_preferences->sounds_mod_date = sounds_w->get_file_specifier().GetDate();
			changed = true;
		}

		path = theme_w->get_path();
		if (strcmp(path, environment_preferences->theme_dir)) {
			strcpy(environment_preferences->theme_dir, path);
			changed = theme_changed = true;
		}

		if (changed)
			load_environment_from_preferences();

		if (theme_changed) {
			FileSpecifier theme = environment_preferences->theme_dir;
			load_theme(theme);
		}

		if (changed || theme_changed)
			write_preferences();
	}

	// Redraw parent dialog
	if (theme_changed)
		parent->quit(0);	// Quit the parent dialog so it won't draw in the old theme
}

#endif
