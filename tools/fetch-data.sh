#!/bin/bash
#
# fetch-data.sh -- download the Marathon 2 demo data the disc image needs.
#
# Aleph One ships no game data. BERO's README.DC points at the Marathon 2 demo
# tarball on SourceForge, which Bungie released freely; it carries Images, Map,
# Shapes and Sounds. The MML, Fonts and Themes come from the port's own skeleton
# in disc-AlephOne/.
#
# Run once. The result lands in data/AlephOne-m2-demo.tar.gz and is gitignored,
# so the repo stays source-only.

set -e

URL="https://downloads.sourceforge.net/marathon/AlephOne-m2-demo.tar.gz"
cd "$(dirname "$0")/.."
mkdir -p data
OUT=data/AlephOne-m2-demo.tar.gz

if [ -f "$OUT" ] && file "$OUT" | grep -qi gzip; then
	echo "already present: $OUT"
	exit 0
fi

echo "fetching $URL"
curl -fSL --retry 3 -o "$OUT" "$URL"

if ! file "$OUT" | grep -qi gzip; then
	echo "error: downloaded file is not a gzip archive" >&2
	rm -f "$OUT"
	exit 1
fi

echo "ok: $OUT ($(du -h "$OUT" | cut -f1))"
