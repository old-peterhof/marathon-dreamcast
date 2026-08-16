/*
 *	dc_vmu.c -- keep Aleph One's preferences on a VMU.
 *
 *	The game writes its preferences to local_data_dir, which on Dreamcast is the
 *	KOS ramdisk (/ram). That is the only writable filesystem available -- the
 *	game itself runs from /cd -- but it is wiped at power-off, so every boot
 *	starts with default keys and default sensitivity.
 *
 *	This mirrors that one file to a memory card either side of the game touching
 *	it: read the VMU copy into the ramdisk before preferences are loaded, write
 *	it back after they are saved. Aleph One is untouched apart from two calls in
 *	wad_prefs.cpp; it still only ever reads and writes /ram.
 *
 *	Saved games are mirrored the same way, with one wrinkle: they do not fit. A
 *	Marathon 2 save measured 215040 bytes on the card, and a VMU does not hold
 *	the 128K its box claims -- 200 of its 256 blocks are available to files, so
 *	102400 bytes, less whatever preferences and other games already occupy. The
 *	first attempt reported "card full, need 421 blocks, 194 free": more than
 *	twice over.
 *
 *	So saves are deflated before they are written. A save wad is overwhelmingly
 *	zeroes and repeated structure -- object slots, polygon state, a thumbnail
 *	overhead map -- which is exactly what zlib is good at. The free-block check
 *	still runs on the compressed size, so a save that will not fit even then is
 *	declined rather than half-written.
 *
 *	Constraints the VMU imposes, from dc/fs_vmu.h:
 *	  - files must be a multiple of 512 bytes, so the payload is zero-padded
 *	  - a vmu_pkg_t header is needed for the file to appear in the BIOS menu
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <kos/fs.h>
#include <dc/fs_vmu.h>
#include <dc/vmu_pkg.h>

extern void dc_trace(int slot, const char *fmt, ...);

/* preferences.cpp: combined sizes of the preference structs this build was
   compiled against. Any change to any of them changes this number. */
extern int dc_prefs_format(void);

static void put_u32(uint8_t *p, uint32_t v);
static uint32_t get_u32(const uint8_t *p);

/* Defined in shell_sdl.cpp. A message box with text supplied directly, so a
   declined save can say how much room the card actually had. */
extern void dc_alert_text(const char *title, const char *line1, const char *line2);

/* Defined in dc_wad.c. Folds a save against the level it was made on, and being
   XOR, folds it back again. Returns -1 if the level could not be read. */
extern int dc_wad_xor_level(uint8_t *save, long save_len,
                            const char *map_path, int level);

#define VMU_FILE_NAME	"ALEPHONE.PRF"

/*
 *	Format stamp for the mirrored preferences.
 *
 *	A preferences file written by one build makes other builds fail to start a
 *	level, and it took most of a night to see it, because the symptom is a black
 *	screen with nothing to read and the file outlives every disc you try.
 *
 *	Aleph One's own guard is not enough. w_get_data_from_preferences notices that
 *	a stored chunk is the wrong size and appends a replacement, then re-extracts
 *	by tag -- and gets the original back, so the struct is read at the wrong
 *	offsets. It breaks in both directions: a newer build reading an older file,
 *	and an older build reading a newer one.
 *
 *	So the card copy carries a stamp: a magic, and a number derived from the
 *	sizes of the preference structs this build was compiled against. If a card
 *	holds preferences shaped for a different build they are left alone, and Aleph
 *	One writes fresh defaults over them. Losing key bindings is a trivial price;
 *	being unable to start a game is not.
 */
#define VMU_PREFS_MAGIC		0x41315046UL	/* 'A1PF' */
#define VMU_PREFS_HDR_LEN	16
#define VMU_BLOCK		512
#define VMU_MAX_BYTES	(64 * VMU_BLOCK)	/* generous ceiling; prefs are tiny */

/*
 *	Find the first memory card. KOS mounts each as its own directory under /vmu
 *	(a1 for the first controller's first slot, and so on), so enumerating that
 *	directory avoids having to probe the maple bus by hand.
 */
static int first_vmu(char *out, size_t out_len)
{
	DIR *d = opendir("/vmu");
	struct dirent *de;

	if (!d)
		return 0;

	while ((de = readdir(d)) != NULL) {
		if (de->d_name[0] == '.')
			continue;

		snprintf(out, out_len, "/vmu/%s", de->d_name);
		closedir(d);
		return 1;
	}

	closedir(d);
	return 0;
}

