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
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <malloc.h>
#include <kos/fs.h>
#include <dc/pvr.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <kos/fs_ramdisk.h>
#include <dc/video.h>
#include <dc/biosfont.h>
#include <arch/timer.h>
#include <dc/maple.h>
#include <dc/maple/vmu.h>

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

	y = 8 + slot * 24;
	if(y < 0 || y > 456)
		return;

	bfont_draw_str(vram_s + y * 640 + 8, 640, 0, buf);
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

/*
 *	dc_heap_used -- how much of main RAM the heap has taken.
 *
 *	The Dreamcast has 16MB starting at 0x8c000000. sbrk(0) is the current top, so
 *	the difference from the start of the heap is what has been handed out plus
 *	whatever the allocator is holding back. Good enough to answer "who ate the
 *	memory", which is the question when the GL renderer dies with bad_alloc
 *	partway through the first frame.
 */
unsigned dc_heap_used(void)
{
	extern void *sbrk(int);
	static unsigned base = 0;
	unsigned now = (unsigned)(uintptr_t)sbrk(0);

	if (!base)
		base = now;

	return now - base;
}

/*
 *	Absolute heap top. dc_heap_used() is relative to whenever it was first
 *	called, which is after the collections load, so it cannot see the largest
 *	consumer in the game. This can.
 */
unsigned dc_heap_top(void)
{
	extern void *sbrk(int);
	return (unsigned)(uintptr_t)sbrk(0);
}

void dc_heap_trace(int slot, const char *where)
{
	dc_trace(slot, "heap: %-18s %u KB", where, dc_heap_used() / 1024);
}

/*
 *	dc_read_file_span -- the bytes at [offset, offset+length) of a file on the
 *	disc, in one GD-ROM transfer.
 *
 *	KOS's ISO9660 driver has two read paths. A request that starts on a sector
 *	boundary, lands in a 32-byte-aligned buffer and asks for at least 32 bytes
 *	goes out as one DMA stream. Anything else is served a sector at a time
 *	through a 16-block cache, one GD-ROM command per sector. stdio reads through
 *	its 1 KB buffer at whatever offset the parser is at, so every collection and
 *	every sound was taking the second path: 1352 sectors and 25.9 seconds for
 *	the first level's shapes, 13.7 more for its monster sounds, and the count
 *	of SDL calls made no difference because the driver was already coalescing
 *	them -- into single sectors.
 *
 *	This rounds the start down and the end up to sectors, reads once into a
 *	32-byte-aligned buffer, and slides the wanted bytes to the front. The result
 *	is freed with free(). The descriptor is kept open between calls to the same
 *	path so a directory lookup is not paid per sound.
 */
void *dc_read_file_span(const char *path, unsigned long offset, unsigned long length)
{
	/* Sounds and sprite frames interleave during play: keep a few files open. */
	enum { kOpenFiles = 4 };
	static file_t fds[kOpenFiles] = { FILEHND_INVALID, FILEHND_INVALID, FILEHND_INVALID, FILEHND_INVALID };
	static char open_paths[kOpenFiles][256];
	static unsigned next_slot = 0;
	unsigned slot;

	for (slot = 0; slot < kOpenFiles; ++slot)
		if (fds[slot] != FILEHND_INVALID && strcmp(open_paths[slot], path) == 0)
			break;
	if (slot == kOpenFiles)
	{
		slot = next_slot;
		next_slot = (next_slot + 1) % kOpenFiles;
		if (fds[slot] != FILEHND_INVALID)
			fs_close(fds[slot]);
		fds[slot] = fs_open(path, O_RDONLY);
		if (fds[slot] == FILEHND_INVALID)
			return NULL;
		strncpy(open_paths[slot], path, sizeof(open_paths[slot]) - 1);
		open_paths[slot][sizeof(open_paths[slot]) - 1] = 0;
	}
	const file_t fd = fds[slot];

	const unsigned long start = offset & ~2047UL;
	const unsigned long skip = offset - start;
	const unsigned long span = ((offset + length + 2047UL) & ~2047UL) - start;
	uint8_t *buf = memalign(32, span);
	if (!buf)
		return NULL;

	if (fs_seek(fd, (off_t)start, SEEK_SET) != (off_t)start)
	{
		free(buf);
		fs_close(fd);
		fds[slot] = FILEHND_INVALID;
		return NULL;
	}

	unsigned long got = 0;
	while (got < span)
	{
		/* The last sector of a file may be short; the driver stops at EOF. */
		ssize_t n = fs_read(fd, buf + got, span - got);
		if (n <= 0)
			break;
		got += (unsigned long)n;
	}
	if (got < skip + length)
	{
		free(buf);
		return NULL;
	}
	if (skip)
		memmove(buf, buf + skip, length);
	return buf;
}

