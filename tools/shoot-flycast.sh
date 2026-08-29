#!/bin/bash
#
# shoot-flycast.sh -- screenshot the running Flycast window.
#
#   tools/shoot-flycast.sh <out.png> [scale]
#
# Why this exists: for a long time the only evidence about what the GL renderer
# was doing came from serial traces, and "it renders" was inference. It was
# wrong -- the geometry was right and every texture was magenta. Looking is
# cheap once the plumbing is here.
#
# Needs Screen Recording permission for the terminal running it. Without it
# screencapture fails with "could not create image from display" rather than
# producing a black frame, so a failure is obvious rather than misleading.
#
# The window position comes from System Events rather than emu.cfg, which is
# only written when Flycast exits and is therefore stale while it runs.
# screencapture -R takes those same point coordinates, so no backing-scale
# arithmetic is needed -- it hands back device pixels for the region asked for.
set -eu

OUT="${1:?usage: shoot-flycast.sh <out.png> [scale]}"
SCALE="${2:-1}"

if ! pgrep -x Flycast >/dev/null; then
	echo "flycast is not running" >&2
	exit 1
fi

# Raise it first. screencapture -R grabs a screen region, not a window, so a
# Flycast sitting behind the terminal yields a picture of the terminal -- which
# looks like a black frame and reads as "the renderer drew nothing".
osascript -e 'tell application "System Events" to set frontmost of process "Flycast" to true' >/dev/null 2>&1 || true
sleep 1

GEOM=$(osascript -e 'tell application "System Events" to tell process "Flycast" to get {position, size} of window 1')
IFS=', ' read -r WX WY WW WH <<< "$GEOM"

# The window frame includes a title bar; the emulated screen does not.
TITLE=28
screencapture -x -o -R"$WX,$((WY + TITLE)),$WW,$((WH - TITLE))" "$OUT"

if [ "$SCALE" != "1" ]; then
	python3 - "$OUT" "$SCALE" <<'PY'
import sys
from PIL import Image
out, scale = sys.argv[1], float(sys.argv[2])
im = Image.open(out)
im.resize((int(im.size[0]*scale), int(im.size[1]*scale)), Image.LANCZOS).save(out)
PY
fi

python3 -c "
from PIL import Image; import sys
im = Image.open('$OUT'); print('$OUT', im.size)
"
