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
 *	Only preferences are mirrored. Saved games are far too large for a VMU's 128K
 *	and would need splitting across blocks to be worth attempting.
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
