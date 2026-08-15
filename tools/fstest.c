/*
 *	fstest.c -- what can we actually do to /ram on this KallistiOS?
 *
 *	Aleph One reports error 4 (errUnknownWadVersion) when starting a new game.
 *	That error is set in exactly one place, wad_prefs.cpp:291, and only when the
 *	preferences file opened and its header read cleanly but the wad body did not
 *	come back. So the preferences wad is being written badly, not read badly.
 *
 *	Rather than guess, this boots on its own and prints pass/fail for each
 *	filesystem operation the wad writer depends on. Results go to the
 *	framebuffer via bfont, so they can simply be read off the screen.
 *
 *	The interesting cases are 7 and 8: write_wad() lays down a placeholder
 *	header, appends the data and directory, then seeks back to offset 0 and
 *	rewrites the header with the real offsets. That needs "r+b" and a backwards
 *	seek over an existing file. If the ramdisk cannot do that, the header on
 *	disc stays a placeholder -- which reads back as a valid-looking header with
 *	an unreadable body. Exactly the symptom.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dc/video.h>
#include <dc/biosfont.h>
#include <SDL/SDL.h>

static int line = 0;

static void say(const char *s)
{
	int y = 8 + line * 24;

	if(y > 460) return;

	bfont_draw_str(vram_s + y * 640 + 8, 640, 0, s);
	line++;
}

static void result(const char *name, int ok, const char *detail)
{
	char buf[80];

	snprintf(buf, sizeof buf, "%-22s %-4s %s", name, ok ? "OK" : "FAIL",
	         detail ? detail : "");
	say(buf);
}

#define PATTERN_LEN 256

int main(int argc, char **argv)
{
	char buf[PATTERN_LEN], back[PATTERN_LEN];
	struct stat st;
	FILE *f;
	SDL_RWops *rw;
	int i, ok;

	vid_set_mode(DM_640x480, PM_RGB565);
	say("Aleph One /ram filesystem probe");
	say("");

	for(i = 0; i < PATTERN_LEN; i++)
		buf[i] = (char)(i & 0xff);

	/* 1. stat() on something that is not there must fail. FileSpecifier::Exists
	   goes through access(), which our dc_compat.c implements with stat(). If
	   this reports success the game believes every file already exists. */
	ok = (stat("/ram/definitely-not-here", &st) != 0);
	result("stat missing fails", ok, "");

	/* 2. same question through access() itself */
	ok = (access("/ram/definitely-not-here", 4) != 0);
	result("access missing fails", ok, "");

	/* 3. mkdir, as used for Saved Games / Recordings / MML / Themes */
	mkdir("/ram/Saved Games", 0777);
	ok = (stat("/ram/Saved Games", &st) == 0);
	result("mkdir", ok, "");

	/* 4. plain create and write */
	f = fopen("/ram/prefs", "wb");
	ok = (f != NULL);
	if(f) {
		ok = (fwrite(buf, 1, PATTERN_LEN, f) == PATTERN_LEN);
		fclose(f);
	}
	result("fopen wb + fwrite", ok, "");

	/* 5. does the size stick? */
	ok = (stat("/ram/prefs", &st) == 0 && st.st_size == PATTERN_LEN);
	{
		char d[40];
		snprintf(d, sizeof d, "size=%ld", (long)st.st_size);
		result("stat size", ok, d);
	}

	/* 6. read it back */
	memset(back, 0, sizeof back);
	f = fopen("/ram/prefs", "rb");
	ok = (f != NULL);
	if(f) {
		ok = (fread(back, 1, PATTERN_LEN, f) == PATTERN_LEN)
		     && (memcmp(buf, back, PATTERN_LEN) == 0);
		fclose(f);
	}
	result("read back matches", ok, "");

	/* 7. THE ONE THAT MATTERS: reopen r+b, seek to 0, rewrite the header.
	   This is write_wad()'s final step. */
	ok = 0;
	f = fopen("/ram/prefs", "r+b");
	if(f) {
		if(fseek(f, 0, SEEK_SET) == 0) {
			char hdr[8];
			memset(hdr, 0xAA, sizeof hdr);
			ok = (fwrite(hdr, 1, sizeof hdr, f) == sizeof hdr);
		}
		fclose(f);
	}
	result("r+b seek0 rewrite", ok, f ? "" : "open failed");

	/* 8. did that rewrite actually land, and leave the rest intact? */
	memset(back, 0, sizeof back);
	f = fopen("/ram/prefs", "rb");
	ok = 0;
	if(f) {
		if(fread(back, 1, PATTERN_LEN, f) == PATTERN_LEN) {
			ok = 1;
			for(i = 0; i < 8; i++)
				if((unsigned char)back[i] != 0xAA) ok = 0;
			for(i = 8; i < PATTERN_LEN; i++)
				if(back[i] != buf[i]) ok = 0;
		}
		fclose(f);
	}
	result("rewrite persisted", ok, "");

	/* 9. SDL_RWops is what FileHandler_SDL actually uses, so test that path
	   too rather than assuming it matches stdio. */
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		result("SDL_Init", 0, SDL_GetError());
	} else {
		rw = SDL_RWFromFile("/ram/rwtest", "wb");
		ok = (rw != NULL);
		if(rw) {
			ok = (SDL_RWwrite(rw, buf, 1, PATTERN_LEN) == PATTERN_LEN);
			SDL_RWclose(rw);
		}
		result("SDL_RWFromFile wb", ok, "");

		rw = SDL_RWFromFile("/ram/rwtest", "r+b");
		ok = (rw != NULL);
		if(rw) {
			char hdr[8];
			memset(hdr, 0x55, sizeof hdr);
			ok = (SDL_RWseek(rw, 0, SEEK_SET) == 0)
			     && (SDL_RWwrite(rw, hdr, 1, sizeof hdr) == sizeof hdr);
			SDL_RWclose(rw);
		}
		result("SDL_RW r+b rewrite", ok, "");

		memset(back, 0, sizeof back);
		rw = SDL_RWFromFile("/ram/rwtest", "rb");
		ok = 0;
		if(rw) {
			if(SDL_RWread(rw, back, 1, PATTERN_LEN) == PATTERN_LEN) {
				ok = 1;
				for(i = 0; i < 8; i++)
					if((unsigned char)back[i] != 0x55) ok = 0;
			}
			SDL_RWclose(rw);
		}
		result("SDL_RW rewrite ok", ok, "");
	}

	/* 10. THE REAL QUESTION. OpenedFile::GetLength (FileHandler_SDL.cpp:161)
	   does not use stat -- it seeks to SEEK_END and reports SDL_RWtell. If the
	   ramdisk rounds that to a block boundary the way stat() does, every wad
	   Aleph One reads back is the wrong length. */
	{
		char d[48];
		long len = -1;
		rw = SDL_RWFromFile("/ram/rwtest", "rb");
		if(rw) {
			SDL_RWseek(rw, 0, SEEK_END);
			len = SDL_RWtell(rw);
			SDL_RWclose(rw);
		}
		snprintf(d, sizeof d, "got=%ld want=%d", len, PATTERN_LEN);
		result("SDL_RW seek-end len", len == PATTERN_LEN, d);
	}

	/* 11. and the stdio equivalent, to see whether the two agree */
	{
		char d[48];
		long len = -1;
		f = fopen("/ram/prefs", "rb");
		if(f) {
			fseek(f, 0, SEEK_END);
			len = ftell(f);
			fclose(f);
		}
		snprintf(d, sizeof d, "got=%ld want=%d", len, PATTERN_LEN);
		result("stdio seek-end len", len == PATTERN_LEN, d);
	}

	/* 12. why did mkdir fail, and can we write into a subdirectory at all?
	   Aleph One makes Saved Games, Recordings, MML and Themes under the local
	   data dir at startup. */
	{
		char d[48];
		int r = mkdir("/ram/subdir", 0777);
		snprintf(d, sizeof d, "ret=%d errno=%d", r, errno);
		result("mkdir errno", r == 0, d);

		f = fopen("/ram/subdir/file", "wb");
		ok = (f != NULL);
		if(f) { fwrite("x", 1, 1, f); fclose(f); }
		result("write in subdir", ok, "");
	}

	say("");
	say("done - screenshot this");

	for(;;) { }
	return 0;
}