/*
 *	Copy the saved preferences off the VMU into the ramdisk, if a card holds a
 *	copy. Called before Aleph One reads preferences, so from its point of view
 *	the file was simply already there.
 */
void dc_vmu_load_prefs(const char *ram_path)
{
	char unit[32], src[64];
	FILE *in, *out;
	char *buf;
	size_t got;

	if (!ram_path)
		return;
	if (!first_vmu(unit, sizeof unit)) {
		dc_trace(16, "vmu: no memory card present, prefs stay on /ram");
		return;
	}

	snprintf(src, sizeof src, "%s/" VMU_FILE_NAME, unit);

	in = fopen(src, "rb");
	if (!in) {
		dc_trace(16, "vmu: no saved prefs on %s", unit);
		return;
	}

	buf = malloc(VMU_MAX_BYTES);
	if (!buf) {
		fclose(in);
		return;
	}

	got = fread(buf, 1, VMU_MAX_BYTES, in);
	fclose(in);

	if (got > VMU_PREFS_HDR_LEN &&
	    get_u32((uint8_t *)buf) == VMU_PREFS_MAGIC) {
		uint32_t fmt = get_u32((uint8_t *)buf + 4);
		uint32_t len = get_u32((uint8_t *)buf + 8);

		if (fmt != (uint32_t)dc_prefs_format() || len > got - VMU_PREFS_HDR_LEN) {
			dc_trace(16, "vmu: prefs are for another build -- ignoring");
			free(buf);
			return;
		}

		memmove(buf, buf + VMU_PREFS_HDR_LEN, len);
		got = len;
	} else if (got > 0) {
		/* No stamp: written before this check existed, so its layout cannot be
		   confirmed and is not trusted. */
		dc_trace(16, "vmu: prefs carry no format stamp -- ignoring");
		free(buf);
		return;
	}

	if (got > 0) {
		out = fopen(ram_path, "wb");
		if (out) {
			fwrite(buf, 1, got, out);
			fclose(out);
			dc_trace(16, "vmu: loaded %u bytes from %s", (unsigned)got, unit);
		}
	}

	free(buf);
}

/*
 *	Write the ramdisk copy back to the card. Called after preferences are saved,
 *	rather than at exit, because a console is switched off rather than quit.
 */
void dc_vmu_save_prefs(const char *ram_path)
{
	char unit[32], dst[64];
	vmu_pkg_t pkg;
	FILE *in;
	file_t fd;
	uint8_t *buf;
	size_t got, padded;

	if (!ram_path)
		return;
	if (!first_vmu(unit, sizeof unit)) {
		dc_trace(16, "vmu: no memory card, not saving prefs");
		return;
	}

	in = fopen(ram_path, "rb");
	if (!in)
		return;

	buf = calloc(1, VMU_MAX_BYTES);
	if (!buf) {
		fclose(in);
		return;
	}

	got = fread(buf, 1, VMU_MAX_BYTES, in);
	fclose(in);

	if (got == 0) {
		free(buf);
		return;
	}

	/* Shift the payload up and stamp the format in front of it. */
	memmove(buf + VMU_PREFS_HDR_LEN, buf, got);
	put_u32((uint8_t *)buf, VMU_PREFS_MAGIC);
	put_u32((uint8_t *)buf + 4, (uint32_t)dc_prefs_format());
	put_u32((uint8_t *)buf + 8, (uint32_t)got);
	put_u32((uint8_t *)buf + 12, 0);
	got += VMU_PREFS_HDR_LEN;

	/* Round up to a whole number of blocks; calloc already zeroed the tail. */
	padded = ((got + VMU_BLOCK - 1) / VMU_BLOCK) * VMU_BLOCK;

	snprintf(dst, sizeof dst, "%s/" VMU_FILE_NAME, unit);

	fd = fs_open(dst, O_WRONLY | O_TRUNC | O_CREAT);
	if (fd == FILEHND_INVALID) {
		dc_trace(16, "vmu: cannot open %s for writing", dst);
		free(buf);
		return;
	}

	/* Without a header the file is invisible in the BIOS memory card manager,
	   which makes it look to the player as though nothing was saved. */
	memset(&pkg, 0, sizeof pkg);
	strncpy(pkg.desc_short, "Marathon 2", sizeof pkg.desc_short - 1);
	strncpy(pkg.desc_long, "Preferences", sizeof pkg.desc_long - 1);
	strncpy(pkg.app_id, "ALEPHONE", sizeof pkg.app_id - 1);
	pkg.icon_cnt = 0;
	pkg.icon_anim_speed = 0;
	pkg.eyecatch_type = VMUPKG_EC_NONE;
	pkg.data_len = padded;
	pkg.data = buf;

	fs_vmu_set_header(fd, &pkg);

	if (fs_write(fd, buf, padded) != (ssize_t)padded)
		dc_trace(16, "vmu: short write to %s", dst);
	else
		dc_trace(16, "vmu: saved %u bytes to %s", (unsigned)padded, unit);

	fs_close(fd);
	free(buf);
}


