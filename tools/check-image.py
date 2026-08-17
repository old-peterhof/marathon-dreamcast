#!/usr/bin/env python3
"""
check-image.py -- read the markers out of a disc image that already exists.

tools/verify-image.sh can only inspect the tree currently staged in disc/, so it
cannot answer "what is actually inside the .cdi I burned last week", and it is
blind to any target that stages a marker and removes it again -- which cdi-debug
and cdi-profile both do.

    tools/check-image.py alephone-b66-bind-screen.cdi
    tools/check-image.py *.cdi

Exit status is 1 if any image carries a marker that does not belong on hardware,
which is every marker except PROFILE -- that one is deliberate on a profiling
image and only turns on the VMU framerate display.

WHY THIS PARSES RECORDS INSTEAD OF SEARCHING FOR NAMES

The first version of this searched the first 60MB for each marker name followed
by ";1" or a NUL byte, and it was wrong in both directions.

It reported markers that were not there. The port's own source contains
"/cd/AlephOne/AUTOSTART", "/cd/AlephOne/PADTEST" and "/cd/AlephOne/DEBUG" as C
string literals, which land NUL-terminated in the executable's .rodata -- exactly
the pattern it looked for. Every image carries the executable, so every image
looked marked, including a clean `make cdi`.

And it reported clean images it had never actually examined. A padded hardware
image is 740MB and it read 60MB, so where mkdcdisc happened to place the payload
decided the answer. At least one image reported "ok (no markers)" while the
scanned window contained no part of the game at all.

A gate that fires when it should not is a gate that gets ignored, and then it
does not fire when it should. So this looks for the only thing that actually
proves a file is on the disc: a well-formed ISO9660 directory record naming it.

    offset  field
    0       length of this record
    32      length of the file identifier
    33      the identifier, e.g. "AUTOSTART;1"

A candidate is accepted only if the two length bytes in front of the name agree
with it. A string in .rodata has no such header and is rejected. Joliet stores
the same names as UTF-16BE, so both encodings are checked -- mkdcdisc writes a
Joliet tree, and a marker present only there would still be a file on the disc.
"""

import re
import sys

MARKERS = ["AUTOSTART", "PADTEST", "DEBUG", "PROFILE", "AUTOKEY"]
HARMLESS = {"PROFILE"}

# Read in chunks, overlapping by more than one directory record, so a record
# straddling a chunk boundary is still seen whole. Records are at most 255 bytes.
CHUNK = 8 * 1024 * 1024
OVERLAP = 512


def _records_for(data, name):
    """Offsets in `data` of valid directory records naming `name`.

    Checked in both the ASCII and the Joliet (UTF-16BE) encodings.
    """
    hits = []

    for ident in (name.encode("ascii") + b";1",
                  (name + ";1").encode("utf-16-be")):
        for m in re.finditer(re.escape(ident), data):
            at = m.start()

            # The identifier sits 33 bytes into its record, so the record's own
            # length byte and the identifier length byte are just in front of it.
            if at < 33:
                continue

            len_dr = data[at - 33]
            len_fi = data[at - 1]

            if len_fi != len(ident):
                continue

            # A record is at least its 33-byte header plus the identifier, and
            # the field is a single byte.
            if len_dr < 33 + len_fi or len_dr > 255:
                continue

            hits.append(at)

    return hits


def markers_in(path):
    found = set()
    tail = b""

    with open(path, "rb") as f:
        while True:
            chunk = f.read(CHUNK)

            if not chunk:
                break

            data = tail + chunk

            for m in MARKERS:
                if m not in found and _records_for(data, m):
                    found.add(m)

            tail = data[-OVERLAP:]

    return [m for m in MARKERS if m in found]


def main(argv):
    if len(argv) < 2:
        print(__doc__.strip())
        return 2

    bad = 0

    for path in argv[1:]:
        try:
            found = markers_in(path)
        except OSError as e:
            print("%-46s could not read: %s" % (path, e))
            bad = 1
            continue

        unfit = [m for m in found if m not in HARMLESS]

        if unfit:
            print("%-46s NOT FIT FOR HARDWARE: %s" % (path, ", ".join(unfit)))
            bad = 1
        elif found:
            print("%-46s ok (%s)" % (path, ", ".join(found)))
        else:
            print("%-46s ok (no markers)" % path)

    return bad


if __name__ == "__main__":
    sys.exit(main(sys.argv))
