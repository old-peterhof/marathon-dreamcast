/*
 *  mouse_sdl.cpp - Mouse handling, SDL specific implementation
 */

#include "cseries.h"

#include "mouse.h"
#include "player.h"
#include "shell.h"
#include "preferences.h"

#ifdef DC
extern "C" {
int dc_input_analog_x(void);
int dc_input_analog_y(void);
int dc_input_trigger_l(void);
int dc_input_trigger_r(void);
}

// Base sensitivity at 100%, measured rather than guessed. FIXED_ONE/4 gives
// 63 angle units/sec at full deflection, which is only 44 deg/sec -- far too
// slow to aim with. Four times that lands near 176 deg/sec, a normal maximum
// turn rate for a console shooter. Pitch is half of yaw: Marathon's vertical
// range is small, so the same rate feels twitchy.
//
// Both are then scaled by the player's Turn/Look Sensitivity preference, so
// this constant only sets what 100% means.
#define DC_YAW_SCALE    (FIXED_ONE)
#define DC_PITCH_SCALE  (FIXED_ONE / 2)
#define DC_STICK_MAX    128
#define DC_TRIGGER_ON   64
#endif


// Global variables
static bool mouse_active = false;
static uint8 button_mask = 0;		// Mask of enabled buttons
static int center_x, center_y;		// X/Y center of screen
static _fixed snapshot_delta_yaw, snapshot_delta_pitch, snapshot_delta_velocity;


/*
 *  Initialize in-game mouse handling
 */

void enter_mouse(short type)
{
#ifdef DC
	// No pointer to grab and no cursor to warp: the analog stick supplies the
	// deltas. Just arm the path.
	if (type != _keyboard_or_game_pad) {
		mouse_active = true;
		snapshot_delta_yaw = snapshot_delta_pitch = snapshot_delta_velocity = 0;
		button_mask = 0;
	}
	return;
#endif
	if (type != _keyboard_or_game_pad) {
#ifndef DEBUG
		SDL_WM_GrabInput(SDL_GRAB_ON);
#endif
		SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
		mouse_active = true;
		snapshot_delta_yaw = snapshot_delta_pitch = snapshot_delta_velocity = 0;
		button_mask = 0;	// Disable all buttons (so a shot won't be fired if we enter the game with a mouse button down from clicking a GUI widget)
		recenter_mouse();
	}
}


/*
 *  Shutdown in-game mouse handling
 */

void exit_mouse(short type)
{
#ifdef DC
	if (type != _keyboard_or_game_pad)
		mouse_active = false;
	return;
#endif
	if (type != _keyboard_or_game_pad) {
#ifndef DEBUG
		SDL_WM_GrabInput(SDL_GRAB_OFF);
#endif
		SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
		mouse_active = false;
	}
}


/*
 *  Calculate new center mouse position when screen size has changed
 */

void recenter_mouse(void)
{
#ifdef DC
	return;		// nothing to recentre; the stick self-centres
#endif
	if (mouse_active) {
		SDL_Surface *s = SDL_GetVideoSurface();
		center_x = s->w / 2;
		center_y = s->h / 2;
		SDL_WarpMouse(center_x, center_y);
	}
}


/*
 *  Take a snapshot of the current mouse state
 */

