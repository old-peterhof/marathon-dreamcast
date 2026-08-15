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

#include <sys/stat.h>
#include <kos/fs_ramdisk.h>

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
