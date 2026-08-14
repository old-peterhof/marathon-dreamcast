#!/bin/bash
#
# build.sh -- Aleph One for Dreamcast
#
# Sources KallistiOS's environ.sh, then runs Makefile.dc. Use this rather than
# calling make directly: the build needs KOS_BASE, KOS_PORTS, KOS_CFLAGS and the
# kos-cc/kos-c++ wrappers, all of which environ.sh exports.
#
#   ./build.sh              build alephone.elf
#   ./build.sh test         unpadded .cdi (~21MB) for Flycast
#   ./build.sh flycast      build the test image and launch Flycast on it
#   ./build.sh cdi          padded .cdi for burning or GDEMU
#   ./build.sh gdi          .gdi, GDEMU's native format
#   ./build.sh clean        remove objects and images
#
set -e

KOS_ENVIRON=${KOS_ENVIRON:-/opt/toolchains/dc/kos/environ.sh}

if [ ! -f "$KOS_ENVIRON" ]; then
	echo "error: cannot find KallistiOS environ.sh at $KOS_ENVIRON" >&2
	echo "       set KOS_ENVIRON to its path, e.g." >&2
	echo "       KOS_ENVIRON=/path/to/kos/environ.sh ./build.sh" >&2
	exit 1
fi

# shellcheck disable=SC1090
source "$KOS_ENVIRON"

if [ -z "${KOS_BASE:-}" ]; then
	echo "error: sourcing $KOS_ENVIRON did not set KOS_BASE" >&2
	exit 1
fi

cd "$(dirname "$0")"
exec make -f Makefile.dc "$@"
