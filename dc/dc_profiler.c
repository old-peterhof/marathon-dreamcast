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
#include <stddef.h>
#include <stdint.h>
#include <kos/thread.h>
#include <arch/timer.h>

#include "vendor/vmu_profiler.h"

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
static uint64_t dc_last_ms = 0;
static float dc_fps = 0.0f;

static void dc_update_fps(vmu_profiler_measurement_t *m)
{
	uint64_t now = timer_ms_gettime64();
	unsigned frames = dc_frames;

	dc_frames = 0;

	if (dc_last_ms && now > dc_last_ms)
		dc_fps = (frames * 1000.0f) / (float)(now - dc_last_ms);

	dc_last_ms = now;
	m->fstorage = dc_fps;
}

void dc_profiler_start(void)
{
	vmu_profiler_t *prof;

	if (access("/cd/AlephOne/PROFILE", 4) != 0)
		return;

	/* Default configuration: the profiler picks its own thread priority,
	   polling interval and frame-averaging window, all of which are sensible
	   for simply watching a framerate. */
	prof = vmu_profiler_start(NULL);
	if (!prof)
		return;

	vmu_profiler_add_measure(prof,
		init_measurement("FPS", use_float, dc_update_fps, NULL));

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
