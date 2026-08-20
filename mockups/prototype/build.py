#!/usr/bin/env python3
"""Generate index.html for the Marathon 2 DC UI prototype.

Every word the player reads lives in strings.md and is pulled in by key, so copy
can be edited without touching code. Layout lives here, behaviour in app.js,
styling in app.css. Re-run after editing either file:

    python3 build.py && python3 check.py

Settings, defaults and wording follow MENU-TREE.md (b57).
"""
import json, os, re, sys, xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
NS = {'svg': 'http://www.w3.org/2000/svg'}

# ── strings ─────────────────────────────────────────────────────────────────
def load_strings(path):
    out, key, buf = {}, None, []
    def flush():
        if key is not None:
            # <!-- ... --> is a note to whoever edits the file, not content
            v = re.sub(r'<!--.*?-->', '', '\n'.join(buf), flags=re.S)
            out[key] = '\n'.join(l for l in v.splitlines() if l.strip()).strip()
    for line in open(path, encoding='utf-8').read().splitlines():
        m = re.match(r'^###\s+(\S+)\s*$', line)
        if m:
            flush(); key, buf = m.group(1), []
        elif line.startswith('## ') or line.startswith('# ') or line.strip() == '---':
            flush(); key, buf = None, []
        elif key is not None:
            buf.append(line)
    flush()
    return out

STR = load_strings(os.path.join(HERE, 'strings.md'))

def T(k, **kw):
    if k not in STR:
        sys.exit(f'strings.md: no key "{k}"')
    s = STR[k]
    for a, b in kw.items():
        s = s.replace('{' + a + '}', str(b))
    return s

def TL(k):
    return [l.strip() for l in T(k).splitlines() if l.strip()]

def esc(s):
    """strings.md uses [x] for an emphasised run; the design paints it amber."""
    return re.sub(r'\[([^\]]+)\]', r'<span class="hi">\1</span>', s)

# ── brand art ───────────────────────────────────────────────────────────────
WORD_SVG = os.path.expanduser('~/Downloads/Marathon2.svg')
ICON_SVG = os.path.expanduser('~/Downloads/Marathon_Logo.svg')

def wordmark():
    r = ET.parse(WORD_SVG).getroot()
    out = ''
    for cls, gid in (('bloom','g1055'), ('halo','g1055'), ('face','g847'), ('dur','text908')):
        g = r.find(f".//svg:g[@id='{gid}']", NS)
        extra = ' filter="url(#m2blur)"' if cls == 'bloom' else ''
        out += f'<g class="{cls}" transform="{g.get("transform")}"{extra}>'
        for p in g.findall('svg:path', NS):
            tr = f' transform="{p.get("transform")}"' if p.get('transform') else ''
            out += f'<path{tr} d="{p.get("d")}"/>'
        out += '</g>'
    return f'<svg class="wordmark" viewBox="0 0 205.31667 55.033334" data-bleed-all>{out}</svg>'

ICON_D = ET.parse(ICON_SVG).getroot().find('.//svg:path', NS).get('d')
def icon(cls='', bleed=False):
    return (f'<svg class="{cls}" viewBox="0 0 750 750"{" data-bleed-all" if bleed else ""}>'
            f'<g transform="translate(-93.75,-93.75)"><path d="{ICON_D}"/></g></svg>')

# ── row builders ────────────────────────────────────────────────────────────
def attrs(d):
    return ''.join(f' {k}="{v}"' for k, v in d.items() if v is not None)

def row(label, val='', cls='row', **kw):
    v = f'<span class="val">{val}</span>' if val != '' else ''
    return (f'<div class="{cls}" data-nav{attrs(kw)}>'
            f'<span class="car"></span>{label}{v}</div>')

def sec(t):
    return f'<div class="sec">{t}</div>'

def slider(v, mx=8):
    return ('<span class="sld">'
            + ''.join(f'<i class="{"f" if k < v else ""}"></i>' for k in range(mx))
            + '</span>')

ARROW = '<span class="vlabel">&#9654;</span>'

