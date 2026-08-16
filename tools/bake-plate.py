#!/usr/bin/env python3
#
# bake-plate.py -- render the prototype's background plate to a BMP the game can load.
#
#   tools/bake-plate.py [--check]
#
# The plate is defined in CSS and SVG (mockups/prototype/app.css), so it is baked
# with the renderer that already draws it correctly rather than reimplemented:
# headless Chrome screenshots it, then PIL writes a 24-bit BMP. SDL_LoadBMP is
# core SDL and already used at sdl_dialogs.cpp:545, so this needs no library the
# port does not already link -- unlike PNG, which would mean libpng.
#
# Two plates, because the brand art is main-menu only (every other screen in the
# prototype carries neither wordmark nor watermark):
#
#   plate.bmp       gradient and seam grid -- every screen
#   plate-main.bmp  the same plus the wordmark and the watermark
#
# The markup is lifted out of the built index.html rather than retyped, so the
# baked image cannot drift from the prototype. Re-run build.py first if the
# design has moved.
#
# NO DITHERING anywhere in this path. UI-HANDOFF.md section 1 measured it making
# every band worse: per-pixel noise means every row differs from its neighbour,
# which is precisely what an interlaced display turns into shimmer. The image is
# written at 24-bit and SDL reduces it to 16 at load time, which does not dither.

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PROTO = os.path.join(ROOT, "mockups", "prototype")
# disc/ is generated -- the disc target does `rm -rf disc` and re-copies from
# disc-AlephOne, so anything baked into disc/ is wiped on the next build.
OUTDIR = os.path.join(ROOT, "disc-AlephOne", "UI")

CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

WIDTH, HEIGHT = 640, 480


def extract(html):
	"""Pull the plate's parts out of the built prototype.

	Returns (defs, plate, wordmark, watermark) as raw markup. The filter defs
	block has to come along: the wordmark's bloom layer references #m2blur, and
	without it the bloom renders as a hard-edged duplicate of the letterforms.
	"""
	m = re.search(r'(<svg width="0" height="0".*?</svg>)', html, re.S)
	if not m:
		raise SystemExit("bake-plate: no <defs> block in index.html -- run build.py?")
	defs = m.group(1)

	m = re.search(r'(<div class="plate"></div>)', html)
	if not m:
		raise SystemExit("bake-plate: no .plate div in index.html")
	plate = m.group(1)

	# Both are <svg class="..."> ... </svg>, and both live in the main section.
	def svg(cls):
		m = re.search(r'(<svg class="%s".*?</svg>)' % cls, html, re.S)
		if not m:
			raise SystemExit("bake-plate: no .%s in index.html" % cls)
		return m.group(1)

	return defs, plate, svg("watermark"), svg("wordmark")


def page(defs, plate, brand):
	# position:absolute on .screen means it needs a zeroed body to land at 0,0,
	# and the 1px box-shadow the prototype draws around each screen would print
	# as a border on the baked image, so it is turned off here.
	return """<!doctype html><meta charset="utf-8">
<link rel="stylesheet" href="app.css">
<style>
  html,body{margin:0;padding:0;background:#05080a;}
  .wrap{padding:0;}
  .screen{position:absolute;left:0;top:0;box-shadow:none;}
</style>
<div class="screen">%s%s%s</div>
""" % (defs, plate, brand)


def shoot(html_text, out_png):
	# Chrome will only load app.css over file:// from the same directory, so the
	# temp page is written next to it rather than in /tmp.
	fd, path = tempfile.mkstemp(suffix=".html", dir=PROTO)
	try:
		with os.fdopen(fd, "w") as f:
			f.write(html_text)

		cmd = [
			CHROME,
			"--headless=new",
			"--disable-gpu",
			"--hide-scrollbars",
			"--force-device-scale-factor=1",
			"--window-size=%d,%d" % (WIDTH, HEIGHT),
			"--screenshot=" + out_png,
			"file://" + path,
		]
		r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
		if not os.path.exists(out_png):
			sys.stderr.write(r.stdout + r.stderr)
			raise SystemExit("bake-plate: Chrome produced no screenshot")
	finally:
		os.unlink(path)


def to_bmp(png, bmp):
	from PIL import Image

	im = Image.open(png).convert("RGB")	 # drops alpha, no dithering involved
	if im.size != (WIDTH, HEIGHT):
		# A device-scale-factor mismatch would silently double everything, and a
		# 1280x960 plate scaled down is exactly the soft, shimmering result the
		# interlace work was trying to avoid.
		raise SystemExit("bake-plate: got %dx%d, expected %dx%d"
		                 % (im.size[0], im.size[1], WIDTH, HEIGHT))
	im.save(bmp, "BMP")
	return im


def horizontal_1px_runs(im):
	"""Count rows that differ sharply from both neighbours.

	UI-HANDOFF.md section 1: a 1px horizontal line exists on one interlace field
	only and buzzes at 30Hz. This does not police the design -- it reports, so a
	plate that picks one up in conversion gets noticed rather than shipped.
	"""
	px = im.load()
	bad = 0
	for y in range(1, HEIGHT - 1):
		hits = 0
		for x in range(0, WIDTH, 4):		# every 4th column is plenty
			a, b, c = px[x, y - 1], px[x, y], px[x, y + 1]
			d_up = sum(abs(b[i] - a[i]) for i in range(3))
			d_dn = sum(abs(b[i] - c[i]) for i in range(3))
			if d_up > 40 and d_dn > 40:
				hits += 1
		if hits > (WIDTH / 4) * 0.5:
			bad += 1
	return bad


def main():
	if not os.path.exists(CHROME):
		raise SystemExit("bake-plate: Chrome not found at " + CHROME)

	index = os.path.join(PROTO, "index.html")
	if not os.path.exists(index):
		raise SystemExit("bake-plate: no built prototype -- run build.py first")

	html = open(index, errors="replace").read()
	defs, plate, watermark, wordmark = extract(html)

	os.makedirs(OUTDIR, exist_ok=True)

	targets = [
		("plate.bmp", ""),
		("plate-main.bmp", watermark + wordmark),
	]

	for name, brand in targets:
		png = os.path.join(tempfile.gettempdir(), name.replace(".bmp", ".png"))
		bmp = os.path.join(OUTDIR, name)

		shoot(page(defs, plate, brand), png)
		im = to_bmp(png, bmp)

		runs = horizontal_1px_runs(im)
		note = "  %d single-pixel rows -- see UI-HANDOFF section 1" % runs if runs else ""
		print("%-16s %dx%d  %d bytes%s"
		      % (name, im.size[0], im.size[1], os.path.getsize(bmp), note))


if __name__ == "__main__":
	main()
