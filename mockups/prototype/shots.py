#!/usr/bin/env python3
"""Render screens to PNG, alone on the page, optionally stacked into a sheet.

    python3 shots.py                       every screen, one PNG each
    python3 shots.py credits term          just those
    python3 shots.py --sheet credits term  stacked into sheet.png
    python3 shots.py --sheet --cols=3      all of them, three across
    python3 shots.py --overscan ...        dim the 40px a TV eats

Writes into shots/. Chrome renders the .screen element alone at 640x480.
"""
import os, re, struct, subprocess, sys, zlib

HERE   = os.path.dirname(os.path.abspath(__file__))
OUT    = os.path.join(HERE, 'shots')
CHROME = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
SCREENS = ['main','difficulty','saves','prefs','prefs-sound','prefs-controls',
           'controller','controller:adv','credits','term','pause',
           'modal:delete','modal:overwrite','modal:quit','modal:save',
           'overlay:capture']

BARE = """
<style>
  .bar,.side,.legend{display:none!important}
  .wrap{padding:0!important} .cols{gap:0!important}
  html,body{background:#000!important}
</style>
<script>
window.addEventListener('load', () => {
  const name = %r;
  /* Overlays get the longest content they can ever hold, so a render shows the
     worst case rather than the tidiest one. */
  const longestSave = () => itemsOf('saves')
    .filter(r => !r.classList.contains('empty'))
    .sort((a,b) => b.querySelector('.nm').textContent.length
                 - a.querySelector('.nm').textContent.length)[0];
  const OPEN = {
    'modal:delete':    () => { S.screen='saves'; paint(); confirmDelete(longestSave()); },
    'modal:overwrite': () => { S.screen='saves'; paint(); confirmOverwrite(longestSave()); },
    'modal:quit':      () => { S.screen='pause'; paint(); confirmQuit(); },
    'modal:save':      () => { S.screen='pause'; paint(); saveGame(); },
    'overlay:capture': () => { S.screen='controller'; S.page='main';
                               S.cursor.controller=0; paint();
                               beginCapture(itemsOf('controller').sort((a,b) =>
                                 b.textContent.trim().length - a.textContent.trim().length)[0]); },
  };
  S.modal = null; S.capture = null;
  if (OPEN[name]) { OPEN[name](); }
  else { const [scr, pg] = name.split(':');
         S.screen = scr; S.page = pg || 'main'; S.cursor[scr] = 0; paint(); }
});
</script>
"""

def shot(name, overscan=False):
    html = open(os.path.join(HERE, 'index.html')).read()
    html = html.replace('<script src="app.js"></script>',
                        '<script src="app.js"></script>' + BARE % name)
    tmp = os.path.join(HERE, '_shot.html')
    open(tmp, 'w').write(html)
    png = os.path.join(OUT, name.replace(':', '-') + '.png')
    try:
        subprocess.run([CHROME, '--headless', '--disable-gpu', '--hide-scrollbars',
                        '--force-device-scale-factor=1', '--window-size=640,480',
                        '--virtual-time-budget=2500', '--screenshot=' + png,
                        'file://' + tmp], capture_output=True, timeout=90)
    finally:
        os.remove(tmp)
    if overscan:
        dim_edges(png)
    return png

# ── minimal PNG read/write, so this needs nothing installed ────────────────
def read_png(p):
    d = open(p, 'rb').read(); i = 8; idat = b''; w = h = ct = None
    while i < len(d):
        ln = struct.unpack('>I', d[i:i+4])[0]; typ = d[i+4:i+8]
        if typ == b'IHDR': w, h, _, ct = struct.unpack('>IIBB', d[i+8:i+18])
        elif typ == b'IDAT': idat += d[i+8:i+8+ln]
        i += 12 + ln
    raw = zlib.decompress(idat)
    ch = {0:1, 2:3, 4:2, 6:4}[ct]; stride = w * ch
    out = bytearray(); prev = bytearray(stride); pos = 0
    for _ in range(h):
        f = raw[pos]; pos += 1
        line = bytearray(raw[pos:pos+stride]); pos += stride
        for x in range(stride):
            a = line[x-ch] if x >= ch else 0
            b = prev[x]; c = prev[x-ch] if x >= ch else 0
            if   f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a+b)//2) & 255
            elif f == 4:
                pp = a + b - c
                pa, pb, pc = abs(pp-a), abs(pp-b), abs(pp-c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += line; prev = line
    rgb = bytearray()
    for k in range(0, len(out), ch): rgb += out[k:k+3] if ch >= 3 else out[k:k+1]*3
    return w, h, bytes(rgb)

def write_png(p, w, h, rgb):
    raw = bytearray()
    for y in range(h):
        raw.append(0); raw += rgb[y*w*3:(y+1)*w*3]
    def ck(t, b):
        return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t+b) & 0xffffffff)
    open(p, 'wb').write(b'\x89PNG\r\n\x1a\n'
        + ck(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
        + ck(b'IDAT', zlib.compress(bytes(raw), 6)) + ck(b'IEND', b''))

def dim_edges(p, margin=40, keep=0.30):
    w, h, rgb = read_png(p); d = bytearray(rgb)
    for y in range(h):
        for x in range(w):
            if margin <= x < w-margin and margin <= y < h-margin: continue
            o = (y*w+x)*3
            for c in range(3): d[o+c] = int(d[o+c] * keep)
    write_png(p, w, h, bytes(d))

def sheet(paths, out, cols=1, gap=8):
    """Tile the renders into one image, `cols` across."""
    imgs = [read_png(p) for p in paths]
    cw = max(i[0] for i in imgs); ch = max(i[1] for i in imgs)
    rows = (len(imgs) + cols - 1) // cols
    w = cols*cw + gap*(cols-1); h = rows*ch + gap*(rows-1)
    buf = bytearray(w*h*3)
    for n, (iw, ih, rgb) in enumerate(imgs):
        cx = (n % cols) * (cw + gap); cy = (n // cols) * (ch + gap)
        for y in range(ih):
            o = ((cy+y)*w + cx)*3
            buf[o:o+iw*3] = rgb[y*iw*3:(y+1)*iw*3]
    write_png(out, w, h, bytes(buf))
    return out

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    args = sys.argv[1:]
    make_sheet = '--sheet' in args;  args = [a for a in args if a != '--sheet']
    cols = 1
    for a in list(args):
        if a.startswith('--cols='):
            cols = int(a.split('=')[1]); args.remove(a)
    over       = '--overscan' in args; args = [a for a in args if a != '--overscan']
    names = args or SCREENS
    paths = [shot(n, over) for n in names]
    print('\n'.join(paths))
    if make_sheet:
        print(sheet(paths, os.path.join(OUT, 'sheet.png'), cols=cols))