def ctl(label, kind, note, **kw):
    """A preference row: slider, toggle or choice."""
    if kind == 'slider':
        val = slider(int(kw['val']), int(kw.get('max', 8)))
        kw.setdefault('max', 8)
    elif kind == 'toggle':
        val = f'<span class="vlabel">{kw["on"] if int(kw["val"]) else kw["off"]}</span>'
        kw['data_label_off'], kw['data_label_on'] = kw.pop('off'), kw.pop('on')
    else:
        val = f'<span class="vlabel">{kw["opts"][int(kw["val"])]}</span>'
        kw['opts'] = '|'.join(kw['opts'])
    d = {'data-kind': kind, 'data-val': kw.pop('val'), 'data-note': note}
    for k, v in kw.items():
        d['data-' + k.replace('data_', '').replace('_', '-')] = v
    d['data-w'] = {'slider':'w_slider','toggle':'w_toggle','choice':'w_select'}[kind]
    return (f'<div class="row"{attrs(d)} data-nav><span class="car"></span>{label}'
            f'<span class="val">{val}</span></div>')

def panel(inner, w, style, cap=None, rowh=None):
    r = f';--row:{rowh}px' if rowh else ''
    c = f'<div class="cap">{cap}</div>' if cap else ''
    tight = ' tight' if rowh and rowh < 30 else ''
    return (f'<div class="panel{tight}" data-w="w_list" '
            f'style="{style};width:{w}px{r}">{c}{inner}</div>')

def hint(*items):
    return ('<div class="hint">' + ''.join(f'<span>{i}</span>' for i in items)
            + f'<span class="sp">{T("build.label")}</span></div>')

def G(t, rnd=True):
    return f'<i class="glyph{" rnd" if rnd else ""}">{t}</i>'

DPAD = G('&#10011;', False)
LR   = G('&#9664;&#9654;', False)
HR_T = '<div class="hr" style="top:92px;--c:var(--rule-hot)"></div>'
HR_B = '<div class="hr" style="bottom:66px"></div>'
EXPLAIN = '<div class="explain" data-w="w_static_text"></div>'

def title(t, k=''):
    return (f'<div class="title" style="top:60px">{t}</div>'
            + (f'<div class="kicker" style="top:64px">{k}</div>' if k else ''))

# shorthand for hint labels
H = lambda btn, key, rnd=True: G(btn, rnd) + T('hint.' + key)

# ── 1. main ─────────────────────────────────────────────────────────────────
MAIN = (icon('watermark', bleed=True) + wordmark()
  + '<div class="hr" style="top:176px;--c:var(--rule-hot)"></div>'
  + panel(
      row(T('main.item.new'),      **{'data-act':'goto:difficulty'})
    + row(T('main.item.continue'), **{'data-act':'goto:term'})
    + row(T('main.item.saves'),    **{'data-act':'goto:saves'})
    + row(T('main.item.prefs'),    **{'data-act':'goto:prefs'})
    + row(T('main.item.credits'),  **{'data-act':'goto:credits'}),
      340, 'left:40px;top:200px', cap=T('main.cap'))
  + f'<div class="statecol" data-w="w_static_text">{T("main.state.label")}<br>'
    f'<em>WAITING PERIOD</em><br><em>VMU A1 &middot; 12 BLK</em></div>'
  + HR_B + hint(H('A','select'), DPAD + T('hint.move')))

# ── 2. new game — difficulty (the names are Bungie's) ───────────────────────
DIFFICULTY = (title(T('difficulty.title'), T('difficulty.kicker')) + HR_T
  + panel(''.join(row(d, **{'data-act':'goto:term'}) for d in TL('difficulty.names')),
      340, 'left:40px;top:128px', cap=T('difficulty.cap'))
  + HR_B + hint(H('A','begin'), H('B','back'), DPAD + T('hint.move')))

# ── 3. manage saves — four slots ────────────────────────────────────────────
def srow(line):
    if '|' not in line:
        return (f'<div class="srow empty off" data-nav><span class="car"></span>'
                f'<span class="nm">{T("saves.empty")}</span>'
                f'<span class="tm"></span><span class="vm"></span></div>')
    n, t, v = [p.strip() for p in line.split('|')]
    return (f'<div class="srow" data-nav data-act="load"><span class="car"></span>'
            f'<span class="nm">{n}</span><span class="tm">{t}</span>'
            f'<span class="vm">{v}</span></div>')

