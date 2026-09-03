/* 
 *  shapes_sdl.cpp - Shapes handling, SDL specific stuff (included by shapes.cpp)
 *
 *  Written in 2000 by Christian Bauer
 */

/*
Oct 19, 2000 (Loren Petrich):
	Added graceful degradation in the case of frames or bitmaps not being found;
	get_shape_surface() returns NULL when that happens
*/

#include <SDL_endian.h>

#include "byte_swapping.h"


/*
 *  Initialize shapes handling
 */

static void initialize_pixmap_handler()
{
	// nothing to do
}


/*
 *  Convert shape to surface
 */

SDL_Surface *get_shape_surface(int shape)
{
	// Get shape information
	int collection_index = GET_COLLECTION(GET_DESCRIPTOR_COLLECTION(shape));
	int clut_index = GET_COLLECTION_CLUT(GET_DESCRIPTOR_COLLECTION(shape));
	int low_level_shape_index = GET_DESCRIPTOR_SHAPE(shape);
	struct collection_definition *collection = get_collection_definition(collection_index);
	struct low_level_shape_definition *low_level_shape = get_low_level_shape_definition(collection_index, low_level_shape_index);
	if (!low_level_shape) return NULL;
	struct bitmap_definition *bitmap = get_bitmap_definition(collection_index, low_level_shape->bitmap_index);
	if (!bitmap) return NULL;

	// Extract color table
	SDL_Color colors[256];
	int num_colors = collection->color_count - NUMBER_OF_PRIVATE_COLORS;
	rgb_color_value *src_colors = get_collection_colors(collection_index, clut_index) + NUMBER_OF_PRIVATE_COLORS;
	for (int i=0; i<num_colors; i++) {
		int idx = src_colors[i].value;
		colors[idx].r = src_colors[i].red >> 8;
		colors[idx].g = src_colors[i].green >> 8;
		colors[idx].b = src_colors[i].blue >> 8;
	}

	SDL_Surface *s = NULL;
	if (bitmap->bytes_per_row == NONE) {

		// Column-order shape, needs to be converted to row-order
		fprintf(stderr, "Drawing of column-order shapes not implemented.\n");
		abort();
		//!!

	} else {

		// Row-order shape, we can directly create a surface from it
		s = SDL_CreateRGBSurfaceFrom(bitmap->row_addresses[0], bitmap->width, bitmap->height, 8, bitmap->bytes_per_row, 0xff, 0xff, 0xff, 0xff);
	}
	if (s == NULL)
		return NULL;

	// Set color table
	SDL_SetColors(s, colors, 0, 256);
	return s;
}


/*
 *  Load collection
 */


#ifdef DC
#include <malloc.h>	/* memalign; see the buffer allocation below */
#include <stdint.h>
extern "C" void dc_trace(int slot, const char *fmt, ...);

/*
 *	Sector-aligned read cache over the shapes file.
 *
 *	load_collection() parses a collection with hundreds of small reads -- two
 *	bytes here, a name there, one bitmap row at a time -- and each one was a
 *	separate trip to the CD driver. That is where 26 of the level load's 42
 *	seconds went.
 *
 *	A previous attempt (c30cf24) put a 64KB buffer on the stdio stream instead
 *	and hung the driver: 0 of 3 controlled runs reached first render. Collection
 *	0 starts at offset 1024, half a sector, and KOS's ISO9660 fast path wants
 *	sector-aligned requests of at least a sector. A large buffered read from a
 *	misaligned position both misses the fast path and, at 64KB, wedges it. An
 *	8KB buffer completed but saved nothing, because misalignment defeats the
 *	fast path at any size.
 *
 *	So do the aligning here rather than asking stdio to do it. Every refill
 *	reads DC_SHAPES_CACHE bytes from a 2048-aligned offset -- pos & ~2047 -- and
 *	the leading bytes before the requested position are simply skipped in the
 *	buffer. The parse is untouched except that its seeks go through this wrapper,
 *	so the change is confined to how bytes are fetched.
 *
 *	Positions here are absolute file offsets. OpenedFile::SetPosition adds a
 *	fork_offset before seeking, so load_collection discovers that offset once
 *	and adds it; see COLL_SEEK.
 */