int dc_trace_on(void) { return dc_trace_enabled(); }

/* Heap sanity probe (diagnostic): mallinfo totals beyond the machine's RAM mean
   the allocator's chunk chain is damaged. Reports the first place it goes bad. */
void dc_heap_probe(const char *where)
{
	struct mallinfo m = mallinfo();
	static int bad_reported = 0;
	int bad = m.uordblks < 0 || m.uordblks > 20*1024*1024 || m.fordblks < 0 || m.fordblks > 20*1024*1024;
	if (bad && !bad_reported) { bad_reported = 1; dc_trace(73, "heap: CORRUPT at %s (live=%d free=%d)", where, m.uordblks, m.fordblks); }
	else if (!bad) dc_trace(73, "heap: ok at %s live=%d KB", where, m.uordblks/1024);
}

/*
 *	Bring the PowerVR back after a trip through the menus.
 *
 *	Leaving a level sets the plain 640x480 mode, and SDL's Dreamcast driver
 *	calls pvr_shutdown() for that. Entering the next level sets the GL mode
 *	again and the driver calls glKosInit() -- which GLdc ignores the second
 *	time (its _initialized flag), so nothing calls pvr_init() and the first
 *	pvr_wait_ready() of the new level dies on "pvr_state.valid". That was the
 *	black screen and crash on loading a saved game, or starting a new one,
 *	after quitting to the main menu. This is GLdc's own pvr_init call with
 *	GLdc's own parameters (GL/platforms/sh4.c: InitGPU), nothing else: GLdc's
 *	texture and vertex bookkeeping is left exactly as it was, which is why a
 *	full glKosShutdown/glKosInit is not used (it re-creates its buffers
 *	without freeing the old ones). pvr_init() returns -1 when the PVR is
 *	already up, which is the first level after boot.
 */
int dc_pvr_reinit(void)
{
	pvr_init_params_t params = {
		{ PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32 },
		2560 * 256,	/* vertex buffer, as GLdc */
		0,		/* no DMA */
		0,		/* no FSAA */
		1,		/* translucent autosort off, as GLdc */
		2		/* OPB overflow bins, as GLdc */
	};
	return pvr_init(&params);
}

/* Send a 48x32 1-bit frame to the first VMU screen; see dc_vmu_hud.cpp. */
int dc_vmu_lcd_send(const void *bitmap)
{
	maple_device_t *vmu = maple_enum_type(0, MAPLE_FUNC_LCD);
	if (!vmu) return -2;
	return vmu_draw_lcd(vmu, bitmap) == MAPLE_EOK ? 0 : -1;
}

/* Milliseconds since boot, for C++98 callers that cannot include KOS's timer.h. */
unsigned long dc_ms(void)
{
	return (unsigned long)timer_ms_gettime64();
}

/*
 *	dc_heap_top() is a high-water mark: freed blocks stay below it and are
 *	reused. This is what is actually live, and what is free to reuse.
 */
/*
 *	What the PVR is doing right now: the frame on the display, the frame being
 *	rendered, how many frames it has flipped, how many vblanks it has seen, and
 *	whether the TA is ready for another scene. For finding out why a frame that
 *	was drawn is not being shown.
 */
void dc_pvr_trace(int slot, const char *where)
{
	pvr_stats_t st;
	if (!dc_trace_enabled()) return;
	pvr_get_stats(&st);
	dc_trace(slot, "pvr %-8s disp=%08lx rend=%08lx frames=%lu vbl=%lu ready=%d",
	         where,
	         (unsigned long)*(volatile uint32_t *)0xa05f8050,
	         (unsigned long)*(volatile uint32_t *)0xa05f8060,
	         (unsigned long)st.frame_count, (unsigned long)st.vbl_count,
	         pvr_check_ready());
}

void dc_memory_trace(void)
{
	if (!dc_trace_enabled()) return;
	struct mallinfo heap = mallinfo();
	dc_trace(63, "ram: live=%u KB reusable=%u KB",
	         (unsigned)(heap.uordblks/1024), (unsigned)(heap.fordblks/1024));
}