void mouse_idle(short type)
{
#ifdef DC
	if (mouse_active) {
		// A stick reports a rate, not a displacement, so unlike the mouse path
		// there is nothing to divide by elapsed ticks: deflection maps straight
		// to a per-tick turn rate.
		int sx = dc_input_analog_x();
		int sy = dc_input_analog_y();

		long sh = input_preferences->sens_horizontal;
		long sv = input_preferences->sens_vertical;

		_fixed vx = (_fixed)((sx * (long)(DC_YAW_SCALE / 128) * sh) / 100);
		_fixed vy = (_fixed)((-sy * (long)(DC_PITCH_SCALE / 128) * sv) / 100);

		if (input_preferences->modifiers & _inputmod_invert_mouse)
			vy = -vy;

		snapshot_delta_yaw = vx;
		snapshot_delta_pitch = vy;
		snapshot_delta_velocity = 0;	// movement is on the face buttons
	}
	return;
#endif
	if (mouse_active) {
		static uint32 last_tick_count = 0;
		uint32 tick_count = SDL_GetTicks();
		int32 ticks_elapsed = tick_count - last_tick_count;

		if (ticks_elapsed < 1)
			return;

		int x, y;
		SDL_GetMouseState(&x, &y);
		SDL_WarpMouse(center_x, center_y);

		// Calculate axis deltas
		_fixed vx = ((x - center_x) << FIXED_FRACTIONAL_BITS) / ticks_elapsed;
		_fixed vy = -((y - center_y) << FIXED_FRACTIONAL_BITS) / ticks_elapsed;
		if (input_preferences->modifiers & _inputmod_invert_mouse)
			vy = -vy;

		// Pin and do nonlinearity
		vx = PIN(vx, -FIXED_ONE/2, FIXED_ONE/2); vx >>= 1; vx *= (vx<0) ? -vx : vx; vx >>= 14;
		vy = PIN(vy, -FIXED_ONE/2, FIXED_ONE/2); vy >>= 1; vy *= (vy<0) ? -vy : vy; vy >>= 13;

		// X axis = yaw
		snapshot_delta_yaw = vx;

		// Y axis = pitch or move, depending on mouse input type
		if (type == _mouse_yaw_pitch) {
			snapshot_delta_pitch = vy;
			snapshot_delta_velocity = 0;
		} else {
			snapshot_delta_pitch = 0;
			snapshot_delta_velocity = vy;
		}

		last_tick_count = tick_count;
	}
}


/*
 *  Return mouse state
 */

void test_mouse(short type, uint32 *flags, _fixed *delta_yaw, _fixed *delta_pitch, _fixed *delta_velocity)
{
#ifdef DC
	if (mouse_active) {
		if (dc_input_trigger_r() > DC_TRIGGER_ON)
			*flags |= _left_trigger_state;		// R: primary fire
		if (dc_input_trigger_l() > DC_TRIGGER_ON)
			*flags |= _right_trigger_state;		// L: alt fire

		*delta_yaw = snapshot_delta_yaw;
		*delta_pitch = snapshot_delta_pitch;
		*delta_velocity = snapshot_delta_velocity;
	}
	return;
#endif
	if (mouse_active) {
		uint8 buttons = SDL_GetMouseState(NULL, NULL);
		uint8 orig_buttons = buttons;
		buttons &= button_mask;				// Mask out disabled buttons
		if (buttons & SDL_BUTTON_LMASK)		// Left button: primary weapon trigger
			*flags |= _left_trigger_state;
		if (buttons & SDL_BUTTON_RMASK)		// Right button: secondary weapon trigger
			*flags |= _right_trigger_state;
		button_mask |= ~orig_buttons;		// A button must be released at least once to become enabled

		*delta_yaw = snapshot_delta_yaw;
		*delta_pitch = snapshot_delta_pitch;
		*delta_velocity = snapshot_delta_velocity;

		snapshot_delta_yaw = snapshot_delta_pitch = snapshot_delta_velocity = 0;
	}
}


/*
 *  Hide/show mouse pointer
 */

void hide_cursor(void)
{
	SDL_ShowCursor(0);
}

void show_cursor(void)
{
#ifdef DC
	// SDL's software cursor smears on Dreamcast: it blits itself but never
	// restores what was underneath, so every mouse movement leaves a trail of
	// arrows across the menu. Nothing here needs a pointer anyway -- the menu
	// is driven with UP/DOWN/RETURN -- so the cursor stays hidden.
	SDL_ShowCursor(0);
#else
	SDL_ShowCursor(1);
#endif
}


/*
 *  Get current mouse position
 */

void get_mouse_position(short *x, short *y)
{
	int mx, my;
	SDL_GetMouseState(&mx, &my);
	*x = mx;
	*y = my;
}


/*
 *  Mouse button still down?
 */

bool mouse_still_down(void)
{
	SDL_PumpEvents();
	Uint8 buttons = SDL_GetMouseState(NULL, NULL);
	return buttons & SDL_BUTTON_LMASK;
}
