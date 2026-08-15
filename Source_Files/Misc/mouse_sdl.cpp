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

// Base sensitivity at 100%, set from the engine's own limit rather than by
// feel. mask_in_absolute_positioning_information() encodes the turn as
//
//     encoded = (delta_yaw >> (16 - ABSOLUTE_YAW_BITS)) + MAXIMUM_ABSOLUTE_YAW/2
//     encoded = PIN(encoded, 0, MAXIMUM_ABSOLUTE_YAW - 1)
//
// With ABSOLUTE_YAW_BITS = 7 that saturates at delta_yaw = 32767, i.e.
// FIXED_ONE/2. Anything larger is thrown away.
//
// This was previously FIXED_ONE, so full stick produced 65536 at 100% and
// 32768 at 50% -- both above the clamp. The whole top half of the slider did
// nothing and the outer edge of the stick was pinned at maximum turn rate,
// which is exactly how it felt on hardware.
//
// FIXED_ONE/2 puts full deflection at 100% right at the top of the usable
// range, so the slider now maps linearly onto real turn rate across its whole
// travel. Pitch stays half of yaw; Marathon's vertical range is small.
#define DC_YAW_SCALE    (FIXED_ONE / 2)
#define DC_PITCH_SCALE  (FIXED_ONE / 4)
#define DC_STICK_MAX    128
// Deflection at or beyond this is treated as full. The outer part of the stick
// travel then all maps to the same rate, instead of continuing to climb.
//
// Reported from hardware: even at a usable overall sensitivity the stick was
// "much too sensitive on the outside range". That is the squared curve doing
// exactly the wrong thing at the rim -- the slope of x^2 is steepest at full
// deflection, so the smallest nudge there swings the rate hardest, which is
// where a thumb has least precision. Saturating early flattens it.
#define DC_STICK_SAT    100
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

		// Squared response, the same shape the mouse path uses. A linear map
		// made the stick unusable on hardware: every small nudge got the full
		// turn rate. Squaring keeps the rim fast while giving fine control
		// around centre, which is where aiming actually happens.
		long ax = (sx < 0) ? -sx : sx;
		long ay = (sy < 0) ? -sy : sy;

		if (ax > DC_STICK_SAT) ax = DC_STICK_SAT;
		if (ay > DC_STICK_SAT) ay = DC_STICK_SAT;

		long cx = (ax * ax) / DC_STICK_SAT;		// 0..DC_STICK_SAT, quadratic
		long cy = (ay * ay) / DC_STICK_SAT;
		if (sx < 0) cx = -cx;
		if (sy < 0) cy = -cy;

		_fixed vx = (_fixed)((cx * (long)(DC_YAW_SCALE / DC_STICK_SAT) * sh) / 100);
		_fixed vy = (_fixed)((-cy * (long)(DC_PITCH_SCALE / DC_STICK_SAT) * sv) / 100);

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

		// Consume it. execute_timer_tasks() calls mouse_idle() once per pass
		// but runs the game tick N times to catch up, and each tick calls
		// test_mouse. Without this the same deflection is applied N times and
		// the turn rate scales with how far behind the renderer is -- which is
		// why hardware at 20fps turned roughly twenty times faster than the
		// emulator at 30, for identical code and settings. Upstream's mouse
		// path zeroes here for exactly this reason; omitting it was the bug.
		snapshot_delta_yaw = snapshot_delta_pitch = snapshot_delta_velocity = 0;
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
