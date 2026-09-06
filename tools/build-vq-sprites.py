#!/usr/bin/env python3
"""Build a Dreamcast-native VQ texture pack from Marathon's Shapes file.

The game data is 8-bit indexed art inside a Shapes container, not a directory
of BMP files.  This tool expands only object/scenery collections through their
own CLUT, pads them exactly as OGL_Textures.cpp does, and asks KOS's host-side
pvrtex utility to encode full-codebook ARGB4444 VQ data.

Only entries whose VQ payload is smaller than the equivalent 16-bit texture
are kept.  Textures with MML overrides are omitted so the runtime GL path can
continue to apply those effects exactly.
"""

from __future__ import print_function

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


PACK_MAGIC = b"A1VQ"
PACK_VERSION = 1
PACK_HEADER = struct.Struct("<4sIIII12x")       # 32 bytes
PACK_ENTRY = struct.Struct("<HHHHHHIII8x")      # 32 bytes
DT_HEADER = struct.Struct("<4sIBBBBHHIIII")     # 32 bytes

MAXIMUM_COLLECTIONS = 32
COLLECTION_HEADER_SIZE = 32
COLLECTION_OBJECT = 2
COLLECTION_SCENERY = 4
BITMAP_COLUMN_ORDER = 0x8000
BITMAP_RLE = -1
SELF_LUMINESCENT = 0x80

KIND_NORMAL = 0
KIND_GLOW = 1
FORMAT_ARGB4444_VQ_TWIDDLED = 1


def be16(data, off, signed=False):
    return struct.unpack_from(">h" if signed else ">H", data, off)[0]


def be32(data, off, signed=False):
    return struct.unpack_from(">i" if signed else ">I", data, off)[0]


def next_power_of_two(value):
    result = 1
    while result < value:
        result <<= 1
    return result


def quantize_555(value):
    five = (value >> 11) & 31
    return (five << 3) | (five >> 2)


def load_mml_overrides(mml_dir):
    """Return (collection, bitmap) keys whose pixels/alpha MML can alter."""
    overridden = set()
    if not os.path.isdir(mml_dir):
        return overridden
    for name in os.listdir(mml_dir):
        if not name.lower().endswith(".mml"):
            continue
        path = os.path.join(mml_dir, name)
        try:
            with open(path, "rb") as stream:
                source = stream.read()
        except OSError as exc:
            print("warning: cannot read {}: {}".format(path, exc), file=sys.stderr)
            continue
        if b"<texture" not in source:
            continue
        try:
            root = ET.fromstring(source)
        except (ET.ParseError, OSError) as exc:
            print("warning: cannot parse {}: {}".format(path, exc), file=sys.stderr)
            continue
        for node in root.iter("texture"):
            try:
                collection = int(node.attrib["coll"])
                bitmap = int(node.attrib["bitmap"])
            except (KeyError, ValueError):
                continue
            # Any matching texture directive is conservatively left to the
            # normal runtime path.  This includes opacity and substitute art.
            overridden.add((collection, bitmap))
    return overridden


def parse_collection_headers(data):
    headers = []
    for collection in range(MAXIMUM_COLLECTIONS):
        off = collection * COLLECTION_HEADER_SIZE
        offset = be32(data, off + 4, signed=True)
        length = be32(data, off + 8, signed=True)
        offset16 = be32(data, off + 12, signed=True)
        length16 = be32(data, off + 16, signed=True)
        # This mirrors load_collection(): most object collections contain only
        # one indexed representation and therefore leave offset16 at -1.
        headers.append((offset16, length16) if offset16 != -1 else (offset, length))
    return headers


