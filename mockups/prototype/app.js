/* ─────────────────────────────────────────────────────────────────────────
   Marathon 2 Dreamcast — controller-native UI prototype
   Pad-only navigation. Every screen is driven by the same four operations:
   move, activate, adjust, back — because that is all a pad gives you.
   ───────────────────────────────────────────────────────────────────────── */
'use strict';

/* ── pad ─────────────────────────────────────────────────────────────────
   MENU-TREE §2: ten bindable inputs, plus Start, which is reserved and
   always means "back out".                                                 */
const PAD = {
  up:['ArrowUp','w','W'], down:['ArrowDown','s','S'],
  left:['ArrowLeft','a','A'], right:['ArrowRight','d','D'],
  A:['x','X','Enter'], B:['z','Z','Escape','Backspace'],
  X:['c','C'], Y:['v','V'], L:['['], R:[']'], start:['Tab'],
};
const BTN_NAME = { A:'A', B:'B', X:'X', Y:'Y', up:'D-UP', down:'D-DOWN',
                   left:'D-LEFT', right:'D-RIGHT', L:'L TRIGGER', R:'R TRIGGER' };
const padOf = key => Object.keys(PAD).find(b => PAD[b].includes(key)) || null;

/* ── copy ────────────────────────────────────────────────────────────────
   window.T is written into the page by build.py from strings.md. Editing the
   markdown and rebuilding is the whole workflow; nothing here holds copy. */
function t(key, vars){
  let s = (window.T || {})[key];
  if (s === undefined) { console.warn('strings.md: no key', key); return key; }
  for (const k in (vars || {})) s = s.replaceAll('{' + k + '}', vars[k]);
  return s;
}

/* ── screen graph ───────────────────────────────────────────────────────── */
const BACK = {
  main:null, difficulty:'main', saves:'main', prefs:'main',
  'prefs-sound':'prefs', 'prefs-controls':'prefs', controller:'prefs-controls',
  credits:'main', term:'pause', pause:'term',
};
let prefsFrom = 'main';       /* Preferences is reachable from two places */
let savesFrom = 'main';

const S = { screen:'main', cursor:{}, modal:null, capture:null,
            page:'main', overwrite:false };

const $  = (s,r=document) => r.querySelector(s);
const $$ = (s,r=document) => [...r.querySelectorAll(s)];
const clamp = (v,a,b) => b < a ? a : Math.max(a, Math.min(b, v));

const sectionOf = name => $(`section[data-screen="${name}"]`);
const itemsOf = name => $$('[data-nav]', sectionOf(name))
  .filter(el => !el.classList.contains('off') && !el.closest('[hidden]'));

const navItems = () => S.modal ? $$('[data-nav]', $('.modal .mf')) : itemsOf(S.screen);
const navKey   = () => S.modal ? '@modal' : S.screen;
const focused  = () => navItems()[S.cursor[navKey()] ?? 0];

/* ── render ─────────────────────────────────────────────────────────────── */
function paint(){
  $$('section[data-screen]').forEach(el => el.hidden = el.dataset.screen !== S.screen
    && !(S.screen === 'pause' && el.dataset.screen === 'term'));
  /* the controller screen holds two pages; only one is ever live */
  $$('.binds').forEach(el => el.hidden = el.dataset.page !== S.page);

  const items = navItems(), key = navKey();
  const i = clamp(S.cursor[key] ?? 0, 0, items.length - 1);
  S.cursor[key] = i;

  $$('.row.on, .srow.on').forEach(el => el.classList.remove('on'));
  if (items[i]) items[i].classList.add('on');

  /* one line of plain English about whatever is focused */
  const ex = $('.explain', sectionOf(S.screen) || document);
  if (ex) ex.textContent = (items[i] && items[i].dataset.note) || '';

  const lbl = $('.pagelbl');
  if (lbl) lbl.textContent = t(S.page === 'main' ? 'controller.page.toadv'
                                                  : 'controller.page.tomain');

  paintSaves();
  $('.modal').classList.toggle('up', !!S.modal);
  $('.capture').classList.toggle('up', !!S.capture);
  document.body.dataset.screen = S.screen;
  countButtons();
}

/* ── operations ─────────────────────────────────────────────────────────── */
function move(d){
  const items = navItems(), n = items.length;
  if (!n) return;
  S.cursor[navKey()] = ((S.cursor[navKey()] ?? 0) + d + n) % n;
  paint();
}

/* Left/Right adjusts the focused control in place — no sub-dialog, because a
   pad has nowhere good to put one. On the binding screen there is nothing to
   adjust, so the same axis moves between the two columns instead. */
