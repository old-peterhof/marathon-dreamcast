/*
 *	dc_wad.c -- fold a saved game against the level it came from.
 *
 *	A saved game in this engine is a complete standalone level wad: the whole
 *	map, the stock physics models, and then the handful of things that actually
 *	changed. Measured on a real save, 94% of its 214629 bytes is a copy of data
 *	already sitting on the disc.
 *
 *	That is ruinous on a memory card, which has 200 usable blocks of 512 bytes.
 *	Deflated, such a save still wanted 163 of them -- one save filled the card,
 *	and a card with anything else on it had no room at all.
 *
 *	So before it is compressed, the save is XORed chunk by chunk against the same
 *	chunk of the same level read back off the disc. Bytes that did not change
 *	become zero, and deflate eats runs of zeroes for nothing. Measured on the
 *	same save: 82495 bytes down to 10452, from 163 blocks to 22.
 *
 *	XOR was chosen over a cleverer record-level diff for one reason: it is its
 *	own inverse and it needs to understand nothing about the data. There is no
 *	table of which fields of side_data a switch may alter, no version skew
 *	between what the writer and the reader believe a polygon looks like. Restore
 *	runs the identical operation and gets back the exact bytes Aleph One wrote.
 *	Anything this code fails to recognise is simply left alone and stored whole.
 *
 *	Why the geometry cannot just be dropped instead, which would be smaller
 *	still: sides and polygons genuinely do mutate during play. Switches retexture
 *	panels, terminals change polygon permutations, platforms move floors. The
 *	measurement shows this plainly -- sides came back 1.5% changed, polygons
 *	1.3%. Small, but not nothing, and a reverted switch is a real bug. XOR keeps
 *	those differences and throws away only what is provably identical.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

extern void dc_trace(int slot, const char *fmt, ...);

/* Wad files are big-endian, whatever the machine reading them. */
static uint16_t be16(const uint8_t *p)
{
	return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

/*
 *	Offsets into wad_header, which is SIZEOF_wad_header (128) bytes:
 *	  0   version			2
 *	  2   data_version		2
 *	  4   file_name			64
 *	  68  checksum			4
 *	  72  directory_offset	4
 *	  76  wad_count			2
 *	  78  app_directory_size	2
 *	  80  entry_header_size	2
 *	  82  directory_entry_base_size	2
 *	  84  parent_checksum	4
 */
#define WH_SIZE			128
#define WH_DIR_OFFSET	72
#define WH_WAD_COUNT	76
#define WH_APP_DIR_SIZE	78
#define WH_DIR_BASE		82

/* Each tagged chunk inside an entry carries a 16-byte header: tag, the offset
   of the next chunk relative to the entry, its length, and a spare. */
#define CHUNK_HDR		16

/*
 *	The Map file on the disc is a Mac file that kept its MacBinary wrapper, so
 *	the wad may start 128 bytes in. Rather than trust either, look at both and
 *	take whichever produces a header that makes sense.
 */
static long wad_base(FILE *f, long file_len)
{
	static const long candidates[2] = { 0, 128 };
	unsigned i;

	for (i = 0; i < 2; i++) {
		uint8_t hdr[WH_SIZE];
		long dir_off;
		int count;

		if (fseek(f, candidates[i], SEEK_SET) != 0)
			continue;
		if (fread(hdr, 1, WH_SIZE, f) != WH_SIZE)
			continue;

		dir_off = (long)be32(hdr + WH_DIR_OFFSET);
		count   = (int)be16(hdr + WH_WAD_COUNT);

		if (count > 0 && count < 4096 &&
		    dir_off > 0 && dir_off + candidates[i] <= file_len)
			return candidates[i];
	}

	return -1;
}

/*
 *	Read the raw bytes of one level entry out of a map file.
 *
 *	Returns a malloc'd buffer the caller frees, or NULL. `*len` receives its
 *	length. The buffer holds the entry exactly as it sits in the file, so the
 *	chunk chain inside it can be walked with offsets relative to its start.
 */
static uint8_t *read_level_entry(const char *map_path, int level, long *len)
{
	uint8_t hdr[WH_SIZE], ent[16];
	uint8_t *buf;
	FILE *f;
	long base, file_len, dir_off, ent_off, start, length;
	int count, app_size, dir_base, step;

	f = fopen(map_path, "rb");
	if (!f)
		return NULL;

	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return NULL;
	}
	file_len = ftell(f);

	base = wad_base(f, file_len);
	if (base < 0) {
		fclose(f);
		return NULL;
	}

	if (fseek(f, base, SEEK_SET) != 0 || fread(hdr, 1, WH_SIZE, f) != WH_SIZE) {
		fclose(f);
		return NULL;
	}

	dir_off  = (long)be32(hdr + WH_DIR_OFFSET);
	count    = (int)be16(hdr + WH_WAD_COUNT);
	app_size = (int)be16(hdr + WH_APP_DIR_SIZE);
	dir_base = (int)be16(hdr + WH_DIR_BASE);
	step     = dir_base + app_size;

	if (level < 0 || level >= count || step < 8) {
		fclose(f);
		return NULL;
	}

	ent_off = base + dir_off + (long)level * step;

	if (fseek(f, ent_off, SEEK_SET) != 0 || fread(ent, 1, 8, f) != 8) {
		fclose(f);
		return NULL;
	}

	start  = (long)be32(ent);
	length = (long)be32(ent + 4);

	if (start < 0 || length <= 0 || base + start + length > file_len) {
		fclose(f);
		return NULL;
	}

	buf = malloc((size_t)length);
	if (!buf) {
		fclose(f);
		return NULL;
	}

	if (fseek(f, base + start, SEEK_SET) != 0 ||
	    fread(buf, 1, (size_t)length, f) != (size_t)length) {
		free(buf);
		fclose(f);
		return NULL;
	}

	fclose(f);
	*len = length;
	return buf;
}