SAVE_LINES = TL('sample.saves')
FREE0 = 200 - sum(1 for l in SAVE_LINES if '|' in l) * 12
SAVES = (title(T('saves.title'), T('saves.kicker', free=FREE0)) + HR_T
  + panel(''.join(srow(l) for l in SAVE_LINES), 560, 'left:40px;top:128px',
          cap=f'{T("saves.cap")}<span class="sp">{T("saves.cap.right")}</span>', rowh=34)
  + f'<div class="savebanner" data-w="w_static_text" hidden>{T("saves.banner")}</div>'
  + HR_B + hint(H('A','load'), H('X','delete'), H('B','back')))

# ── 4. preferences root ─────────────────────────────────────────────────────
# MENU-TREE 5.2: of six graphics settings only Brightness survives, and it is a
# single row, so it sits on the root rather than in a screen of one.
# Difficulty is not here. It is chosen on New Game and fixed for the run.
# get_difficulty_level() is read at exactly two places — interface.cpp:419 on a
# save load and :1394 on a new game — and goto_level does not re-read it, so a
# row here could only ever have applied to the next new game or load. Decided
# against rather than worked around.
PREFS = (title(T('prefs.title'), T('prefs.kicker')) + HR_T
  + panel(
      ctl(T('prefs.brightness'), 'choice', T('prefs.brightness.note'),
          opts=TL('prefs.brightness.values'), val=3)
    + row(T('prefs.sound'), ARROW,
          **{'data-act':'goto:prefs-sound', 'data-note':T('prefs.sound.note')})
    + row(T('prefs.controls'), ARROW,
          **{'data-act':'goto:prefs-controls', 'data-note':T('prefs.controls.note')}),
      560, 'left:40px;top:128px')
  + EXPLAIN + HR_B
  + hint(DPAD + T('hint.move'), LR + T('hint.adjust'), H('A','open'), H('B','back')))

# ── 5. sound (MENU-TREE 5.4; Channels dropped as moot) ──────────────────────
SOUND = (title(T('sound.title'), T('sound.kicker')) + HR_T
  + panel(
      ctl(T('sound.volume'),  'slider', T('sound.volume.note'), val=6)
    + ctl(T('sound.quality'), 'toggle', T('sound.quality.note'),
          on=T('sound.quality.on'), off=T('sound.quality.off'), val=1)
    + ctl(T('sound.stereo'),  'toggle', T('sound.stereo.note'),  on='ON', off='OFF', val=1)
    + ctl(T('sound.panning'), 'toggle', T('sound.panning.note'), on='ON', off='OFF', val=1)
    + ctl(T('sound.ambient'), 'toggle', T('sound.ambient.note'), on='ON', off='OFF', val=1)
    + ctl(T('sound.more'),    'toggle', T('sound.more.note'),    on='ON', off='OFF', val=1),
      560, 'left:40px;top:128px', rowh=30)
  + EXPLAIN + HR_B
  + hint(DPAD + T('hint.move'), LR + T('hint.adjust'), H('B','back')))

# ── 6. controls (MENU-TREE 5.5; Mouse Control removed — there is no mouse) ──
CONTROLS = (title(T('controls.title'), T('controls.kicker')) + HR_T
  + panel(
      ctl(T('controls.stick'), 'choice', T('controls.stick.note'),
          opts=TL('controls.stick.values'), val=0)
    + ctl(T('controls.turnsens'),   'slider', T('controls.turnsens.note'), val=4)
    + ctl(T('controls.looksens'),   'slider', T('controls.looksens.note'), val=3)
    + ctl(T('controls.invert'),     'toggle', T('controls.invert.note'), on='ON', off='OFF', val=0)
    + ctl(T('controls.run'),        'toggle', T('controls.run.note'),    on='ON', off='OFF', val=1)
    + ctl(T('controls.swim'),       'toggle', T('controls.swim.note'),   on='ON', off='OFF', val=1)
    + ctl(T('controls.autoswitch'), 'toggle', T('controls.autoswitch.note'), on='ON', off='OFF', val=1)
    + row(T('controls.configure'), ARROW,
          **{'data-act':'goto:controller', 'data-note':T('controls.configure.note')}),
      560, 'left:40px;top:128px', rowh=28)
  + EXPLAIN + HR_B
  + hint(DPAD + T('hint.move'), LR + T('hint.adjust'), H('A','open'), H('B','back')))

# ── 7. configure controller (MENU-TREE 6) ───────────────────────────────────
MAIN_DEFAULTS = ['Y','A','X','B',None,None,None,None,'D-RIGHT','R TRIGGER','L TRIGGER',
                 'D-UP','D-DOWN']
