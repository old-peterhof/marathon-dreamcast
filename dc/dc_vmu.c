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

#define VMU_FILE_NAME	"ALEPHONE.PRF"
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
                         uint32_t stored_len, uint8_t flags)
{
	memset(h, 0, SAVE_HDR_LEN);
	h[0] = SAVE_MAGIC_0; h[1] = SAVE_MAGIC_1;
	h[2] = SAVE_MAGIC_2; h[3] = SAVE_MAGIC_3;
	put_u32(h + 4, raw_len);
	put_u32(h + 8, stored_len);
	h[12] = flags;
	strncpy((char *)h + SAVE_NAME_OFF, name, SAVE_HDR_LEN - SAVE_NAME_OFF - 1);
}

static int save_hdr_get(const uint8_t *h, char *name, size_t name_len,
                        uint32_t *raw_len, uint32_t *stored_len, uint8_t *flags)
{
	if (h[0] != SAVE_MAGIC_0 || h[1] != SAVE_MAGIC_1 ||
	    h[2] != SAVE_MAGIC_2 || h[3] != SAVE_MAGIC_3)
		return 0;

	*raw_len    = get_u32(h + 4);
	*stored_len = get_u32(h + 8);
	*flags      = h[12];

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
 *	How many blocks the card has left. KOS wants the maple device rather than the
 *	mount path, so this asks the bus directly for the first memory card. Returns
 *	-1 if there is no card or the count cannot be read, which callers treat as
 *	"do not write".
 */
static int vmu_free_blocks(void)
{
	maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_MEMCARD);

	if (!dev)
		return -1;

	return vmufs_free_blocks(dev);
}

/*
 *	Restore every save on the card into the ramdisk. Called once at start-up,
 *	before Aleph One looks at saved_games_dir, so the load dialog simply finds
 *	them there.
 */
void dc_vmu_load_saves(const char *ram_dir)
{
	char unit[32];
	int slot, restored = 0;

	if (!ram_dir)
		ram_dir = "/ram";

	if (!first_vmu(unit, sizeof unit))
		return;

	for (slot = 1; slot <= SAVE_SLOTS; slot++) {
		char src[80], dst[160], name[SAVE_HDR_LEN];
		uint8_t hdr[SAVE_HDR_LEN], flags;
		uint32_t raw_len, stored_len;
		FILE *in, *out;
		uint8_t *stored, *raw;
		size_t got;

		snprintf(src, sizeof src, "%s/AOSAVE%02d.SAV", unit, slot);

		in = fopen(src, "rb");
		if (!in)
			continue;

		if (fread(hdr, 1, SAVE_HDR_LEN, in) != SAVE_HDR_LEN ||
		    !save_hdr_get(hdr, name, sizeof name, &raw_len, &stored_len, &flags) ||
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

		snprintf(dst, sizeof dst, "%s/%s", ram_dir, name);
		out = fopen(dst, "wb");
		if (out) {
			fwrite(raw, 1, raw_len, out);
			fclose(out);
			restored++;
		}

		free(raw);
	}

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
void dc_vmu_save_game(const char *ram_path)
{
	char unit[32], dst[80];
	const char *name;
	vmu_pkg_t pkg;
	FILE *in;
	file_t fd;
	uint8_t *raw = NULL, *out = NULL;
	uLongf bound;
	size_t alloc;
	long size;
	size_t got, payload, padded;
	int slot, target = 0, first_free = 0, free_blocks, need_blocks;

	if (!ram_path)
		return;

	name = base_name(ram_path);
	if (strcmp(name, SAVE_TEMP_NAME) == 0)
		return;

	if (!first_vmu(unit, sizeof unit)) {
		dc_trace(17, "vmu: no memory card, save stays on /ram only");
		return;
	}

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

	/* Deflate. A save is far too big for a card at full size -- 215040 bytes
	   against roughly 99328 free -- so this is not an optimisation, it is the
	   only way the file fits at all. */
	bound = compressBound((uLong)size);

	/* Room for the header, the worst-case deflate, and the block padding that
	   gets rounded up afterwards -- the padded length is computed from the
	   *actual* compressed size, which can round past the bound. */
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

	/* The tail between the payload and the block boundary must be zero on the
	   card; the buffer was sized above to guarantee it is there to zero. */
	if (padded > payload)
		memset(out + payload, 0, padded - payload);

	save_hdr_put(out, name, (uint32_t)size, (uint32_t)bound, SAVE_FLAG_ZLIB);

	/* Pick a slot: the one already holding this name, else the lowest unused. */
	for (slot = 1; slot <= SAVE_SLOTS; slot++) {
		char probe[80], existing[SAVE_HDR_LEN];
		uint8_t hdr[SAVE_HDR_LEN], eflags;
		uint32_t eraw, estored;
		FILE *f;

		snprintf(probe, sizeof probe, "%s/AOSAVE%02d.SAV", unit, slot);

		f = fopen(probe, "rb");
		if (!f) {
			if (!first_free)
				first_free = slot;
			continue;
		}

		if (fread(hdr, 1, SAVE_HDR_LEN, f) == SAVE_HDR_LEN &&
		    save_hdr_get(hdr, existing, sizeof existing, &eraw, &estored, &eflags) &&
		    strcmp(existing, name) == 0)
			target = slot;

		fclose(f);

		if (target)
			break;
	}

	if (!target)
		target = first_free;

	if (!target) {
		dc_trace(17, "vmu: all %d save slots in use", SAVE_SLOTS);
		free(out);
		return;
	}

	snprintf(dst, sizeof dst, "%s/AOSAVE%02d.SAV", unit, target);

	/* Delete any previous copy before measuring free space, so overwriting a
	   save is judged on what it actually costs rather than counted twice. */
	fs_unlink(dst);

	free_blocks = vmu_free_blocks();
	need_blocks = (int)(padded / VMU_BLOCK) + 1;	/* +1 for the directory header */

	if (free_blocks >= 0 && free_blocks < need_blocks) {
		dc_trace(17, "vmu: card full, %s needs %d blocks, %d free",
		         name, need_blocks, free_blocks);
		free(out);
		return;
	}

	fd = fs_open(dst, O_WRONLY | O_TRUNC | O_CREAT);
	if (fd == FILEHND_INVALID) {
		dc_trace(17, "vmu: cannot open %s", dst);
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

	if (fs_write(fd, out, padded) != (ssize_t)padded)
		dc_trace(17, "vmu: short write saving %s", name);
	else
		dc_trace(17, "vmu: saved %s to slot %d, %ld -> %u bytes (%d blocks)",
		         name, target, size, (unsigned)padded, need_blocks);

	fs_close(fd);
	free(out);
}