/*
 *	Saved games.
 *
 *	Same shape as preferences -- copy off the card at start-up, copy back after a
 *	write -- but a save has a name the player chose, and a VMU directory entry
 *	holds only twelve characters. So the card-side name is a slot (AOSAVE01.SAV
 *	through AOSAVE08.SAV) and the real name travels inside the file, in a small
 *	header this code strips again on the way back out.
 *
 *	The header also carries the true payload length. VMU files are always a whole
 *	number of 512-byte blocks, and handing Aleph One's wad reader a file with a
 *	tail of padding invites it to trust offsets that are no longer right.
 */

#include <dc/maple.h>
#include <dc/maple/vmu.h>
#include <dc/vmufs.h>
#include <zlib/zlib.h>	/* kos-ports installs it under a zlib/ subdirectory */

#define SAVE_SLOTS		8
#define SAVE_HDR_LEN	64
#define SAVE_MAGIC_0	'A'
#define SAVE_MAGIC_1	'1'
#define SAVE_MAGIC_2	'S'
#define SAVE_MAGIC_3	'V'
#define SAVE_NAME_OFF	16			/* name lives here, after the length fields */
#define SAVE_FLAG_ZLIB	0x01		/* payload is deflated */
#define SAVE_FLAG_DELTA	0x02		/* payload was XORed against its map level */

static void put_u32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xff);       p[1] = (uint8_t)((v >>  8) & 0xff);
	p[2] = (uint8_t)((v >> 16) & 0xff); p[3] = (uint8_t)((v >> 24) & 0xff);
}

static uint32_t get_u32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* The name Aleph One gives its safe-save temporary, which must never be
   mirrored: it exists for a few milliseconds between write and exchange. */
#define SAVE_TEMP_NAME	"savetemp.dat"

static void save_hdr_put(uint8_t *h, const char *name, uint32_t raw_len,
                         uint32_t stored_len, uint8_t flags, int level)
{
	memset(h, 0, SAVE_HDR_LEN);
	h[0] = SAVE_MAGIC_0; h[1] = SAVE_MAGIC_1;
	h[2] = SAVE_MAGIC_2; h[3] = SAVE_MAGIC_3;
	put_u32(h + 4, raw_len);
	put_u32(h + 8, stored_len);
	h[12] = flags;
	/* Which level the save was folded against; meaningless without the flag. */
	h[13] = (uint8_t)(level & 0xff);
	h[14] = (uint8_t)((level >> 8) & 0xff);
	strncpy((char *)h + SAVE_NAME_OFF, name, SAVE_HDR_LEN - SAVE_NAME_OFF - 1);
}

static int save_hdr_get(const uint8_t *h, char *name, size_t name_len,
                        uint32_t *raw_len, uint32_t *stored_len, uint8_t *flags,
                        int *level)
{
	if (h[0] != SAVE_MAGIC_0 || h[1] != SAVE_MAGIC_1 ||
	    h[2] != SAVE_MAGIC_2 || h[3] != SAVE_MAGIC_3)
		return 0;

	*raw_len    = get_u32(h + 4);
	*stored_len = get_u32(h + 8);
	*flags      = h[12];

	if (level)
		*level = (int)h[13] | ((int)h[14] << 8);

	strncpy(name, (const char *)h + SAVE_NAME_OFF, name_len - 1);
	name[name_len - 1] = 0;

	return name[0] != 0;
}

/* Everything after the last slash, or the whole string if there is none. */
static const char *base_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

/*
 *	How many blocks a given card has left.
 *
 *	KOS wants the maple device rather than the mount path, and the mount name
 *	encodes it: /vmu/a1 is port a, unit 1. Returns -1 if the device cannot be
 *	found or the count cannot be read, which callers treat as "do not write".
 */