ADV_DEFAULTS  = [None,None,None,None,None,'D-LEFT',None]

def bindrow(label, btn):
    v = (f'<span class="vlabel">{btn}</span>' if btn
         else '<span class="vlabel none">&mdash;</span>')
    return (f'<div class="row bind" data-nav data-kind="bind" data-w="w_pad_key" '
            f'data-note="{T("controller.bind.note", name=label.lower())}">'
            f'<span class="car"></span>{label}<span class="val">{v}</span></div>')

def bindpage(name, labels, defaults, split, cap):
    rows = [bindrow(l, d) for l, d in zip(labels, defaults)]
    return (f'<div class="binds" data-page="{name}" data-split="{split}">'
            + panel(''.join(rows[:split]), 268, 'left:40px;top:128px', cap=cap, rowh=26)
            + panel(''.join(rows[split:]), 268, 'left:332px;top:128px',
                    cap='<span class="sp"></span><span class="spent"></span>', rowh=26)
            + '</div>')

CONTROLLER = (title(T('controller.title'), T('controller.kicker')) + HR_T
  + bindpage('main', TL('controller.actions.main'), MAIN_DEFAULTS, 7, T('controller.cap.main'))
  + bindpage('adv',  TL('controller.actions.adv'),  ADV_DEFAULTS,  4, T('controller.cap.adv'))
  + EXPLAIN + HR_B
  + hint(H('A','bind'), G('X') + f'<span class="pagelbl">{T("controller.page.toadv")}</span>',
         H('Y','defaults'), H('B','cancel'), DPAD + T('hint.move'))
  + f'<div class="capture" data-bleed><div class="cbox">'
    f'<div class="ch">{T("controller.capture.title")}</div>'
    f'<div class="cb">{T("controller.capture.body", name="<em class=\'cwhat\'></em>")}</div>'
    f'<div class="cf">{T("controller.capture.foot")}</div></div></div>')

# ── 8. credits ──────────────────────────────────────────────────────────────
def credits_roll():
    out = ''
    for line in TL('credits.roll'):
        if line.startswith('='):
            out += f'<p>{line[1:].strip()}</p>'
        else:
            out += f'<h4>{line}</h4>'
    return out

CREDITS = (title(T('credits.title')) + HR_T
  + f'<div class="credits"><div class="scroll">{credits_roll()}</div></div>'
  + HR_B + hint(H('B','back')))

# ── 9. terminal ─────────────────────────────────────────────────────────────
# Untouched, permanently. Original graphics and text stay as they are.
TERM = ('<div class="term" data-bleed><div class="frame">'
  '<div class="tbar">TERMINAL 2<span class="sp">DURANDAL</span></div>'
  '<div class="body"><div class="txt">'
  "<p>You are aboard the <span class=\"hi\">Rozinante</span>. It is a small ship, and "
  "it was not built for what I am about to ask of it.</p>"
  "<p>The S'pht have opened the lower decks to vacuum. I have sealed what I can. "
  "Go to the <span class=\"hi\">north airlock</span> and I will do the rest.</p>"
  "<p>I need you alive for about four more hours.</p>"
  f'</div><div class="pic">{icon()}</div></div>'
  '<div class="tfoot"><span>PAGE 1 OF 3</span><span class="sp">'
  + G('A') + 'NEXT &nbsp; ' + G('START', False) + 'PAUSE</span></div></div></div>')

# ── 10. pause ───────────────────────────────────────────────────────────────
PAUSE = ('<div class="scrim" data-bleed></div>'
  + f'<div class="title" style="top:96px">{T("pause.title")}</div>'
  + f'<div class="kicker" style="top:100px">{T("sample.level")}</div>'
  + '<div class="hr" style="top:128px;--c:var(--rule-hot)"></div>'
  + panel(
      row(T('pause.item.resume'), **{'data-act':'goto:term'})
    + row(T('pause.item.save'),   **{'data-act':'save'})
    + row(T('pause.item.prefs'),  **{'data-act':'goto:prefs'})
    + row(T('pause.item.quit'),   **{'data-act':'quitmain'}),
      340, 'left:40px;top:160px', cap=T('pause.cap'))
  + f'<div class="statecol" data-w="w_static_text" style="top:164px">'
    f'{T("pause.state.elapsed")}<br><em>{T("sample.elapsed")}</em><br>'
    f'{T("pause.state.lastsave")}<br><em>{T("sample.lastsave")}</em></div>'
  + HR_B + hint(H('A','select'), G('START', False) + T('hint.resume'),
                DPAD + T('hint.move')))