#define DC_SHAPES_SECTOR	2048
#define DC_SHAPES_CACHE		(32 * DC_SHAPES_SECTOR)		/* 64KB, 32 sectors */

struct dc_shapes_cache {
	SDL_RWops *src;		/* the real file */
	long pos;			/* logical position, absolute in the file */
	long base;			/* file offset of buf[0], sector aligned; -1 = empty */
	long len;			/* valid bytes in buf */
	uint8 *buf;
	unsigned refills;	/* real reads issued */
	unsigned long served;	/* bytes handed to the parse */
	unsigned long fetched;	/* bytes actually read off the disc */
};

static int dc_shapes_seek(SDL_RWops *ctx, int offset, int whence)
{
	struct dc_shapes_cache *c = (struct dc_shapes_cache *)ctx->hidden.unknown.data1;

	switch (whence) {
	case SEEK_SET:	c->pos = offset; break;
	case SEEK_CUR:	c->pos += offset; break;
	default:
		/* SEEK_END is not used by the parse; let the real file answer it. */
		c->pos = SDL_RWseek(c->src, offset, whence);
		break;
	}
	return (int)c->pos;
}

static int dc_shapes_read(SDL_RWops *ctx, void *ptr, int size, int maxnum)
{
	struct dc_shapes_cache *c = (struct dc_shapes_cache *)ctx->hidden.unknown.data1;
	long want = (long)size * (long)maxnum;
	uint8 *out = (uint8 *)ptr;
	long done = 0;

	while (done < want) {
		if (c->base < 0 || c->pos < c->base || c->pos >= c->base + c->len) {
			int got;

			c->base = c->pos & ~(long)(DC_SHAPES_SECTOR - 1);
			if (SDL_RWseek(c->src, c->base, SEEK_SET) < 0)
				break;
			got = SDL_RWread(c->src, c->buf, 1, DC_SHAPES_CACHE);
			c->refills++;
			if (got > 0)
				c->fetched += (unsigned long)got;
			if (got <= 0) {
				c->len = 0;
				break;
			}
			c->len = got;
		}

		{
			long off = c->pos - c->base;
			long avail = c->len - off;
			long n;

			if (avail <= 0)
				break;
			n = want - done;
			if (n > avail)
				n = avail;
			memcpy(out + done, c->buf + off, (size_t)n);
			done += n;
			c->pos += n;
		}
	}

	c->served += (unsigned long)done;
	return size ? (int)(done / size) : 0;
}

static int dc_shapes_write(SDL_RWops *ctx, const void *ptr, int size, int num)
{
	(void)ctx; (void)ptr; (void)size; (void)num;
	return -1;		/* read-only */
}

static int dc_shapes_close(SDL_RWops *ctx)
{
	(void)ctx;		/* the wrapper and its buffer are reused, not freed */
	return 0;
}

/*
 *	One wrapper, reused for every collection and every level load. Returns the
 *	raw stream unchanged if the buffer cannot be had, so a tight heap costs
 *	speed rather than the level.
 */
static struct dc_shapes_cache *dc_shapes_stats = NULL;

unsigned long dc_shapes_cache_served(void)
{ return dc_shapes_stats ? dc_shapes_stats->served : 0; }

unsigned long dc_shapes_cache_fetched(void)
{ return dc_shapes_stats ? dc_shapes_stats->fetched : 0; }

unsigned dc_shapes_cache_refills(void)
{ return dc_shapes_stats ? dc_shapes_stats->refills : 0; }