static int vmu_free_blocks(const char *unit)
{
	const char *name = strrchr(unit, '/');
	maple_device_t *dev;
	int port, slot;

	if (!name)
		return -1;

	name++;
	port = name[0] - 'a';
	slot = name[1] - '0';

	if (port < 0 || port > 3 || slot < 0 || slot > 5)
		return -1;

	dev = maple_enum_dev(port, slot);
	if (!dev)
		return -1;

	return vmufs_free_blocks(dev);
}

/*
 *	Choose a card with room for `need` blocks.
 *
 *	A player may have two cards fitted, and the first one found is not
 *	necessarily the one with space -- a Dreamcast owner's VMU is usually already
 *	full of other games. So try each in turn, and if none has room, report back
 *	the most free space any of them had so the message can say something useful.
 */
static int pick_vmu(int need, char *out, size_t out_len, int *best_free, int *found_any)
{
	DIR *d = opendir("/vmu");
	struct dirent *de;
	int ok = 0;

	*best_free = -1;
	*found_any = 0;

	if (!d)
		return 0;

	while ((de = readdir(d)) != NULL) {
		char unit[32];
		int freeb;

		if (de->d_name[0] == '.')
			continue;

		snprintf(unit, sizeof unit, "/vmu/%s", de->d_name);
		*found_any = 1;

		freeb = vmu_free_blocks(unit);
		if (freeb > *best_free)
			*best_free = freeb;

		if (freeb < 0 || freeb >= need) {
			snprintf(out, out_len, "%s", unit);
			ok = 1;
			break;
		}
	}

	closedir(d);
	return ok;
}

/*
 *	Restore every save on the card into the ramdisk. Called once at start-up,
 *	before Aleph One looks at saved_games_dir, so the load dialog simply finds
 *	them there.
 */
void dc_vmu_load_saves(const char *ram_dir, const char *map_path)
{
	char unit[32];
	int slot, restored = 0;
	DIR *dir;
	struct dirent *de;

	if (!ram_dir)
		ram_dir = "/ram";

	dir = opendir("/vmu");
	if (!dir)
		return;

	/* Look on every card, not just the first: a player may keep the game on one
	   and something else on the other. */
	while ((de = readdir(dir)) != NULL) {

	if (de->d_name[0] == '.')
		continue;

	snprintf(unit, sizeof unit, "/vmu/%s", de->d_name);

	for (slot = 1; slot <= SAVE_SLOTS; slot++) {
		char src[80], dst[160], name[SAVE_HDR_LEN];
		uint8_t hdr[SAVE_HDR_LEN], flags;
		uint32_t raw_len, stored_len;
		int level = 0;
		FILE *in, *out;
		uint8_t *stored, *raw;
		size_t got;

		snprintf(src, sizeof src, "%s/AOSAVE%02d.SAV", unit, slot);

		in = fopen(src, "rb");
		if (!in)
			continue;

		if (fread(hdr, 1, SAVE_HDR_LEN, in) != SAVE_HDR_LEN ||
		    !save_hdr_get(hdr, name, sizeof name, &raw_len, &stored_len, &flags, &level) ||
		    raw_len == 0 || stored_len == 0) {
			fclose(in);
			continue;
		}

		stored = malloc(stored_len);
		if (!stored) {
			fclose(in);
			continue;
		}

		got = fread(stored, 1, stored_len, in);
		fclose(in);

		if (got != stored_len) {
			free(stored);
			continue;
		}

		if (flags & SAVE_FLAG_ZLIB) {
			uLongf out_len = raw_len;

			raw = malloc(raw_len);
			if (!raw) {
				free(stored);
				continue;
			}

			if (uncompress(raw, &out_len, stored, stored_len) != Z_OK ||
			    out_len != raw_len) {
				dc_trace(17, "vmu: %s failed to decompress", name);
				free(raw);
				free(stored);
				continue;
			}

			free(stored);
		} else {
			raw = stored;
		}

		/* Unfold against the level, which is the same XOR that folded it. */
		if (flags & SAVE_FLAG_DELTA) {
			if (dc_wad_xor_level(raw, (long)raw_len, map_path, level) < 0) {
				dc_trace(17, "vmu: %s needs level %d, which could not be read",
				         name, level);
				free(raw);
				continue;
			}
		}

		snprintf(dst, sizeof dst, "%s/%s", ram_dir, name);
		out = fopen(dst, "wb");
		if (out) {
			fwrite(raw, 1, raw_len, out);
			fclose(out);
			restored++;
		}

		free(raw);
	}

	}

	closedir(dir);

	if (restored)
		dc_trace(17, "vmu: restored %d saved game(s)", restored);
}