SCREENS = [('main',MAIN),('difficulty',DIFFICULTY),('saves',SAVES),('prefs',PREFS),
           ('prefs-sound',SOUND),('prefs-controls',CONTROLS),('controller',CONTROLLER),
           ('credits',CREDITS),('term',TERM),('pause',PAUSE)]

PADMAP = ''.join(f'<span data-btn="{b}">{l}</span>' for b, l in
  [('up','&#9650;'),('down','&#9660;'),('left','&#9664;'),('right','&#9654;'),
   ('A','A'),('B','B'),('X','X'),('Y','Y'),('start','START')])

HTML = f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<title>Marathon 2 DC &mdash; UI prototype</title>
<link rel="stylesheet" href="app.css">
</head><body>
<div class="wrap">

<div class="bar">
  <b>Overlays</b>
  <label><input type="checkbox" id="t-safe"> safe area</label>
  <label><input type="checkbox" id="t-overscan"> overscan</label>
  <label><input type="checkbox" id="t-crt"> scanlines</label>
  <label><input type="checkbox" id="t-field"> <b style="color:#f0b83c">interlace 480i</b></label>
  <label><input type="checkbox" id="t-map"> widget map</label>
  <label><input type="checkbox" id="t-zoom"> 2&times;</label>
  <span class="sp"></span>
  <span class="padmap">{PADMAP}</span>
</div>

<div class="cols"><div class="stage"><div class="screen">
  <svg width="0" height="0" style="position:absolute" aria-hidden="true"><defs>
    <filter id="m2blur" x="-25%" y="-40%" width="150%" height="180%">
      <feGaussianBlur stdDeviation="3.00"/></filter></defs></svg>
  <div class="plate" data-bleed></div>
  {''.join(f'<section data-screen="{k}" hidden>{v}</section>' for k, v in SCREENS)}
  <div class="modal" data-bleed><div class="box"><div class="mh"></div><div class="mb"></div>
    <div class="mf"></div></div></div>
  <div class="ov ov-safe"></div><div class="ov ov-scan"></div>
  <div class="ov ov-crt"></div><div class="ov ov-field"></div>
</div></div>

<div class="side">
  <h3>Try this</h3>
  <p style="margin:0 0 10px">Preferences &rarr; Controls &rarr; Configure Controller.
     <kbd>C</kbd> flips to the ADVANCED page, <kbd>X</kbd> on a row binds it.</p>
  <h3>Rules the screens obey</h3>
  <dl>
    <dt>edges</dt><dd>40px clear</dd>
    <dt>h-rules</dt><dd>2px + flanks</dd>
    <dt>1px horiz</dt><dd>never</dd>
    <dt>icon min</dt><dd>60px</dd>
    <dt>amber</dt><dd>#FFC000</dd>
    <dt>Start</dt><dd>always backs out</dd>
  </dl>
  <h3>Copy</h3>
  <p style="margin:0">Every word is in <code>strings.md</code>. Edit it, then
     <code>python3 build.py</code>.</p>
</div>

<div class="legend">
  <span><kbd>&#8593;</kbd><kbd>&#8595;</kbd> or <kbd>W</kbd><kbd>S</kbd> &mdash; D-pad</span>
  <span><kbd>&#8592;</kbd><kbd>&#8594;</kbd> &mdash; adjust</span>
  <span><kbd>X</kbd> or <kbd>&#9166;</kbd> &mdash; A</span>
  <span><kbd>Z</kbd> or <kbd>esc</kbd> &mdash; B</span>
  <span><kbd>C</kbd> &mdash; X</span>
  <span><kbd>V</kbd> &mdash; Y</span>
  <span><kbd>tab</kbd> &mdash; Start</span>
</div>

</div>
<script>window.T = {json.dumps(STR, ensure_ascii=False)};</script>
<script src="app.js"></script>
</body></html>'''

open(os.path.join(HERE, 'index.html'), 'w', encoding='utf-8').write(HTML)
print(f'index.html written — {len(SCREENS)} screens, {len(STR)} strings, {len(HTML)} bytes')
