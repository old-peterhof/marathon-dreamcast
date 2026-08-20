#!/usr/bin/env python3
"""Check every screen against the rules in UI-HANDOFF.md.

    python3 check.py            # all screens
    python3 check.py prefs      # one screen

Two passes. A static pass over app.css for one-pixel horizontals, and a live
pass in Chrome that walks the real DOM of each screen for safe-area breaches,
one-scanline horizontals and undersized icons. Mark a deliberate exception with
a trailing /* 480i-ok */ comment on the CSS line. Exit status is 1 on failure,
so this can gate anything.
"""
import json, os, re, subprocess, sys, tempfile

HERE   = os.path.dirname(os.path.abspath(__file__))
CHROME = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
SCREENS = ['main','difficulty','saves','prefs','prefs-sound','prefs-controls',
           'controller','controller:adv','credits','term','pause',
           # overlay states — these draw over a screen and had never been checked
           'modal:delete','modal:overwrite','modal:quit','modal:save',
           'overlay:capture']

WAIVED = {}

RED, GRN, YEL, DIM, OFF = '\033[31m','\033[32m','\033[33m','\033[2m','\033[0m'

def css_pass():
    """One-pixel horizontals in the stylesheet, including pseudo-elements the
       live pass cannot measure."""
    bad = []
    pats = [(r'height:\s*1px', 'height:1px'),
            (r'border-top:\s*1px', 'border-top:1px'),
            (r'border-bottom:\s*1px', 'border-bottom:1px'),
            (r'border:\s*1px', 'border:1px (includes top and bottom)')]
    for n, line in enumerate(open(os.path.join(HERE, 'app.css')), 1):
        if '480i-ok' in line:
            continue
        for pat, why in pats:
            if re.search(pat, line):
                bad.append((n, why, line.strip()[:74]))
    return bad

def contrast_pass():
    """Small text on a composite CRT at couch distance is the first thing to go,
       so every token pair the design actually uses is measured here. Floors are
       4.5:1, dropping to 3.0:1 for text at 17px and above."""
    css = open(os.path.join(HERE, 'app.css')).read()
    tok = dict(re.findall(r'--([\w-]+):\s*(#[0-9a-fA-F]{6})', css))
    tok.setdefault('plate', '#0f1a1a')          # the gradient's lightest region

    def lin(v):
        v /= 255
        return v/12.92 if v <= 0.03928 else ((v+0.055)/1.055)**2.4
    def lum(h):
        h = h.lstrip('#')
        r, g, b = (int(h[i:i+2], 16) for i in (0, 2, 4))
        return 0.2126*lin(r) + 0.7152*lin(g) + 0.0722*lin(b)
    def ratio(a, b):
        la, lb = lum(a), lum(b)
        hi, lo = max(la, lb), min(la, lb)
        return (hi + 0.05) / (lo + 0.05)

    PAIRS = [
        ('menu row, unselected',  'item',  'panel',   18),
        ('menu row, selected',    'hot',   'hot-bar', 18),
        ('screen title',          'face',  'plate',   15),
        ('modal body',            'face',  'panel',   17),
        ('panel caption',         'label', 'panel',   11),
        ('row value',             'label', 'panel',   11),
        ('explainer line',        'label', 'plate',   11),
        ('modal note',            'label', 'panel',   11),
        ('hint bar label',        'label', 'plate',   11),
        ('hint glyph letter',     'item',  'plate',    9),
        ('button counter',        'item',  'panel',   11),
    ]
    bad = []
    for name, fg, bg, px in PAIRS:
        if fg not in tok or bg not in tok:
            bad.append((name, None, px, f'no token --{fg if fg not in tok else bg}'))
            continue
        r = ratio(tok[fg], tok[bg])
        floor = 3.0 if px >= 17 else 4.5
        if r < floor:
            bad.append((name, r, px, f'{tok[fg]} on {tok[bg]}, floor {floor}'))
    return bad