function adjust(dir){
  if (S.screen === 'controller') return columnJump(dir);
  const el = focused();
  if (!el) return;
  const kind = el.dataset.kind;
  if (kind === 'slider'){
    const max = +el.dataset.max || 8;
    const v = clamp((+el.dataset.val || 0) + dir, 0, max);
    el.dataset.val = v;
    $('.sld', el).innerHTML = Array.from({length:max},
      (_,k) => `<i class="${k < v ? 'f' : ''}"></i>`).join('');
  } else if (kind === 'toggle'){
    const v = el.dataset.val === '1' ? '0' : '1';
    el.dataset.val = v;
    $('.vlabel', el).textContent = v === '1' ? el.dataset.labelOn : el.dataset.labelOff;
  } else if (kind === 'choice'){
    const opts = el.dataset.opts.split('|');
    const k = clamp((+el.dataset.val || 0) + dir, 0, opts.length - 1);
    el.dataset.val = k;
    $('.vlabel', el).textContent = opts[k];
  }
}

function columnJump(dir){
  const page = $(`.binds[data-page="${S.page}"]`);
  if (!page) return;
  const split = +page.dataset.split, items = navItems();
  let i = S.cursor.controller ?? 0;
  if (dir > 0 && i < split)       i = clamp(split + i, split, items.length - 1);
  else if (dir < 0 && i >= split) i = clamp(i - split, 0, split - 1);
  S.cursor.controller = i;
  paint();
}

function activate(){
  if (S.modal){
    const el = navItems()[S.cursor['@modal'] ?? 0];
    const yes = el && el.dataset.act === 'confirm';
    const job = S.modal;
    S.modal = null;
    if (yes && job.onYes) job.onYes();
    else if (!yes && job.onNo) job.onNo();
    paint();
    return;
  }
  const el = focused();
  if (!el) return;
  const act = el.dataset.act || '';
  if (act.startsWith('goto:'))   go(act.slice(5));
  else if (act === 'delete')     confirmDelete(el);
  else if (act === 'quitmain')   confirmQuit();
  else if (act === 'save')       saveGame();
  else if (act === 'load')       S.overwrite ? confirmOverwrite(el) : null;
  else if (el.dataset.kind === 'bind') beginCapture(el);
  else if (el.dataset.kind)      adjust(+1);   /* A on a control nudges it */
}

function back(){
  if (S.capture){ S.capture = null; paint(); return; }
  if (S.modal){ S.modal = null; paint(); return; }
  if (S.screen === 'saves' && S.overwrite){ S.overwrite = false; go(savesFrom, true); return; }
  const t = S.screen === 'prefs' ? prefsFrom : BACK[S.screen];
  if (t) go(t, true);
}

function go(name, isBack){
  if (!isBack){
    if (name === 'prefs') prefsFrom = S.screen;
    if (name === 'saves') savesFrom = S.screen;
    if (name === 'controller') S.page = 'main';
  }
  if (name !== 'saves') S.overwrite = false;
  S.screen = name;
  if (name === 'credits') creditsReset();
  paint();
}

/* ── saving ──────────────────────────────────────────────────────────────
   No keyboard, so the name is generated from the level. With four slots the
   only real decision left is which one to spend. */
const CURRENT_LEVEL = t('sample.level');
const ELAPSED = t('sample.elapsed');

const saveRows  = () => $$('section[data-screen="saves"] .srow');
const freeSlot  = () => saveRows().find(r => r.classList.contains('empty'));

function writeSlot(el, name, time){
  el.classList.remove('empty','off');
  el.dataset.act = 'load';
  $('.nm', el).textContent = name;
  $('.tm', el).textContent = time;
  $('.vm', el).textContent = 'A1 12 BLK';
}

function saveGame(){
  const slot = freeSlot();
  if (!slot){
    /* Card full: the choice becomes which save to give up. */
    S.overwrite = true;
    S.cursor.saves = 0;
    go('saves');
    return;
  }
  writeSlot(slot, CURRENT_LEVEL, ELAPSED);
  openModal({
    title: t('save.title'), body: `&ldquo;${CURRENT_LEVEL}&rdquo;`,
    note: t('save.note', { free: saveRows().filter(r => r.classList.contains('empty')).length }),
    no: null, yes: t('save.yes'), onYes(){ go('term', true); }});
}

