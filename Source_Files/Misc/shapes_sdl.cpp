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
/*
 *	Aggregate breakdown of load_collection, accumulated silently and printed
 *	once by load_collections. Nothing is traced per collection or per bitmap:
 *	dc_trace goes out over the emulated serial line and would dominate what it
 *	is trying to measure.
 */
unsigned dc_lc_ms_header = 0, dc_lc_ms_alloc = 0, dc_lc_ms_clut = 0;
unsigned dc_lc_ms_high = 0, dc_lc_ms_low = 0, dc_lc_ms_bitmaps = 0;
unsigned dc_lc_n_rle = 0, dc_lc_n_raw = 0, dc_lc_n_rle_rows = 0;
unsigned long dc_lc_bytes_rle = 0, dc_lc_bytes_raw = 0;
#endif

#ifdef DC
/*
 *	The whole collection, read from the disc in one transfer and parsed from
 *	memory. If the read fails the parser falls back to the file stream, which is
 *	slow but correct. Frees itself on every return path.
 */
struct DCCollectionSpan
{
	void *buf;
	SDL_RWops *rw;
	DCCollectionSpan(long offset, long length)
	{
		buf = dc_read_file_span(dc_shapes_path, dc_shapes_fork + offset, length);
		rw = buf ? SDL_RWFromMem(buf, length) : NULL;
	}
	~DCCollectionSpan()
	{
		if (rw) SDL_FreeRW(rw);
		free(buf);
	}
};
#endif

static bool load_collection(short collection_index, bool strip)
{
#ifdef DC
	unsigned dc_t = SDL_GetTicks();
#endif
	SDL_RWops *p = ShapesFile.GetRWops();	// Source stream
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

#ifdef DC
	DCCollectionSpan span(src_offset, src_length);
	if (span.rw)
		p = span.rw;
#define SRC_SEEK(pos) do { if (span.rw) SDL_RWseek(span.rw, (pos) - src_offset, SEEK_SET); \
	                       else ShapesFile.SetPosition(pos); } while (0)
#else
#define SRC_SEEK(pos) ShapesFile.SetPosition(pos)
#endif

	// Read collection definition
	SRC_SEEK(src_offset);
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

#ifdef DC
	dc_lc_ms_header += SDL_GetTicks() - dc_t;
	dc_t = SDL_GetTicks();
#endif
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

#ifdef DC
	dc_lc_ms_alloc += SDL_GetTicks() - dc_t;
	dc_t = SDL_GetTicks();
#endif
	// Set up destination pointer
	uint8 *q = (uint8 *)c + 0x220;
#define dst_offset (q - (uint8 *)c)

	// Convert CLUTs
	SRC_SEEK(src_offset + color_table_offset);
	cd->color_table_offset = dst_offset;
	for (int i=0; i<clut_count*color_count; i++) {
		rgb_color_value *r = (rgb_color_value *)q;
		SDL_RWread(p, r, 1, 2);
		r->red = SDL_ReadBE16(p);
		r->green = SDL_ReadBE16(p);
		r->blue = SDL_ReadBE16(p);
		q += sizeof(rgb_color_value);
	}

#ifdef DC
	dc_lc_ms_clut += SDL_GetTicks() - dc_t;
	dc_t = SDL_GetTicks();
#endif
	// Convert high-level shape definitions
	SRC_SEEK(src_offset + high_level_shape_offset_table_offset);
	cd->high_level_shape_offset_table_offset = dst_offset;

	t = (uint32 *)q;	// Offset table
	SDL_RWread(p, t, sizeof(uint32), high_level_shape_count);
	byte_swap_memory(t, _4byte, high_level_shape_count);
	q += high_level_shape_count * sizeof(uint32);

	for (int i=0; i<high_level_shape_count; i++) {

		// Seek to offset in source file, correct destination offset
		SRC_SEEK(src_offset + t[i]);
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

#ifdef DC
	dc_lc_ms_high += SDL_GetTicks() - dc_t;
	dc_t = SDL_GetTicks();
#endif
	// Convert low-level shape definitions
	SRC_SEEK(src_offset + low_level_shape_offset_table_offset);
	cd->low_level_shape_offset_table_offset = dst_offset;

	t = (uint32 *)q;	// Offset table
	SDL_RWread(p, t, sizeof(uint32), low_level_shape_count);
	byte_swap_memory(t, _4byte, low_level_shape_count);
	q += low_level_shape_count * sizeof(uint32);

	for (int i=0; i<low_level_shape_count; i++) {

		// Seek to offset in source file, correct destination offset
		SRC_SEEK(src_offset + t[i]);
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

#ifdef DC
	dc_lc_ms_low += SDL_GetTicks() - dc_t;
	dc_t = SDL_GetTicks();
#endif
	// Convert bitmap definitions
	SRC_SEEK(src_offset + bitmap_offset_table_offset);
	cd->bitmap_offset_table_offset = dst_offset;

	t = (uint32 *)q;	// Offset table
	SDL_RWread(p, t, sizeof(uint32), bitmap_count);
	byte_swap_memory(t, _4byte, bitmap_count);
	q += bitmap_count * sizeof(uint32);
	if (dst_offset & 7)	// Align to 64-bit boundary
		q += 8 - (dst_offset & 7);

	for (int i=0; i<bitmap_count; i++) {

		// Seek to offset in source file, correct destination offset
		SRC_SEEK(src_offset + t[i]);
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
#ifdef DC
			dc_lc_n_rle++;
			dc_lc_n_rle_rows += (unsigned)rows;
#endif
			for (int j=0; j<rows; j++) {
				int16 first = SDL_ReadBE16(p);
				int16 last = SDL_ReadBE16(p);
#ifdef DC
				dc_lc_bytes_rle += (unsigned long)(last - first);
#endif
				*q++ = first >> 8; *q++ = first;
				*q++ = last >> 8; *q++ = last;
				SDL_RWread(p, q, 1, last - first);
				q += last - first;
			}
		} else {
			// Raw format
#ifdef DC
			dc_lc_n_raw++;
			dc_lc_bytes_raw += (unsigned long)rows * (unsigned long)d->bytes_per_row;
#endif
			SDL_RWread(p, q, d->bytes_per_row, rows);
			q += rows * d->bytes_per_row;
		}
		if (dst_offset & 7)	// Align to 64-bit boundary
			q += 8 - (dst_offset & 7);
	}

#ifdef DC
	dc_lc_ms_bitmaps += SDL_GetTicks() - dc_t;
#endif

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
