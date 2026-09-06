#ifdef DC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glkos.h>

#include "dc_vq_sprites.h"

extern "C" void dc_trace(int slot, const char *fmt, ...);
extern "C" void *dc_read_file_span(const char *path, unsigned long offset, unsigned long length);
extern "C" unsigned long dc_ms(void);
extern bool OGL_TextureAllocationFits(unsigned bytes);

namespace
{
const uint32_t kPackVersion = 1;
const uint32_t kEntrySize = 32;
const uint32_t kFormatARGB4444VQTwiddled = 1;

#pragma pack(push, 1)
struct PackHeader
{
	char magic[4];
	uint32_t version;
	uint32_t entry_count;
	uint32_t entry_size;
	uint32_t data_offset;
	uint8_t reserved[12];
};

struct PackEntry
{
	uint16_t collection;
	uint16_t clut;
	uint16_t bitmap;
	uint16_t kind;
	uint16_t width;
	uint16_t height;
	uint32_t offset;
	uint32_t size;
	uint32_t format;
	uint8_t reserved[8];
};
#pragma pack(pop)

const char *PackPath = NULL;
PackEntry *Entries = NULL;
uint32_t EntryCount = 0;
bool TriedOpening = false;
unsigned UploadCount = 0;
unsigned long ReadMs = 0;

bool valid_header(const PackHeader& header)
{
	return memcmp(header.magic, "A1VQ", 4) == 0 &&
	       header.version == kPackVersion &&
	       header.entry_size == kEntrySize &&
	       header.entry_count <= 65536 &&
	       header.data_offset >= sizeof(PackHeader) +
	                              header.entry_count * sizeof(PackEntry);
}

/*
 * Everything comes off the disc through dc_read_file_span(): one aligned
 * transfer per request. stdio would read a 40 KB sprite frame 1 KB at a time,
 * one GD-ROM command per sector, which is what made the first seconds of every
 * level a slideshow while monsters appeared.
 */
bool open_pack()
{
	if (TriedOpening)
		return PackPath != NULL;
	TriedOpening = true;

	static const char *const candidates[] = { "/cd/AlephOne/VQSprites.dat", "VQSprites.dat" };
	PackHeader *header = NULL;
	for (unsigned i = 0; i < 2 && !header; ++i)
	{
		header = static_cast<PackHeader *>(dc_read_file_span(candidates[i], 0, sizeof(PackHeader)));
		if (header)
			PackPath = candidates[i];
	}
	if (!header)
	{
		dc_trace(53, "vq: VQSprites.dat absent; raw sprite fallback");
		return false;
	}
	if (!valid_header(*header))
	{
		dc_trace(53, "vq: invalid pack header; raw sprite fallback");
		free(header);
		PackPath = NULL;
		return false;
	}
	const uint32_t entry_count = header->entry_count;
	free(header);

	Entries = static_cast<PackEntry *>(dc_read_file_span(PackPath, sizeof(PackHeader),
	                                                     entry_count * sizeof(PackEntry)));
	if (!Entries)
	{
		dc_trace(53, "vq: cannot read pack index; raw sprite fallback");
		PackPath = NULL;
		return false;
	}
	EntryCount = entry_count;
	dc_trace(53, "vq: %u entries, %u KB index", EntryCount,
	         (unsigned)(EntryCount * sizeof(PackEntry) / 1024));
	return true;
}

int compare_key(const PackEntry& entry, unsigned collection, unsigned clut,
	unsigned bitmap, unsigned kind)
{
	if (entry.collection != collection) return entry.collection < collection ? -1 : 1;
	if (entry.clut != clut) return entry.clut < clut ? -1 : 1;
	if (entry.bitmap != bitmap) return entry.bitmap < bitmap ? -1 : 1;
	if (entry.kind != kind) return entry.kind < kind ? -1 : 1;
	return 0;
}

const PackEntry *find_entry(unsigned collection, unsigned clut, unsigned bitmap,
	unsigned kind)
{
	uint32_t low = 0;
	uint32_t high = EntryCount;
	while (low < high)
	{
		uint32_t middle = low + (high - low) / 2;
		int order = compare_key(Entries[middle], collection, clut, bitmap, kind);
		if (order < 0)
			low = middle + 1;
		else
			high = middle;
	}
	if (low < EntryCount && compare_key(Entries[low], collection, clut, bitmap, kind) == 0)
		return Entries + low;
	return NULL;
}

const PackEntry *matching_entry(unsigned collection, unsigned clut, unsigned bitmap,
	bool glowing, unsigned width, unsigned height)
{
	if (!open_pack())
		return NULL;

	const PackEntry *entry = find_entry(collection, clut, bitmap, glowing ? 1 : 0);
	if (!entry || entry->width != width || entry->height != height ||
	    entry->format != kFormatARGB4444VQTwiddled || !entry->size)
		return NULL;
	return entry;
}
}

bool dc_vq_sprite_available(unsigned collection, unsigned clut, unsigned bitmap,
	bool glowing, unsigned width, unsigned height)
{
	return matching_entry(collection, clut, bitmap, glowing, width, height) != NULL;
}

bool dc_vq_sprite_upload(unsigned collection, unsigned clut, unsigned bitmap,
	bool glowing, unsigned width, unsigned height)
{
	const PackEntry *entry = matching_entry(collection, clut, bitmap, glowing,
	                                        width, height);
	if (!entry)
		return false;
	if (!OGL_TextureAllocationFits(entry->size))
		return false;

	const unsigned long started = dc_ms();
	void *data = dc_read_file_span(PackPath, entry->offset, entry->size);
	ReadMs += dc_ms() - started;
	if (!data)
		return false;

	/* Do not blame this upload for an older GL error. */
	while (glGetError() != GL_NO_ERROR) {}
	glCompressedTexImage2DARB(GL_TEXTURE_2D, 0,
		GL_COMPRESSED_ARGB_4444_VQ_TWID_KOS,
		(GLsizei)width, (GLsizei)height, 0, (GLsizei)entry->size, data);
	free(data);

	if (glGetError() != GL_NO_ERROR)
		return false;

	++UploadCount;
	if (UploadCount == 1 || (UploadCount % 25) == 0)
		dc_trace(53, "vq: %u compressed sprite uploads, %lu ms reading", UploadCount, ReadMs);
	return true;
}

#endif
