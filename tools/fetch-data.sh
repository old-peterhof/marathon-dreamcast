#!/bin/bash
#
# fetch-data.sh -- download the Marathon 2 game data the disc image needs.
#
# Aleph One ships no game data. Bungie made the Marathon trilogy freely
# available in 2005 and granted the Aleph One project a limited distribution
# license in 2021, so the full retail data comes straight from Aleph One's
# GitHub releases. Free for noncommercial use; Bungie retains the copyright.
#
# The modern package names its files Map.sceA / Shapes.shpA / Sounds.sndA /
# Images.imgA. Aleph One 0.12.0 predates those extensions and looks for bare
# names, so they get renamed on the way in. The formats themselves are
# unchanged -- these are the original Bungie data files.
#
# Everything lands in data/, which is gitignored, so the repo stays source-only.

set -e

REL="release-20250829"
ZIP="Marathon2-20250829-Data.zip"
URL="https://github.com/Aleph-One-Marathon/alephone/releases/download/$REL/$ZIP"

cd "$(dirname "$0")/.."
mkdir -p data
ARCHIVE="data/$ZIP"
GAME="data/game"

if [ -f "$GAME/Map" ] && [ -f "$GAME/Shapes" ] && [ -f "$GAME/Sounds" ] && [ -f "$GAME/Images" ]; then
	echo "game data already present in $GAME"
	exit 0
fi

if [ ! -f "$ARCHIVE" ] || ! file "$ARCHIVE" | grep -qi "zip archive"; then
	echo "fetching $URL"
	curl -fSL --retry 3 -o "$ARCHIVE" "$URL"
fi

if ! file "$ARCHIVE" | grep -qi "zip archive"; then
	echo "error: $ARCHIVE is not a zip archive" >&2
	rm -f "$ARCHIVE"
	exit 1
fi

echo "extracting"
rm -rf data/_unzip "$GAME"
mkdir -p data/_unzip "$GAME"
unzip -q "$ARCHIVE" -d data/_unzip

SRC="data/_unzip/Marathon 2"
if [ ! -d "$SRC" ]; then
	echo "error: expected '$SRC' inside $ZIP" >&2
	exit 1
fi

# Bare names, as 0.12.0 expects. Plugins/, Scripts/ and Physics Models/ are
# deliberately left out: they target Aleph One 1.x and its MML dialect, which
# 0.12.0's parser does not understand.
cp "$SRC/Map.sceA"    "$GAME/Map"
cp "$SRC/Shapes.shpA" "$GAME/Shapes"
cp "$SRC/Sounds.sndA" "$GAME/Sounds"
cp "$SRC/Images.imgA" "$GAME/Images"

rm -rf data/_unzip

echo "ok:"
ls -la "$GAME"
