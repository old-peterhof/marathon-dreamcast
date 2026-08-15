/*
	dc_trace.cpp

	Startup tracing for Dreamcast, by painting the whole framebuffer a solid
	colour and holding it long enough to see.

	Every other diagnostic tried on this port has failed for the same reason:
	printf goes out the SCIF serial port, which needs a coder's cable, and a
	GDEMU offers no return channel. Drawing through SDL is no better -- the bug
	under investigation is that SDL draws nothing.

	KOS's vid_clear() writes straight to video memory with no SDL involvement,
	so it works whether or not SDL is healthy. The last colour seen before the
	screen goes black says how far startup got.

	Enabled with -DDC_TRACE_ENABLED (./build.sh trace). Compiled out otherwise.
*/

#ifdef DC_TRACE_ENABLED

#include "dc_trace.h"

#include <dc/video.h>
#include <kos/thread.h>

// Long enough to read off a CRT without being tedious across seven stops.
#define DC_TRACE_HOLD_MS 1200

void DC_Trace(int step)
{
	unsigned char r = 0, g = 0, b = 0;

	switch (step) {
		case DC_TRACE_MAIN_ENTERED:   r = 255; g =   0; b =   0; break; // red
		case DC_TRACE_ARGS_PARSED:    r = 255; g = 128; b =   0; break; // orange
		case DC_TRACE_SDL_INIT_OK:    r = 255; g = 255; b =   0; break; // yellow
		case DC_TRACE_DC_INIT_OK:     r =   0; g = 255; b =   0; break; // green
		case DC_TRACE_VIDEO_MODE_OK:  r =   0; g = 255; b = 255; break; // cyan
		case DC_TRACE_APP_INIT_DONE:  r =   0; g =   0; b = 255; break; // blue
		case DC_TRACE_EVENT_LOOP:     r = 255; g =   0; b = 255; break; // magenta
		default:                      r = 128; g = 128; b = 128; break; // grey
	}

	vid_clear(r, g, b);
	thd_sleep(DC_TRACE_HOLD_MS);
}

#endif /* DC_TRACE_ENABLED */
