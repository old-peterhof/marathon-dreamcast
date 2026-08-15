#!/bin/bash
#
# new-build.sh -- start a new, identifiable build.
#
#   tools/new-build.sh <slug> "<one line describing what changed>"
#
# Bumps the build number, sets a short human name, and appends a row to
# BUILDS.md recording the number, name, git commit and description.
#
# Every image is then named alephone-b<N>-<slug>.cdi, and the same "b<N> <slug>"
# is drawn on the main menu, so a build can be identified from the filename, from
# the TV, or from the log without guessing.

set -e
cd "$(dirname "$0")/.."

SLUG="${1:?usage: new-build.sh <slug> \"<description>\"}"
DESC="${2:?usage: new-build.sh <slug> \"<description>\"}"

# slug goes in a filename and on a 640-pixel-wide screen: keep it short and plain
if ! echo "$SLUG" | grep -qE '^[a-z0-9][a-z0-9-]{0,20}$'; then
	echo "slug must be lowercase letters, digits and dashes, 21 chars or fewer" >&2
	exit 1
fi

NUM=$(( $(cat BUILD_NUMBER 2>/dev/null || echo 0) + 1 ))
echo "$NUM"  > BUILD_NUMBER
echo "$SLUG" > BUILD_NAME

if [ ! -f BUILDS.md ]; then
	cat > BUILDS.md <<'HDR'
# Builds

Every disc image is named `alephone-b<N>-<slug>.cdi`, and the same `b<N> <slug>`
is drawn at the bottom of the main menu. Quote either when reporting how a build
behaved and it is unambiguous which one you mean.

| Build | Name | Commit | What changed |
|-------|------|--------|--------------|
HDR
fi

printf '| b%s | %s | %s | %s |\n' \
	"$NUM" "$SLUG" "$(git rev-parse --short HEAD 2>/dev/null || echo '-')" "$DESC" >> BUILDS.md

echo "now building b$NUM ($SLUG)"
