#!/usr/bin/env python3
#
# shoot-prototype.py -- render the design prototype's screens to PNG.
#
#   tools/shoot-prototype.py [screen ...]
#
# The prototype in mockups/prototype IS the design. Reading its CSS and
# reimplementing from the numbers produces something that matches your reading of
# the design rather than the design, and the difference is not visible until it
# is on a television. So: render the real thing, and compare against it.
#
# Output goes to mockups/renders/<screen>.png at 640x480, which is exactly the
# framebuffer the game draws into, so a render and a Flycast capture can be put
# side by side or subtracted.
#
# Uses the same headless Chrome as tools/bake-plate.py, and the same reasoning:
# the design is expressed in CSS and SVG, so the thing that renders CSS and SVG
# correctly is the thing that should render it.

import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PROTO = os.path.join(ROOT, "mockups", "prototype")
OUT = os.path.join(ROOT, "mockups", "renders")

CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

WIDTH, HEIGHT = 640, 480

SCREENS = [
    "main", "difficulty", "saves", "prefs", "prefs-sound",
    "prefs-controls", "controller", "credits", "term", "pause",
]


def page(screen):
    """A page showing exactly one screen, with the prototype's own chrome off.

    The prototype wraps every screen in page furniture -- a toolbar, a caption,
    a drop shadow -- that is explanation rather than design. All of it is turned
    off here so what lands in the PNG is only what the Dreamcast would draw.
    """
    return """<!doctype html><meta charset="utf-8">
<base href="file://%s/">
<link rel="stylesheet" href="app.css">
<style>
  html,body{margin:0;padding:0;background:#05080a;overflow:hidden;}
  /* Page furniture only. NOT .cap -- that is also the panel's caption bar,
     and hiding it silently removed a 20px row from every render. */
  .wrap > .bar, .note, h1, h2, .wrap > p {display:none!important;}
  .screen{position:absolute!important;left:0!important;top:0!important;
          box-shadow:none!important;display:block!important;}
  section[data-screen]{display:block!important;}
</style>
<div id="host"></div>
<script>
fetch('index.html').then(r => r.text()).then(html => {
  const doc = new DOMParser().parseFromString(html, 'text/html');

  // The screen itself, plus the <defs> block the wordmark's bloom filter needs
  // -- without it the bloom renders as a hard-edged duplicate.
  const defs  = doc.querySelector('svg[width="0"]');
  // .plate is a SIBLING of the sections, not inside them -- it is the shared
  // background every screen sits on. Leaving it out renders each screen over
  // nothing, which is not what the Dreamcast draws and made the first
  // comparison meaningless.
  const plate = doc.querySelector('.plate');
  const sec   = doc.querySelector('section[data-screen="%s"]');

  const host = document.getElementById('host');
  const div  = document.createElement('div');
  div.className = 'screen';

  if (defs)  div.appendChild(defs.cloneNode(true));
  if (plate) div.appendChild(plate.cloneNode(true));
  if (sec) {
    const s = sec.cloneNode(true);
    s.hidden = false;
    div.appendChild(s);
  }

  // The prototype's JS is not running, so the selection class it would apply
  // has to be applied here -- otherwise every render shows the unselected state
  // and the highlight, which is the thing hardest to get right, never gets
  // compared at all.
  const first = div.querySelector('.row, .srow');
  if (first) first.classList.add('on');

  host.appendChild(div);
  document.title = 'ready';
});
</script>
""" % (PROTO, screen)


def shoot(screen):
    fd, path = tempfile.mkstemp(suffix=".html", dir=PROTO)
    png = os.path.join(OUT, screen + ".png")

    try:
        with os.fdopen(fd, "w") as f:
            f.write(page(screen))

        subprocess.run([
            CHROME,
            "--headless=new",
            "--disable-gpu",
            "--hide-scrollbars",
            "--allow-file-access-from-files",
            "--force-device-scale-factor=1",
            "--virtual-time-budget=3000",
            "--window-size=%d,%d" % (WIDTH, HEIGHT),
            "--screenshot=" + png,
            "file://" + path,
        ], capture_output=True, text=True, timeout=120)
    finally:
        os.unlink(path)

    if not os.path.exists(png):
        return None

    from PIL import Image

    im = Image.open(png).convert("RGB")
    if im.size != (WIDTH, HEIGHT):
        im = im.crop((0, 0, WIDTH, HEIGHT))
        im.save(png)

    return png


def main():
    if not os.path.exists(CHROME):
        raise SystemExit("shoot-prototype: Chrome not found")

    os.makedirs(OUT, exist_ok=True)

    wanted = sys.argv[1:] or SCREENS

    for screen in wanted:
        png = shoot(screen)
        print("%-16s %s" % (screen, png or "FAILED"))


if __name__ == "__main__":
    main()