function confirmOverwrite(el){
  const old = $('.nm', el).textContent.trim();
  openModal({
    title: t('overwrite.title'), body: t('overwrite.body', { name: old }),
    note: t('overwrite.note', { level: CURRENT_LEVEL, time: ELAPSED }),
    no: t('overwrite.no'), yes: t('overwrite.yes'),
    onYes(){ writeSlot(el, CURRENT_LEVEL, ELAPSED); S.overwrite = false; go('term', true); }});
}

function paintSaves(){
  const banner = $('.savebanner');
  if (banner) banner.hidden = !S.overwrite;
  const free = saveRows().filter(r => r.classList.contains('empty')).length;
  const k = $('section[data-screen="saves"] .kicker');
  if (k) k.textContent = t('saves.kicker', { free: 200 - (4 - free) * 12 });
}

/* ── binding capture (MENU-TREE 6.3) ────────────────────────────────────────
   While capturing, nothing else on the pad responds and Start is the only way
   out — the same contract w_pad_key gives in the engine. */
function beginCapture(el){
  S.capture = el;
  $('.capture .cwhat').textContent = el.childNodes[1].textContent.trim().toLowerCase();
  paint();
}

function applyBinding(btn){
  const name = BTN_NAME[btn];
  if (!name) return;
  /* Ten buttons, twenty actions: taking one takes it from whoever had it. */
  $$('.row.bind').forEach(r => {
    const v = $('.vlabel', r);
    if (r !== S.capture && v.textContent === name){
      v.textContent = '—'; v.classList.add('none');
    }
  });
  const v = $('.vlabel', S.capture);
  v.textContent = name; v.classList.remove('none');
  S.capture = null;
  paint();
}

let DEFAULT_BINDS = null;
const snapshotBinds = () => { DEFAULT_BINDS = $$('.row.bind').map(r => $('.vlabel', r).textContent); };
function restoreDefaults(){
  if (!DEFAULT_BINDS) return;
  $$('.row.bind').forEach((r,i) => {
    const v = $('.vlabel', r);
    v.textContent = DEFAULT_BINDS[i];
    v.classList.toggle('none', DEFAULT_BINDS[i] === '—');
  });
  paint();
}

function countButtons(){
  const used = new Set($$('.row.bind').map(r => $('.vlabel', r).textContent)
    .filter(t => t && t !== '—'));
  $$('.spent').forEach(el => {
    el.textContent = t('controller.spent', { n: used.size });
  });
}

/* ── confirms ───────────────────────────────────────────────────────────── */
function openModal({title, body, note, yes, no, onYes, onNo}){
  $('.modal .mh').textContent = title;
  $('.modal .mb').innerHTML = body + (note ? `<small>${note}</small>` : '');
  $('.modal .mf').innerHTML =
    (no ? `<div class="row" data-nav data-act="cancel">${no}</div>` : '') +
    `<div class="row" data-nav data-act="confirm">${yes}</div>`;
  S.cursor['@modal'] = 0;          /* defaults to the safe answer */
  S.modal = { onYes, onNo };
  paint();
}

function confirmDelete(el){
  const name = $('.nm', el).textContent.trim();
  openModal({
    title: t('delete.title'), body: t('delete.body', { name }),
    note: t('delete.note'),
    no: t('delete.no'), yes: t('delete.yes'),
    onYes(){
      el.classList.add('off','empty');
      delete el.dataset.act;
      $('.nm', el).textContent = t('saves.empty');
      $('.tm', el).textContent = ''; $('.vm', el).textContent = '';
      S.cursor.saves = 0; paint();
    }});
}

function confirmQuit(){
  openModal({
    title: t('quit.title'), body: t('quit.body'), note: t('quit.note'),
    no: t('quit.no'), yes: t('quit.yes'), onYes(){ go('main', true); }});
}

/* ── credits scroll ─────────────────────────────────────────────────────── */
let creditsY = 0, creditsRun = false;
function creditsReset(){ creditsY = 0; creditsRun = true; }
function creditsTick(){
  if (S.screen !== 'credits' || !creditsRun) return;
  const box = $('.credits'), sc = $('.credits .scroll');
  if (!box || !sc) return;
  creditsY += 0.35;
  if (creditsY > sc.offsetHeight) creditsY = -box.offsetHeight;
  sc.style.transform = `translateY(${-creditsY}px)`;
}

/* ── held-direction repeat ──────────────────────────────────────────────────
   A pad has no operating system behind it deciding when a held direction
   repeats, so the interface has to. Until now this prototype inherited the
   host's keyboard repeat, which is whatever the user set in System Settings and
   is nothing at all when key repeat is off.

   Only directions repeat. A held A must never fire twice, and a held Start must
   never back out twice.

   Numbers to carry into dc_input.c. A stick wants a longer lead-in than a D-pad
   because it is easy to over-push and hard to nudge exactly once; the D-pad
   gives a clean discrete press, so it can start sooner. */