def parse_collection(data, collection, offset, length):
    if offset < 0 or length <= 0 or offset + length > len(data):
        return None

    base = offset
    collection_type = be16(data, base + 2, signed=True)
    color_count = be16(data, base + 6, signed=True)
    clut_count = be16(data, base + 8, signed=True)
    color_table_offset = be32(data, base + 10, signed=True)
    bitmap_count = be16(data, base + 26, signed=True)
    bitmap_table_offset = be32(data, base + 28, signed=True)

    if collection_type not in (COLLECTION_OBJECT, COLLECTION_SCENERY):
        return None
    if not (0 < color_count <= 256 and 0 < clut_count <= 8 and bitmap_count > 0):
        raise ValueError("collection {} has invalid counts".format(collection))

    cluts = []
    colors_base = base + color_table_offset
    for clut in range(clut_count):
        palette = [(0, 0, 0, 0, False)] * 256
        for i in range(color_count):
            pos = colors_base + (clut * color_count + i) * 8
            flags = data[pos]
            value = data[pos + 1]
            red = quantize_555(be16(data, pos + 2))
            green = quantize_555(be16(data, pos + 4))
            blue = quantize_555(be16(data, pos + 6))
            palette[value] = (red, green, blue, 255, bool(flags & SELF_LUMINESCENT))
        palette[0] = (0, 0, 0, 0, False)
        cluts.append(palette)

    bitmap_offsets = [
        be32(data, base + bitmap_table_offset + i * 4)
        for i in range(bitmap_count)
    ]
    bitmaps = []
    for bitmap, relative in enumerate(bitmap_offsets):
        pos = base + relative
        width = be16(data, pos, signed=True)
        height = be16(data, pos + 2, signed=True)
        bytes_per_row = be16(data, pos + 4, signed=True)
        flags = be16(data, pos + 6)
        bit_depth = be16(data, pos + 8, signed=True)
        if width <= 0 or height <= 0 or bit_depth != 8:
            raise ValueError("collection {} bitmap {} has invalid geometry".format(collection, bitmap))

        rows = width if flags & BITMAP_COLUMN_ORDER else height
        columns = height if flags & BITMAP_COLUMN_ORDER else width
        pixels_pos = pos + 26 + (rows + 1) * 4
        strips = []
        cursor = pixels_pos
        if bytes_per_row == BITMAP_RLE:
            for _ in range(rows):
                first = be16(data, cursor)
                last = be16(data, cursor + 2)
                cursor += 4
                if first > last or last > columns:
                    raise ValueError("collection {} bitmap {} has invalid RLE".format(collection, bitmap))
                strips.append((first, bytes(data[cursor:cursor + last - first])))
                cursor += last - first
        else:
            if bytes_per_row < columns:
                raise ValueError("collection {} bitmap {} has a short row".format(collection, bitmap))
            for _ in range(rows):
                strips.append((0, bytes(data[cursor:cursor + columns])))
                cursor += bytes_per_row
        bitmaps.append((width, height, flags, strips))

    return cluts, bitmaps


def sprite_rgba(bitmap, palette, glow):
    width, height, flags, strips = bitmap
    # OGL_Textures transposes column-order sprite data: source bitmap height is
    # the GL width and source width is the GL height.
    base_width = height if flags & BITMAP_COLUMN_ORDER else width
    base_height = width if flags & BITMAP_COLUMN_ORDER else height
    tex_width = next_power_of_two(base_width + 2)
    tex_height = next_power_of_two(base_height + 2)
    xoff = (tex_width - base_width) // 2
    yoff = (tex_height - base_height) // 2
    pixels = bytearray(tex_width * tex_height * 4)

    for row, (first, values) in enumerate(strips):
        for column, index in enumerate(values):
            if flags & BITMAP_COLUMN_ORDER:
                x = xoff + first + column
                y = yoff + row
            else:
                x = xoff + first + column
                y = yoff + row
            red, green, blue, alpha, self_luminous = palette[index]
            if glow:
                if not self_luminous or max(red, green, blue) < 15:
                    continue
                alpha = 127
            dest = (y * tex_width + x) * 4
            pixels[dest:dest + 4] = bytes((red, green, blue, alpha))
    return tex_width, tex_height, pixels


def write_tga(path, width, height, rgba):
    # Uncompressed 32-bit TGA.  Top-left origin keeps byte row zero identical
    # to the buffer Aleph One currently hands to glTexImage2D.
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0,
                         width, height, 32, 0x28)
    bgra = bytearray(len(rgba))
    for i in range(0, len(rgba), 4):
        bgra[i:i + 4] = bytes((rgba[i + 2], rgba[i + 1], rgba[i], rgba[i + 3]))
    with open(path, "wb") as stream:
        stream.write(header)
        stream.write(bgra)


def encode_vq(pvrtex, temp_dir, key, width, height, rgba):
    stem = "c{:02d}-p{:02d}-b{:03d}-k{}".format(*key)
    source = os.path.join(temp_dir, stem + ".tga")
    output = os.path.join(temp_dir, stem + ".dt")
    write_tga(source, width, height, rgba)
    command = [pvrtex, "-i", source, "-o", output, "-f", "argb4444", "-c", "256"]
    result = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    os.unlink(source)
    if result.returncode != 0:
        raise RuntimeError("pvrtex failed for {}: {}".format(stem, result.stderr.decode("utf-8", "replace")))

    with open(output, "rb") as stream:
        encoded = stream.read()
    os.unlink(output)
    if len(encoded) < DT_HEADER.size:
        raise ValueError("short DT output for {}".format(stem))
    fields = DT_HEADER.unpack_from(encoded)
    magic, chunk_size, version, header_units, codebook_minus_one, _colors, out_w, out_h, pvr_type = fields[:9]
    header_size = (header_units + 1) * 32
    if magic != b"DcTx" or chunk_size > len(encoded) or header_size > chunk_size:
        raise ValueError("invalid DT output for {}".format(stem))
    if codebook_minus_one != 255:
        raise ValueError("{} did not receive a full VQ codebook".format(stem))
    if out_w != width or out_h != height:
        raise ValueError("pvrtex resized {} unexpectedly".format(stem))
    if not (pvr_type & (1 << 30)) or (pvr_type & (1 << 26)):
        raise ValueError("{} is not twiddled VQ".format(stem))
    return encoded[header_size:chunk_size]