/*
 *	Find a chunk by tag within an entry, returning its payload and length.
 *
 *	`entry_start` is where the chunk chain begins inside `buf`; each chunk names
 *	the offset of the next one relative to that same point, and a next offset of
 *	zero ends the chain.
 */
static const uint8_t *find_chunk(const uint8_t *buf, long buf_len,
                                 long entry_start, uint32_t tag, long *out_len)
{
	long p = entry_start;
	int guard = 0;

	while (p >= 0 && p + CHUNK_HDR <= buf_len && guard++ < 256) {
		uint32_t this_tag = be32(buf + p);
		long next = (long)(int32_t)be32(buf + p + 4);
		long size = (long)(int32_t)be32(buf + p + 8);

		if (size < 0 || p + CHUNK_HDR + size > buf_len)
			return NULL;

		if (this_tag == tag) {
			*out_len = size;
			return buf + p + CHUNK_HDR;
		}

		if (next == 0)
			break;

		p = entry_start + next;
	}

	return NULL;
}

/*
 *	XOR a saved game against the level it was made on, in place.
 *
 *	Called with the same arguments before compressing and after decompressing:
 *	the operation is its own inverse, so one routine covers both directions.
 *
 *	Only chunks that exist in both and have identical lengths are touched. A
 *	chunk the map does not carry -- the physics models, the player, the dynamic
 *	world -- is left exactly as it was and simply compresses on its own merits.
 *
 *	Returns the number of chunks folded, or -1 if the level could not be read, in
 *	which case the buffer is untouched and the caller should store it whole.
 */
int dc_wad_xor_level(uint8_t *save, long save_len,
                     const char *map_path, int level)
{
	uint8_t *lvl;
	long lvl_len, save_start;
	long p;
	int folded = 0, guard = 0;

	if (!save || save_len <= WH_SIZE || !map_path)
		return -1;

	lvl = read_level_entry(map_path, level, &lvl_len);
	if (!lvl)
		return -1;

	/* A save holds exactly one entry, and its chunk chain starts directly after
	   the wad header. The map entry's chain starts at the top of its buffer. */
	save_start = WH_SIZE;
	p = save_start;

	while (p >= 0 && p + CHUNK_HDR <= save_len && guard++ < 256) {
		uint32_t tag = be32(save + p);
		long next = (long)(int32_t)be32(save + p + 4);
		long size = (long)(int32_t)be32(save + p + 8);
		const uint8_t *other;
		long other_len = 0;

		if (size < 0 || p + CHUNK_HDR + size > save_len)
			break;

		other = find_chunk(lvl, lvl_len, 0, tag, &other_len);

		if (other && other_len == size) {
			uint8_t *dst = save + p + CHUNK_HDR;
			long i;

			for (i = 0; i < size; i++)
				dst[i] ^= other[i];

			folded++;
		}

		if (next == 0)
			break;

		p = save_start + next;
	}

	free(lvl);
	return folded;
}