const REPEAT = {
  dpad:  { delay: 400, rate: 120 },
  stick: { delay: 500, rate: 150 },
};
const REPEATS = new Set(['up','down','left','right']);
let heldBtn = null, heldTimer = null;

function stopRepeat(){
  clearTimeout(heldTimer);
  heldTimer = null;
  heldBtn = null;
}

function startRepeat(btn, fire){
  stopRepeat();
  heldBtn = btn;
  fire();
  const { delay, rate } = REPEAT.dpad;
  const tick = () => {
    if (heldBtn !== btn) return;
    fire();
    heldTimer = setTimeout(tick, rate);
  };
  heldTimer = setTimeout(tick, delay);
}

/* ── input ──────────────────────────────────────────────────────────────── */
addEventListener('keyup', e => {
  if (padOf(e.key) === heldBtn) stopRepeat();
});
addEventListener('blur', stopRepeat);

addEventListener('keydown', e => {
  const t = e.target;
  if (t && t.tagName === 'INPUT' && !['checkbox','radio','button'].includes(t.type)) return;
  const btn = padOf(e.key);
  if (!btn) return;
  e.preventDefault();
  if (e.repeat) return;          /* the host's repeat; we run our own */
  flash(btn);

  /* Capture swallows everything, and nothing repeats inside it — a held button
     must name one binding, not a stream of them. Start is the way out. */
  if (S.capture){
    stopRepeat();
    if (btn === 'start') back(); else applyBinding(btn);
    return;
  }

  if (REPEATS.has(btn)){
    startRepeat(btn, () => {
      if (btn === 'up')    return move(-1);
      if (btn === 'down')  return move(+1);
      if (btn === 'left')  return S.modal ? move(-1) : adjust(-1);
      if (btn === 'right') return S.modal ? move(+1) : adjust(+1);
    });
    return;
  }
  stopRepeat();

  switch (btn){
    case 'A':     activate(); break;
    case 'B':     back(); break;
    case 'X':
      if (S.modal) break;
      if (S.screen === 'saves' && !S.overwrite){
        const el = focused();
        if (el && !el.classList.contains('empty')) confirmDelete(el);
      } else if (S.screen === 'controller'){
        S.page = S.page === 'main' ? 'adv' : 'main';
        S.cursor.controller = 0; paint();
      }
      break;
    case 'Y':
      if (!S.modal && S.screen === 'controller') restoreDefaults();
      break;
    /* MENU-TREE §2: Start is reserved and always backs out. In game that
       means opening the pause menu; everywhere else it means cancel. */
    case 'start':
      if (S.screen === 'term') go('pause');
      else if (S.screen === 'pause') go('term', true);
      else back();
      break;
  }
});

/* Mouse: click a row to select and activate it, so the prototype can be
   reviewed without learning the key map first. */
addEventListener('click', e => {
  const el = e.target.closest('[data-nav]');
  if (!el || el.classList.contains('off') || S.capture) return;
  if (!!S.modal !== !!el.closest('.modal')) return;
  const i = navItems().indexOf(el);
  if (i < 0) return;
  S.cursor[navKey()] = i;
  paint();
  activate();
});

function flash(btn){
  const el = $(`.padmap [data-btn="${btn}"]`);
  if (!el) return;
  el.classList.add('lit');
  setTimeout(() => el.classList.remove('lit'), 110);
}

/* ── debug overlays ─────────────────────────────────────────────────────── */
const page = $('.wrap');
[['safe','showsafe'],['overscan','showoverscan'],['crt','showcrt'],
 ['field','showfield'],['map','showmap'],['zoom','zoom2']].forEach(([id,cls]) => {
  const box = document.getElementById('t-' + id);
  if (box) box.addEventListener('change', () => page.classList.toggle(cls, box.checked));
});

/* Interlace simulation: flip which scanline parity is blanked, once per frame.
   At 60Hz a 1px horizontal feature is then shown 30 times a second and buzzes;
   a 2px one lands on both fields and holds still. */
let parity = false;
function frame(){
  if (page.classList.contains('showfield')){
    parity = !parity;
    page.classList.toggle('field1', parity);
  }
  creditsTick();
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

/* ?screen=prefs-controls deep-links a screen, for review and screenshots */
{ const q = new URLSearchParams(location.search).get('screen');
  if (q && BACK.hasOwnProperty(q)) S.screen = q; }

snapshotBinds();
paint();