static SDL_RWops *dc_shapes_cached(SDL_RWops *src)
{
	static SDL_RWops *Wrapper = NULL;
	static struct dc_shapes_cache Cache;

	if (Wrapper == NULL) {
		/*
		 *	memalign, not malloc: KOS's ISO9660 fast path wants a 32-byte
		 *	aligned destination as well as a sector-aligned offset, and falls
		 *	back to a slower copy without it.
		 */
		Cache.buf = (uint8 *)memalign(32, DC_SHAPES_CACHE);
		if (Cache.buf == NULL)
			return src;
		Wrapper = SDL_AllocRW();
		if (Wrapper == NULL) {
			free(Cache.buf);
			Cache.buf = NULL;
			return src;
		}
		Wrapper->seek  = dc_shapes_seek;
		Wrapper->read  = dc_shapes_read;
		Wrapper->write = dc_shapes_write;
		Wrapper->close = dc_shapes_close;
		Wrapper->type  = 0;
		Wrapper->hidden.unknown.data1 = &Cache;
		dc_trace(63, "shapes: cache buf %p (%s32-byte aligned), %d KB",
		         (void *)Cache.buf,
		         (((uintptr_t)Cache.buf & 31u) == 0) ? "" : "NOT ",
		         DC_SHAPES_CACHE / 1024);
		Cache.base = -1;
		Cache.len = 0;
		Cache.refills = 0;
	}

	if (Cache.src != src) {
		/* Different file: whatever is buffered belongs to the old one. */
		Cache.src = src;
		Cache.base = -1;
		Cache.len = 0;
	}
	Cache.pos = 0;
	dc_shapes_stats = &Cache;
	return Wrapper;
}
#endif	/* DC */