def align(value, boundary):
    return (value + boundary - 1) & ~(boundary - 1)


def build_pack(args):
    with open(args.shapes, "rb") as stream:
        shapes = stream.read()
    overrides = load_mml_overrides(args.mml)
    entries = []
    skipped_small = 0
    encoded_count = 0
    raw_total = 0
    vq_total = 0
    temp_dir = tempfile.mkdtemp(prefix="alephone-vq-")
    try:
        for collection, (offset, length) in enumerate(parse_collection_headers(shapes)):
            parsed = parse_collection(shapes, collection, offset, length)
            if parsed is None:
                continue
            cluts, bitmaps = parsed
            for bitmap_index, bitmap in enumerate(bitmaps):
                if (collection, bitmap_index) in overrides:
                    continue
                for clut_index, palette in enumerate(cluts):
                    has_glow = any(color[4] and max(color[:3]) >= 15 for color in palette)
                    for kind in ((KIND_NORMAL, KIND_GLOW) if has_glow else (KIND_NORMAL,)):
                        width, height, rgba = sprite_rgba(bitmap, palette, kind == KIND_GLOW)
                        raw_size = width * height * 2
                        key = (collection, clut_index, bitmap_index, kind)
                        # GLdc's compressed texture entry point accepts no
                        # dimension below 8; these tiny frames are cheaper raw
                        # after allocator rounding in any case.
                        if width < 8 or height < 8:
                            skipped_small += 1
                            continue
                        payload = encode_vq(args.pvrtex, temp_dir, key, width, height, rgba)
                        encoded_count += 1
                        # GLdc allocates in 256-byte units; compare like with like.
                        if align(len(payload), 256) >= align(raw_size, 256):
                            skipped_small += 1
                            continue
                        entries.append((key, width, height, payload))
                        raw_total += raw_size
                        vq_total += len(payload)
                        if encoded_count % 100 == 0:
                            print("encoded {} candidates, kept {}".format(encoded_count, len(entries)))
    finally:
        shutil.rmtree(temp_dir)

    entries.sort(key=lambda item: item[0])
    index_end = PACK_HEADER.size + len(entries) * PACK_ENTRY.size
    data_start = align(index_end, 2048)
    cursor = data_start
    records = []
    for key, width, height, payload in entries:
        cursor = align(cursor, 2048)
        records.append((key, width, height, cursor, payload))
        cursor += len(payload)

    output_dir = os.path.dirname(os.path.abspath(args.output))
    os.makedirs(output_dir, exist_ok=True)
    with open(args.output, "wb") as stream:
        stream.write(PACK_HEADER.pack(PACK_MAGIC, PACK_VERSION, len(records), PACK_ENTRY.size, data_start))
        for key, width, height, offset, payload in records:
            collection, clut, bitmap, kind = key
            stream.write(PACK_ENTRY.pack(collection, clut, bitmap, kind,
                                         width, height, offset, len(payload),
                                         FORMAT_ARGB4444_VQ_TWIDDLED))
        stream.write(b"\0" * (data_start - stream.tell()))
        for _key, _width, _height, offset, payload in records:
            stream.write(b"\0" * (offset - stream.tell()))
            stream.write(payload)
        # The runtime deliberately reads each blob through the following
        # sector boundary.  Interior gaps already supply this padding; make
        # the final entry obey the same rule.
        stream.write(b"\0" * (align(stream.tell(), 2048) - stream.tell()))

    print("{}: {} VQ entries, {:.1f} KiB payload, {:.1f} KiB raw equivalent".format(
        args.output, len(records), vq_total / 1024.0, raw_total / 1024.0))
    print("skipped {} candidates that did not beat raw 16-bit".format(skipped_small))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--shapes", required=True)
    parser.add_argument("--mml", required=True)
    parser.add_argument("--pvrtex", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    build_pack(args)


if __name__ == "__main__":
    main()
