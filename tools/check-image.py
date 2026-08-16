#!/usr/bin/env python3
"""
check-image.py -- read the markers out of a disc image that already exists.

tools/verify-image.sh can only inspect the tree currently staged in disc/, so it
cannot answer "what is actually inside the .cdi I burned last week", and it is
blind to any target that stages a marker and removes it again -- which cdi-debug
and cdi-profile both do.

This reads the image itself. ISO9660 directory records hold the filename
followed by ";1", so the markers are findable in the raw bytes without mounting
anything.

    tools/check-image.py alephone-b38-falco-flags.cdi
    tools/check-image.py *.cdi

Exit status is 1 if any image carries a marker that does not belong on hardware,
which is every marker except PROFILE -- that one is deliberate on a profiling
image and only turns on the VMU framerate display.
"""

import re
import sys

MARKERS = ["AUTOSTART", "PADTEST", "DEBUG", "PROFILE", "AUTOKEY"]
HARMLESS = {"PROFILE"}

# The markers sit in the ISO directory near the start of the data track; there is
# no need to read a 740MB padded image in full.
SCAN_BYTES = 60 * 1024 * 1024


def markers_in(path):
    with open(path, "rb") as f:
        data = f.read(SCAN_BYTES)

    return [m for m in MARKERS
            if re.search(re.escape(m.encode()) + rb"(;1|\x00)", data)]


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