/*
 *	Mirror one freshly written save to the card.
 *
 *	Reuses the slot already holding a save of the same name, so saving over a
 *	game does not consume a second slot. Failure here is reported and otherwise
 *	ignored: the save itself is already safely on the ramdisk and the player can
 *	keep playing, they simply will not have it after a power cycle.
 */
/*
 *	Look for a slot on this card already holding a save of this name, so saving
 *	over a game reuses its space instead of consuming a second slot. Returns the
 *	slot number, or 0 if there is none. `first_free` receives the lowest slot
 *	number not in use, or 0 if the card is out of slots.
 */
static int find_slot(const char *unit, const char *name, int *first_free)
{
	int slot;

	*first_free = 0;

	for (slot = 1; slot <= SAVE_SLOTS; slot++) {
		char probe[80], existing[SAVE_HDR_LEN];
		uint8_t hdr[SAVE_HDR_LEN], flags;
		uint32_t raw_len, stored_len;
		FILE *f;

		snprintf(probe, sizeof probe, "%s/AOSAVE%02d.SAV", unit, slot);

		f = fopen(probe, "rb");
		if (!f) {
			if (!*first_free)
				*first_free = slot;
			continue;
		}

		if (fread(hdr, 1, SAVE_HDR_LEN, f) == SAVE_HDR_LEN &&
		    save_hdr_get(hdr, existing, sizeof existing, &raw_len, &stored_len, &flags, NULL) &&
		    strcmp(existing, name) == 0) {
			fclose(f);
			return slot;
		}

		fclose(f);
	}

	return 0;
}

