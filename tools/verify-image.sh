#!/bin/bash
#
# verify-image.sh -- list what is actually inside a built disc image.
#
#   tools/verify-image.sh            # checks the staged disc/AlephOne tree
#
# A hardware image must not contain AUTOSTART, PADTEST or DEBUG. Those markers
# make the game start a level by itself, spin the view with a synthetic stick,
# and draw diagnostics over the screen -- all fine for an emulator run, all
# wrong on a console.
#
# This exists because a corrupted `-include` line in Makefile.dc caused make to
# build the marker-staging target on every invocation, so the markers were
# written into a "clean" image. Grepping the image for the string "DEBUG" did
# not catch it, because the binary itself contains "/cd/AlephOne/DEBUG". The
# only trustworthy check is to read the ISO9660 directory, which is what this
# does: dump the data track, mount it, and list.

set -u

cd "$(dirname "$0")/.."

MKDCDISC=/opt/toolchains/dc/mkdcdisc/build/mkdcdisc
TMP=$(mktemp -d)
ISO="$TMP/verify.iso"

trap 'hdiutil detach "$MP" >/dev/null 2>&1; rm -rf "$TMP"' EXIT

"$MKDCDISC" -e alephone.elf -d disc/AlephOne -o "$TMP/verify.cdi" -I \
	-n "verify" -N -q >/dev/null 2>&1

if [ ! -f "$ISO" ]; then
	echo "could not produce an ISO to inspect" >&2
	exit 1
fi

MP=$(hdiutil attach "$ISO" -nobrowse -readonly 2>/dev/null | tail -1 | awk '{print $NF}')
if [ -z "${MP:-}" ] || [ ! -d "$MP" ]; then
	echo "could not mount the dumped ISO" >&2
	exit 1
fi

echo "Contents of the staged image:"
ls "$MP" | sed 's/^/  /'
echo "  AlephOne/"
ls "$MP/AlephOne" | sed 's/^/    /'
echo

fail=0
for m in AUTOSTART PADTEST DEBUG; do
	if [ -e "$MP/AlephOne/$m" ]; then
		echo "PRESENT: $m -- this image is NOT fit for hardware"
		fail=1
	fi
done

if [ "$fail" -eq 0 ]; then
	echo "No test markers present. Safe for hardware."
fi

exit $fail
