/*
 *	dc_compat.c -- Dreamcast compatibility shims for Aleph One 0.12.0
 *
 *	BERO's 2002 port shipped dc/syscalls.c and dc/fs_mem.c against KallistiOS
 *	1.1.7. Neither can be used as-is on a modern KOS:
 *
 *	  - syscalls.c defines _read, _write, _open, _close, _lseek, _fstat, _stat,
 *	    _sbrk, _exit and friends. Modern KOS supplies all of those through
 *	    newlib, so linking the 2002 file produces duplicate symbols.
 *
 *	  - fs_mem.c hand-rolled an in-RAM filesystem mounted at /mem, because KOS
 *	    1.1.7 had none. Modern KOS ships exactly that as fs_ramdisk, mounted at
 *	    /ram.
 *
 *	So this file provides only the two things that are genuinely still missing.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <kos/fs_ramdisk.h>
#include <dc/video.h>
#include <dc/biosfont.h>

/*
 *	dc_trace -- draw a line of text straight into video RAM.
 *
 *	There is no serial console under Flycast and KOS here has no framebuffer
 *	dbgio device, so printf goes nowhere we can see. This writes with bfont
 *	directly to vram_s, which is the same memory SDL's Dreamcast driver uses as
 *	its framebuffer in the non-textured path (current->pixels = vram_l).
 *
 *	That makes it useful precisely when the screen is black: whatever the game
 *	failed to draw, this still lands on screen and can be screenshotted.
 *
 *	`slot` is a line number so successive calls stack rather than overwrite.
 */
/*
 *	Tracing is off unless the disc carries a DEBUG file, the same marker trick as
 *	AUTOSTART and PADTEST. Gating here rather than at the call sites means one
 *	switch covers every trace in the port, and a release image stays silent
 *	without any of them being edited or removed.
 *
 *	`make test` stages the marker; `cdi` and `gdi` never do.
 */
static int dc_trace_enabled(void)
{
	static int checked = 0, enabled = 0;

	if (!checked) {
		checked = 1;
		enabled = (access("/cd/AlephOne/DEBUG", 4) == 0);
	}

	return enabled;
}

void dc_trace(int slot, const char *fmt, ...)
{
	char buf[128];
	va_list ap;
	int y;

	if (!dc_trace_enabled())
		return;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	// Serial first: KOS's printf goes out the SCIF port, which Flycast surfaces
	// when "Serial Console" is enabled. That survives the screen being wrong,
	// and unlike the framebuffer it cannot be overdrawn by the game.
	printf("[dctrace %d] %s\n", slot, buf);
	fflush(stdout);

	// Inset for overscan: a television eats the edges, and traces drawn 8 pixels
	// from the corner were visible on a real set but unreadable. Inert unless the
	// disc carries DEBUG, since dc_trace returns before this point without it.
	y = 40 + slot * 24;
	if(y < 0 || y > 424)
		return;

	bfont_draw_str(vram_s + y * 640 + 40, 640, 0, buf);
}

/*
 *	Aleph One needs somewhere writable for preferences, saved games and film
 *	recordings. The game itself runs from /cd, which is read-only, so shell_sdl.cpp
 *	points local_data_dir at the ramdisk.
 *
 *	Note this means preferences and saves do not survive a power cycle. BERO had
 *	the same limitation in 2002 (his README lists VMU support as not implemented).
 *	Writing them through to the VMU is a worthwhile later addition.
 */
int fs_mem_init(void)
{
	fs_ramdisk_init();
	return 0;
}

/*
 *	access() is not in KOS's newlib. FileHandler_SDL.cpp uses it in exactly one
 *	place, to test readability before opening a file, so testing existence via
 *	stat() is a faithful enough stand-in on a console with no permission model.
 */
int access(const char *path, int mode)
{
	struct stat st;

	(void)mode;

	if (stat(path, &st) < 0)
		return -1;

	return 0;
}

/*
 *	dc_build_stamp -- draw the build tag in the corner of the main menu.
 *
 *	Not gated on DEBUG: knowing which build is running matters most on a normal
 *	image, which is exactly the one a player is giving feedback about. Drawn
 *	every pass rather than once, because the menu redraws its buttons and would
 *	otherwise erase it.
 */
void dc_build_stamp(const char *tag)
{
	bfont_draw_str(vram_s + 452 * 640 + 8, 640, 0, (char *)tag);
}
