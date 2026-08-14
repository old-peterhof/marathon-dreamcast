/*
	dc_compat.c

	POSIX functions that newlib declares for sh-elf but never implements, so
	they only surface as undefined references at link time.

	access(): declared in sys/unistd.h line 20, defined nowhere in libc.a or
	libkallisti.a. Aleph One calls it in two places, both in FileHandler_SDL.cpp
	and both only to ask "does this path exist and can I read it" -- so stat()
	answers the question completely.

	KOS's VFS has no per-user permission model: anything you can stat on /cd,
	/ram, /pc or /vmu you can read. W_OK is the one real distinction, since the
	GD-ROM is physically read-only, so that case is reported honestly.
*/

#ifdef DC

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int access(const char *__path, int __amode)
{
	struct stat st;

	if (__path == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (stat(__path, &st) != 0)
		return -1;			/* stat() has already set errno */

	/* The GD-ROM is read-only; refuse write checks against it rather than
	   claiming success and failing later at the actual fopen(). */
	if ((__amode & W_OK) && strncmp(__path, "/cd", 3) == 0) {
		errno = EROFS;
		return -1;
	}

	/* F_OK, R_OK and X_OK are all satisfied by the file existing. */
	return 0;
}

#endif /* DC */
