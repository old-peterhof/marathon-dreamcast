/*
	dc_trace.h

	Startup tracing for Dreamcast. See dc_trace.cpp.

	Colour order, so the sequence can be read off the screen:

	  red      main() entered
	  orange   arguments parsed
	  yellow   SDL_Init returned
	  green    DC video + input init returned
	  cyan     first SDL_SetVideoMode returned a surface
	  blue     initialize_application() finished
	  magenta  entering the main event loop

	Whatever colour it stops on is the step that did not complete.
*/

#ifndef _DC_TRACE_H_
#define _DC_TRACE_H_

#ifdef DC_TRACE_ENABLED

enum {
	DC_TRACE_MAIN_ENTERED = 0,
	DC_TRACE_ARGS_PARSED,
	DC_TRACE_SDL_INIT_OK,
	DC_TRACE_DC_INIT_OK,
	DC_TRACE_VIDEO_MODE_OK,
	DC_TRACE_APP_INIT_DONE,
	DC_TRACE_EVENT_LOOP
};

void DC_Trace(int step);
#define DC_TRACE(step) DC_Trace(step)

#else
#define DC_TRACE(step) ((void)0)
#endif

#endif
