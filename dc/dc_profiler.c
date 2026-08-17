/*
 *	dc_profiler.c -- real framerate from real hardware.
 *
 *	Every framerate number in this port before now came from Flycast, and
 *	Flycast lied: it held a steady 30 fps because that is the engine's tick cap,
 *	while a real Dreamcast renders the 640x320 software view at roughly 20. That
 *	produced a claim that had to be withdrawn, and there was no way to measure
 *	the difference without a console in the loop.
 *
 *	The VMU Profiler (Falco Girgis, written for the GTA3 Dreamcast port) solves
 *	that: a background thread that displays live statistics on the VMU's LCD, so
 *	numbers can be read off actual hardware while the game runs. Vendored in
 *	dc/vendor with its licence intact.
 *
 *	Gated on a PROFILE file on the disc, the same marker trick as AUTOSTART and
 *	DEBUG, and deliberately a separate marker from DEBUG: the bfont traces DEBUG
 *	enables draw over the 3D view, which is the last thing wanted while reading
 *	a framerate.
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <kos/thread.h>
#include <arch/timer.h>

#include "vendor/vmu_profiler.h"
#include "build_id.h"

extern void dc_trace(int slot, const char *fmt, ...);

static int profiling = 0;

/*
 *	Frames are counted here rather than read out of the profiler. The library
 *	ships a sample FPS callback but it is commented out and reaches for a
 *	fps_frames member the public struct does not have, so it would not build.
 *	Counting ourselves is both simpler and independent of the library's
 *	internals.
 *
 *	The counter is touched from the render loop and read from the profiler's
 *	background thread. A frame counted on one side of the boundary or the other
 *	shifts the average by a fraction of a frame, which does not matter for a
 *	number being read off a VMU screen.
 */
static volatile unsigned dc_frames = 0;
static uint64 dc_last_ms = 0;
static float dc_fps = 0.0f;

static void dc_update_fps(vmu_profiler_measurement_t *m)
{
	uint64 now = timer_ms_gettime64();
	unsigned frames = dc_frames;

	dc_frames = 0;

	if (dc_last_ms && now > dc_last_ms)
		dc_fps = (frames * 1000.0f) / (float)(now - dc_last_ms);

	dc_last_ms = now;
	m->fstorage = dc_fps;
}

/*
 *	Health and oxygen, pushed in from the render loop.
 *
 *	Read rather than fetched: this file is C and knows nothing about player_data,
 *	and the profiler's callback runs on its own thread where reaching into the
 *	game's state would be a race for no benefit. screen_sdl.cpp already calls in
 *	here once per rendered frame, so it carries the two numbers along.
 *
 *	-1 means "not in a game", and the line then renders as nothing at all rather
 *	than as zeroes -- a VMU showing HP0 on the main menu reads as a dead player.
 */
static volatile int dc_hp = -1;
static volatile int dc_air = -1;

void dc_profiler_set_vitals(int hp, int air_percent)
{
	static int traced = 0;

	dc_hp = hp;
	dc_air = air_percent;

	/* Once per boot, so the numbers can be confirmed sane from a log rather than
	   by squinting at a VMU in an emulator. */
	if (!traced && hp >= 0) {
		traced = 1;
		dc_trace(26, "vitals: hp=%d air=%d%%", hp, air_percent);
	}
}

/*
 *	One line for both, not two. The LCD is 48x32 pixels and the profiler allows
 *	five measurements; the title, the build number and the framerate already take
 *	three, and two more rows of text on a screen that size is not readable across
 *	a room, which is the whole point of putting it there.
 */
static void dc_line_vitals(vmu_profiler_measurement_t *m)
{
	int hp = dc_hp, air = dc_air;

	if (hp < 0) {
		m->sstorage[0] = 0;
		return;
	}

	snprintf(m->sstorage, sizeof m->sstorage, "\nHP%d A%d", hp, air);
}

/*
 *	The two static lines above the framerate. The profiler renders a use_string
 *	measurement's buffer verbatim, so these are free text rather than a label and
 *	a number; every line after the first starts with a newline.
 */
static void dc_line_title(vmu_profiler_measurement_t *m)
{
	strcpy(m->sstorage, "MARATHON 2");
}

static void dc_line_build(vmu_profiler_measurement_t *m)
{
	snprintf(m->sstorage, sizeof m->sstorage, "\nBuild %s", DC_BUILD_NUM);
}

void dc_profiler_start(void)
{
	vmu_profiler_t *prof;

	/*
	 *	Always on. Max wants the framerate readable on the VMU in every build,
	 *	not only in a profiling image, because a number read off the console beats
	 *	a number read off an emulator that runs at the engine's tick cap. It costs
	 *	a background thread and some framerate; that is the trade he asked for.
	 */

	/* Default configuration: the profiler picks its own thread priority,
	   polling interval and frame-averaging window, all of which are sensible
	   for simply watching a framerate. */
	prof = vmu_profiler_start(NULL);
	if (!prof)
		return;

	vmu_profiler_add_measure(prof,
		init_measurement("", use_string, dc_line_title, NULL));
	vmu_profiler_add_measure(prof,
		init_measurement("", use_string, dc_line_build, NULL));
	vmu_profiler_add_measure(prof,
		init_measurement("FPS", use_float, dc_update_fps, NULL));
	vmu_profiler_add_measure(prof,
		init_measurement("", use_string, dc_line_vitals, NULL));

	dc_last_ms = timer_ms_gettime64();

	profiling = 1;
}

/*
 *	Called once per rendered frame. The profiler counts frames itself and
 *	averages them; this just tells it a frame happened.
 */
void dc_profiler_frame(void)
{
	if (profiling) {
		dc_frames++;
		vmu_profiler_update();
	}
}

void dc_profiler_stop(void)
{
	if (profiling) {
		vmu_profiler_stop();
		profiling = 0;
	}
}