void dc_vmu_save_game(const char *ram_path, const char *map_path, int level)
{
	static int warned_no_card = 0;
	char unit[32], dst[80], msg1[96], msg2[96];
	const char *name;
	vmu_pkg_t pkg;
	FILE *in;
	file_t fd;
	uint8_t *raw = NULL, *out = NULL;
	uLongf bound;
	uint8_t delta = 0;
	size_t alloc;
	long size;
	size_t got, payload, padded;
	int slot, target = 0, first_free = 0, free_blocks, need_blocks;
	int best_free = -1, found_any = 0, chose = 0;
	DIR *dir;
	struct dirent *de;

	if (!ram_path)
		return;

	name = base_name(ram_path);
	if (strcmp(name, SAVE_TEMP_NAME) == 0)
		return;

	in = fopen(ram_path, "rb");
	if (!in)
		return;

	if (fseek(in, 0, SEEK_END) != 0) {
		fclose(in);
		return;
	}
	size = ftell(in);
	rewind(in);

	if (size <= 0) {
		fclose(in);
		return;
	}

	raw = malloc((size_t)size);
	if (!raw) {
		fclose(in);
		return;
	}

	got = fread(raw, 1, (size_t)size, in);
	fclose(in);

	if (got != (size_t)size) {
		free(raw);
		return;
	}

	/*
	 *	Fold the save against the level it was made on before compressing. Most
	 *	of a save is a verbatim copy of the map already on the disc, so this
	 *	turns the bulk of it into zeroes for deflate to swallow: measured, 163
	 *	blocks down to 22. If the level cannot be read the save is simply stored
	 *	whole, which still works, just large.
	 */
	if (dc_wad_xor_level(raw, (long)size, map_path, level) >= 0)
		delta = SAVE_FLAG_DELTA;
	else
		dc_trace(17, "vmu: level %d unreadable, storing %s whole", level, name);

	bound = compressBound((uLong)size);
	alloc = SAVE_HDR_LEN + (size_t)bound + VMU_BLOCK;

	out = malloc(alloc);
	if (!out) {
		free(raw);
		return;
	}

	if (compress2(out + SAVE_HDR_LEN, &bound, raw, (uLong)size,
	              Z_BEST_COMPRESSION) != Z_OK) {
		dc_trace(17, "vmu: could not compress %s", name);
		free(raw);
		free(out);
		return;
	}

	free(raw);
	raw = NULL;

	payload = SAVE_HDR_LEN + (size_t)bound;
	padded = (payload + VMU_BLOCK - 1) / VMU_BLOCK * VMU_BLOCK;

	if (padded > payload)
		memset(out + payload, 0, padded - payload);

	save_hdr_put(out, name, (uint32_t)size, (uint32_t)bound,
	             (uint8_t)(SAVE_FLAG_ZLIB | delta), level);

	need_blocks = (int)(padded / VMU_BLOCK) + 1;	/* +1 for the directory header */

	/*
	 *	Prefer a card that already holds this save: overwriting it costs nothing
	 *	extra, and its blocks come back the moment the old copy is unlinked.
	 */
	dir = opendir("/vmu");
	if (dir) {
		while ((de = readdir(dir)) != NULL) {
			char cand[32];
			int ff;

			if (de->d_name[0] == '.')
				continue;

			snprintf(cand, sizeof cand, "/vmu/%s", de->d_name);
			found_any = 1;

			slot = find_slot(cand, name, &ff);
			if (slot) {
				char old[80];

				snprintf(old, sizeof old, "%s/AOSAVE%02d.SAV", cand, slot);
				fs_unlink(old);

				snprintf(unit, sizeof unit, "%s", cand);
				target = slot;
				chose = 1;
				break;
			}
		}
		closedir(dir);
	}

	/* Otherwise take any card with room for it. */
	if (!chose) {
		if (pick_vmu(need_blocks, unit, sizeof unit, &best_free, &found_any)) {
			find_slot(unit, name, &first_free);
			target = first_free;
			chose = target != 0;
		}
	}

	if (!found_any) {
		dc_trace(17, "vmu: no memory card, save stays on /ram only");
		if (!warned_no_card) {
			warned_no_card = 1;
			dc_alert_text("NO MEMORY CARD",
			              "This game is saved, but only until",
			              "the console is switched off.");
		}
		free(out);
		return;
	}

	if (!chose) {
		/* Tell the player, with the real numbers. A save that is quietly
		   dropped is worse than one that fails loudly: the game otherwise
		   reports success and the loss only shows up after a power cycle. */
		dc_trace(17, "vmu: card full, %s needs %d blocks, %d free",
		         name, need_blocks, best_free);

		snprintf(msg1, sizeof msg1, "Needs %d blocks, %d free.",
		         need_blocks, best_free < 0 ? 0 : best_free);
		snprintf(msg2, sizeof msg2, "Saved to memory only.");

		dc_alert_text("MEMORY CARD FULL", msg1, msg2);
		free(out);
		return;
	}

	snprintf(dst, sizeof dst, "%s/AOSAVE%02d.SAV", unit, target);

	free_blocks = vmu_free_blocks(unit);
	if (free_blocks >= 0 && free_blocks < need_blocks) {
		dc_trace(17, "vmu: %s short by %d blocks", unit,
		         need_blocks - free_blocks);

		snprintf(msg1, sizeof msg1, "Needs %d blocks, %d free.",
		         need_blocks, free_blocks);
		snprintf(msg2, sizeof msg2, "Saved to memory only.");

		dc_alert_text("MEMORY CARD FULL", msg1, msg2);
		free(out);
		return;
	}

	fd = fs_open(dst, O_WRONLY | O_TRUNC | O_CREAT);
	if (fd == FILEHND_INVALID) {
		dc_trace(17, "vmu: cannot open %s", dst);
		dc_alert_text("MEMORY CARD ERROR",
		              "Could not write to the card.",
		              "Saved to memory only.");
		free(out);
		return;
	}

	memset(&pkg, 0, sizeof pkg);
	strncpy(pkg.desc_short, "Marathon 2", sizeof pkg.desc_short - 1);
	strncpy(pkg.desc_long, name, sizeof pkg.desc_long - 1);
	strncpy(pkg.app_id, "ALEPHONE", sizeof pkg.app_id - 1);
	pkg.icon_cnt = 0;
	pkg.icon_anim_speed = 0;
	pkg.eyecatch_type = VMUPKG_EC_NONE;
	pkg.data_len = padded;
	pkg.data = out;

	fs_vmu_set_header(fd, &pkg);

	if (fs_write(fd, out, padded) != (ssize_t)padded) {
		dc_trace(17, "vmu: short write saving %s", name);
		dc_alert_text("MEMORY CARD ERROR",
		              "The write did not complete.",
		              "Saved to memory only.");
	} else {
		dc_trace(17, "vmu: saved %s to %s slot %d, %ld -> %u bytes (%d blocks)",
		         name, unit, target, size, (unsigned)padded, need_blocks);
	}

	fs_close(fd);
	free(out);
}