def live_pass(only=None):
    html = open(os.path.join(HERE, 'index.html')).read()
    runner = """
<script src="check.js"></script>
<script>
window.addEventListener('load', () => {
  const want = %s;
  const all = [];
  /* Every overlay is opened through the same call the pad would make, and each
     is handed the longest content it can ever hold, so a name that only just
     fits is caught here rather than on a TV. */
  const longestSave = () => {
    const rows = itemsOf('saves').filter(r => !r.classList.contains('empty'));
    return rows.sort((a, b) => b.querySelector('.nm').textContent.length
                             - a.querySelector('.nm').textContent.length)[0];
  };
  const OPEN = {
    'modal:delete':    () => { S.screen='saves'; paint(); confirmDelete(longestSave()); },
    'modal:overwrite': () => { S.screen='saves'; paint(); confirmOverwrite(longestSave()); },
    'modal:quit':      () => { S.screen='pause'; paint(); confirmQuit(); },
    'modal:save':      () => { S.screen='pause'; paint(); saveGame(); },
    'overlay:capture': () => { S.screen='controller'; S.page='main';
                               S.cursor.controller = 0; paint();
                               /* the longest action label of the twenty */
                               const rows = itemsOf('controller');
                               beginCapture(rows.sort((a,b) =>
                                 b.textContent.trim().length - a.textContent.trim().length)[0]); },
  };
  for (const name of want) {
    S.modal = null; S.capture = null;
    if (OPEN[name]) {
      OPEN[name]();
    } else {
      const [scr, pg] = name.split(':');
      S.screen = scr; S.page = pg || 'main'; S.cursor[scr] = 0;
      paint();
    }
    const r = window.__audit(); r.screen = name; all.push(r);
  }
  const p = document.createElement('pre');
  p.id = 'AUDIT'; p.textContent = JSON.stringify(all);
  document.body.prepend(p);
});
</script>
""" % json.dumps([only] if only else SCREENS)
    html = html.replace('<script src="app.js"></script>',
                        '<script src="app.js"></script>' + runner)
    path = os.path.join(HERE, '_audit.html')
    open(path, 'w').write(html)
    try:
        dom = subprocess.run(
            [CHROME, '--headless', '--disable-gpu', '--virtual-time-budget=4000',
             '--hide-scrollbars', '--force-device-scale-factor=1', '--dump-dom',
             'file://' + path],
            capture_output=True, text=True, timeout=90).stdout
    finally:
        os.remove(path)
    m = re.search(r'<pre id="AUDIT">(.*?)</pre>', dom, re.S)
    if not m:
        print(f'{RED}the page did not run — check for a JS error{OFF}')
        sys.exit(2)
    import html as H
    return json.loads(H.unescape(m.group(1)))

def main():
    only = sys.argv[1] if len(sys.argv) > 1 else None
    fails = 0

    print(f'\n{DIM}── app.css: one-pixel horizontals ──{OFF}')
    css = css_pass()
    if css:
        fails += len(css)
        for n, why, line in css:
            print(f'  {RED}app.css:{n}{OFF}  {why}\n      {DIM}{line}{OFF}')
    else:
        print(f'  {GRN}none{OFF}')

    print(f'\n{DIM}── contrast ──{OFF}')
    con = contrast_pass()
    if con:
        fails += len(con)
        for name, r, px, why in con:
            shown = f'{r:.2f}' if r else '—'
            print(f'  {RED}{shown:>5}{OFF}  {name} at {px}px  {DIM}{why}{OFF}')
    else:
        print(f'  {GRN}every pair clears its floor{OFF}')

    print(f'\n{DIM}── screens ──{OFF}')
    for r in live_pass(only):
        waived = []
        for sub, why in WAIVED.get(r['screen'], []):
            for kind in ('safe','thin','icon','hit','over'):
                keep = []
                for v in r[kind]:
                    if sub in json.dumps(v): waived.append((sub, why))
                    else: keep.append(v)
                r[kind] = keep
        n = len(r['safe']) + len(r['thin']) + len(r['icon']) + len(r['hit']) + len(r['over'])
        fails += n
        mark = f'{GRN}ok{OFF}' if not n else f'{RED}{n}{OFF}'
        if waived: mark += f'  {YEL}{len(waived)} waived{OFF}'
        print(f"  {r['screen']:<16} {mark}")
        for sub, why in waived:
            print(f"      {YEL}waived{OFF}           {sub}  {DIM}{why}{OFF}")
        for v in r['safe']:
            print(f"      {RED}outside 560x400{OFF}  {v['el']}  {v['box']}")
        for v in r['thin']:
            print(f"      {RED}1px horizontal{OFF}   {v['el']}  w={v['w']} y={v['y']}")
        for v in r['icon']:
            print(f"      {RED}icon under 60px{OFF}  w={v['w']}")
        for v in r['over']:
            print(f"      {RED}text overflows{OFF}   {v['el']}  needs {v['need']}px, has {v['has']}px")
        for v in r['hit']:
            print(f"      {RED}labels overlap{OFF}   {v['a']}  x  {v['b']}  by {v['overlap']}")

    print()
    if fails:
        print(f'{RED}{fails} problem(s).{OFF} '
              f'{DIM}Deliberate CSS exceptions take a trailing /* 480i-ok */.{OFF}\n')
        return 1
    print(f'{GRN}All clear.{OFF}\n')
    return 0

if __name__ == '__main__':
    sys.exit(main())
