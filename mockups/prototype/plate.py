#!/usr/bin/env python3
"""Bake the static background bitmaps the engine blits behind the menus.

    python3 plate.py

Writes into assets/:

    menu-plate.png    gradient + hull seams; shared by every screen
    main-plate.png    the same, plus the wordmark and watermark; main menu only
    *-565.png         each one quantised to RGB565, undithered, to inspect banding

Nothing dynamic is baked in: no rules, no panels, no text, no scanlines. Rules
and panel chrome are drawn by widgets so a layout change does not need new art.
"""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shots import read_png, write_png

HERE   = os.path.dirname(os.path.abspath(__file__))
OUT    = os.path.join(HERE, 'assets')
CHROME = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'

# Hide everything the widgets will draw themselves. Left standing: .plate, and
# for the main menu the two pieces of brand art.
STRIP = """
  .bar,.side,.legend{display:none!important}
  .wrap{padding:0!important} .cols{gap:0!important}
  html,body{background:#000!important}
  .screen .hr,.screen .panel,.screen .hint,.screen .statecol,.screen .title,
  .screen .kicker,.screen .explain,.screen .savebanner,.screen .credits,
  .screen .term,.screen .scrim,.screen .modal,.screen .capture,.screen .ov,
  .screen .binds{display:none!important}
"""
NO_ART = '.wordmark,.watermark{display:none!important}'

def bake(name, extra_css):
    html = open(os.path.join(HERE, 'index.html')).read()
    inject = f'<style>{STRIP}{extra_css}</style><script>' \
             'window.addEventListener("load",()=>{S.screen="main";paint();});</script>'
    html = html.replace('<script src="app.js"></script>',
                        '<script src="app.js"></script>' + inject)
    tmp = os.path.join(HERE, '_plate.html')
    open(tmp, 'w').write(html)
    png = os.path.join(OUT, name + '.png')
    try:
        subprocess.run([CHROME, '--headless', '--disable-gpu', '--hide-scrollbars',
                        '--force-device-scale-factor=1', '--window-size=640,480',
                        '--virtual-time-budget=2500', '--screenshot=' + png,
                        'file://' + tmp], capture_output=True, timeout=90)
    finally:
        os.remove(tmp)
    return png

# No dithering. A column-only pattern is interlace-safe, but measured where the
# banding actually is — a flat patch of the gradient — it bought one extra colour
# out of seven and raised the error doing it. The whole-image colour count that
# made it look worthwhile was dominated by the wordmark, not the gradient.
def to565(src, dst, dither=None):
    """Quantise to 5-6-5, straight truncation."""
    w, h, rgb = read_png(src)
    out = bytearray(len(rgb))
    for pix in range(len(rgb) // 3):
        i = pix * 3
        off = dither[(pix % w) % len(dither)] if dither else 0
        for c, bits in ((0,5),(1,6),(2,5)):
            step = 1 << (8 - bits)
            v = max(0, min(255, rgb[i+c] + off))
            q = (v // step) * step
            out[i+c] = q | (q >> bits)
    write_png(dst, w, h, bytes(out))
    # how far any pixel moved, and how many distinct steps the ramp still has
    worst = max(abs(rgb[i]-out[i]) for i in range(len(rgb)))
    return w, h, worst, len({bytes(out[i:i+3]) for i in range(0, len(out), 3)})

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for name, css in (('menu-plate', NO_ART), ('main-plate', '')):
        png = bake(name, css)
        q = os.path.join(OUT, name + '-565.png')
        w, h, worst, colours = to565(png, q)
        print(f'{name:<12} {w}x{h}  '
              f'png {os.path.getsize(png)//1024}K  '
              f'565 {os.path.getsize(q)//1024}K  '
              f'worst shift {worst}/255  {colours} colours after quantising  '
              f'surface {w*h*2//1024}K in RGB565')