static bool load_collection(short collection_index, bool strip)
{
	SDL_RWops *p = ShapesFile.GetRWops();	// Source stream
#ifdef DC
	/*
	 *	Read the collection through the sector-aligned cache above rather than
	 *	straight off the disc. SetPosition() adds a fork offset before seeking,
	 *	so find that offset once -- seek to fork position 0 and ask the real
	 *	stream where that landed -- and add it to every absolute seek below.
	 */
	long ForkBase;
	ShapesFile.SetPosition(0);
	ForkBase = SDL_RWtell(p);
	p = dc_shapes_cached(p);
#define COLL_SEEK(off)	SDL_RWseek(p, ForkBase + (long)(off), SEEK_SET)
#else
#define COLL_SEEK(off)	ShapesFile.SetPosition(off)
#endif
	uint32 *t;								// Offset table pointer

	// Get offset and length of data in source file from header
	collection_header *header = get_collection_header(collection_index);
	long src_offset, src_length;
	if (bit_depth == 8 || header->offset16 == -1) {
		vassert(header->offset != -1, csprintf(temporary, "collection #%d does not exist.", collection_index));
		src_offset = header->offset;
		src_length = header->length;
	} else {
		src_offset = header->offset16;
		src_length = header->length16;
	}

	// Read collection definition
	COLL_SEEK(src_offset);
	int16 version = SDL_ReadBE16(p);
	int16 type = SDL_ReadBE16(p);
	uint16 flags = SDL_ReadBE16(p);
	int16 color_count = SDL_ReadBE16(p);
	int16 clut_count = SDL_ReadBE16(p);
	int32 color_table_offset = SDL_ReadBE32(p);
	int16 high_level_shape_count = SDL_ReadBE16(p);
	int32 high_level_shape_offset_table_offset = SDL_ReadBE32(p);
	int16 low_level_shape_count = SDL_ReadBE16(p);
	int32 low_level_shape_offset_table_offset = SDL_ReadBE32(p);
	int16 bitmap_count = SDL_ReadBE16(p);
	int32 bitmap_offset_table_offset = SDL_ReadBE32(p);
	int16 pixels_to_world = SDL_ReadBE16(p);
	int32 size = SDL_ReadBE32(p);

	// Allocate memory for collection
	int extra_length = 1024 + high_level_shape_count * 4 + low_level_shape_count * 4 + bitmap_count * 2048;
	void *c = malloc(src_length + extra_length);
	if (c == NULL)
		return false;

	// Initialize collection definition
	collection_definition *cd = (collection_definition *)c;
	cd->version = version;
	cd->type = type;
	cd->flags = flags;
	cd->color_count = color_count;
	cd->clut_count = clut_count;
	cd->high_level_shape_count = high_level_shape_count;
	cd->low_level_shape_count = low_level_shape_count;
	cd->bitmap_count = bitmap_count;
	cd->pixels_to_world = pixels_to_world;
//	printf(" index %d, version %d, type %d, %d colors, %d cluts, %d hl, %d ll, %d bitmaps\n", collection_index, version, type, color_count, clut_count, high_level_shape_count, low_level_shape_count, bitmap_count);

	// Set up destination pointer
	uint8 *q = (uint8 *)c + 0x220;
#define dst_offset (q - (uint8 *)c)

	// Convert CLUTs
	COLL_SEEK(src_offset + color_table_offset);
	cd->color_table_offset = dst_offset;
	for (int i=0; i<clut_count*color_count; i++) {
		rgb_color_value *r = (rgb_color_value *)q;
		SDL_RWread(p, r, 1, 2);
		r->red = SDL_ReadBE16(p);
		r->green = SDL_ReadBE16(p);
		r->blue = SDL_ReadBE16(p);
		q += sizeof(rgb_color_value);
	}

	// Convert high-level shape definitions
	COLL_SEEK(src_offset + high_level_shape_offset_table_offset);
	cd->high_level_shape_offset_table_offset = dst_offset;

	t = (uint32 *)q;	// Offset table
	SDL_RWread(p, t, sizeof(uint32), high_level_shape_count);
	byte_swap_memory(t, _4byte, high_level_shape_count);
	q += high_level_shape_count * sizeof(uint32);

	for (int i=0; i<high_level_shape_count; i++) {

		// Seek to offset in source file, correct destination offset
		COLL_SEEK(src_offset + t[i]);
		t[i] = dst_offset;

		// Convert high-level shape definition
		high_level_shape_definition *d = (high_level_shape_definition *)q;
		d->type = SDL_ReadBE16(p);
		d->flags = SDL_ReadBE16(p);
		SDL_RWread(p, d->name, 1, HIGH_LEVEL_SHAPE_NAME_LENGTH + 2);
		d->number_of_views = SDL_ReadBE16(p);
		d->frames_per_view = SDL_ReadBE16(p);
		d->ticks_per_frame = SDL_ReadBE16(p);
		d->key_frame = SDL_ReadBE16(p);
		d->transfer_mode = SDL_ReadBE16(p);
		d->transfer_mode_period = SDL_ReadBE16(p);
		d->first_frame_sound = SDL_ReadBE16(p);
		d->key_frame_sound = SDL_ReadBE16(p);
		d->last_frame_sound = SDL_ReadBE16(p);
		d->pixels_to_world = SDL_ReadBE16(p);
		d->loop_frame = SDL_ReadBE16(p);
		SDL_RWseek(p, 28, SEEK_CUR);

		// Convert low-level shape index list
		int num_views;
		switch (d->number_of_views) {
			case _unanimated:
			case _animated1:
				num_views = 1;
				break;
			case _animated3to4:
			case _animated4:
				num_views = 4;
				break;
			case _animated3to5:
			case _animated5:
				num_views = 5;
				break;
			case _animated2to8:
			case _animated5to8:
			case _animated8:
				num_views = 8;
				break;
			default:
				num_views = d->number_of_views;
				break;
		}
		for (int j=0; j<num_views*d->frames_per_view; j++)
			d->low_level_shape_indexes[j] = SDL_ReadBE16(p);

		q += sizeof(high_level_shape_definition) + (num_views * d->frames_per_view - 1) * sizeof(int16);
		if (dst_offset & 3)	// Align to 32-bit boundary
			q += 4 - (dst_offset & 3);
	}

	// Convert low-level shape definitions
	COLL_SEEK(src_offset + low_level_shape_offset_table_offset);
	cd->low_level_shape_offset_table_offset = dst_offset;

	t = (uint32 *)q;	// Offset table
	SDL_RWread(p, t, sizeof(uint32), low_level_shape_count);
	byte_swap_memory(t, _4byte, low_level_shape_count);
	q += low_level_shape_count * sizeof(uint32);

	for (int i=0; i<low_level_shape_count; i++) {

		// Seek to offset in source file, correct destination offset
		COLL_SEEK(src_offset + t[i]);
		t[i] = dst_offset;

		// Convert low-level shape definition
		low_level_shape_definition *d = (low_level_shape_definition *)q;
		d->flags = SDL_ReadBE16(p);
		d->minimum_light_intensity = SDL_ReadBE32(p);
		d->bitmap_index = SDL_ReadBE16(p);
		d->origin_x = SDL_ReadBE16(p);
		d->origin_y = SDL_ReadBE16(p);
		d->key_x = SDL_ReadBE16(p);
		d->key_y = SDL_ReadBE16(p);
		d->world_left = SDL_ReadBE16(p);
		d->world_right = SDL_ReadBE16(p);
		d->world_top = SDL_ReadBE16(p);
		d->world_bottom = SDL_ReadBE16(p);
		d->world_x0 = SDL_ReadBE16(p);
		d->world_y0 = SDL_ReadBE16(p);
		SDL_RWseek(p, 8, SEEK_CUR);
		q += sizeof(low_level_shape_definition);
	}

	// Convert bitmap definitions
	COLL_SEEK(src_offset + bitmap_offset_table_offset);
	cd->bitmap_offset_table_offset = dst_offset;

	t = (uint32 *)q;	// Offset table
	SDL_RWread(p, t, sizeof(uint32), bitmap_count);
	byte_swap_memory(t, _4byte, bitmap_count);
	q += bitmap_count * sizeof(uint32);
	if (dst_offset & 7)	// Align to 64-bit boundary
		q += 8 - (dst_offset & 7);

	for (int i=0; i<bitmap_count; i++) {

		// Seek to offset in source file, correct destination offset
		COLL_SEEK(src_offset + t[i]);
		t[i] = dst_offset;

		// Convert bitmap definition
		bitmap_definition *d = (bitmap_definition *)q;
		d->width = SDL_ReadBE16(p);
		d->height = SDL_ReadBE16(p);
		d->bytes_per_row = SDL_ReadBE16(p);
		d->flags = SDL_ReadBE16(p);
		d->bit_depth = SDL_ReadBE16(p);
		SDL_RWseek(p, 16, SEEK_CUR);
		q += sizeof(bitmap_definition);

		// Skip row address pointers
		int rows = (d->flags & _COLUMN_ORDER_BIT) ? d->width : d->height;
		SDL_RWseek(p, (rows + 1) * sizeof(uint32), SEEK_CUR);
		q += rows * sizeof(pixel8 *);

		// Copy bitmap data
		if (d->bytes_per_row == NONE) {
			// RLE format
			for (int j=0; j<rows; j++) {
				int16 first = SDL_ReadBE16(p);
				int16 last = SDL_ReadBE16(p);
				*q++ = first >> 8; *q++ = first;
				*q++ = last >> 8; *q++ = last;
				SDL_RWread(p, q, 1, last - first);
				q += last - first;
			}
		} else {
			// Raw format
			SDL_RWread(p, q, d->bytes_per_row, rows);
			q += rows * d->bytes_per_row;
		}
		if (dst_offset & 7)	// Align to 64-bit boundary
			q += 8 - (dst_offset & 7);
	}

	// Set pointer to collection in collection header
	header->collection = cd;
	cd->size = dst_offset;
//	printf(" collection at %p, size %d -> %d\n", cd, src_length, cd->size);
	assert(cd->size <= src_length + extra_length);

	if (strip) {
		//!! don't know what to do
		fprintf(stderr, "Stripped shapes not implemented\n");
		abort();
	}

	// Allocate enough space for this collection's shading tables
	if (strip)
		header->shading_tables = NULL;
	else {
		collection_definition *definition = get_collection_definition(collection_index);
		header->shading_tables = (byte *)malloc(get_shading_table_size(collection_index) * definition->clut_count + shading_table_size * NUMBER_OF_TINT_TABLES);
	}
	if (header->shading_tables == NULL) {
		free(header->collection);
		header->collection = NULL;
		free(c);
		return false;
	}

	// Everything OK
	return true;
}

#undef COLL_SEEK


/*
 *  Unload collection
 */

static void unload_collection(struct collection_header *header)
{
	assert(header->collection);
	free(header->collection);
	free(header->shading_tables);
	header->collection = NULL;
	header->shading_tables = NULL;
}
