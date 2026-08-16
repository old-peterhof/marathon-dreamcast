/* ─────────────────────────────────────────────────────────────────────────
   Marathon 2 Dreamcast — controller-native UI prototype
   Pad-only navigation. Every screen is driven by the same four operations:
   move, activate, adjust, back — because that is all a pad gives you.
   ───────────────────────────────────────────────────────────────────────── */
'use strict';

/* ── pad ─────────────────────────────────────────────────────────────────
   Dreamcast has: D-pad, analog stick, A B X Y, two analog triggers, Start.  */
const PAD = {
  up:['ArrowUp','w','W'], down:['ArrowDown','s','S'],
  left:['ArrowLeft','a','A'], right:['ArrowRight','d','D'],
  A:['x','X','Enter'], B:['z','Z','Escape','Backspace'],
  X:['c','C'], Y:['v','V'], start:['Tab'],
};
function padOf(key){
  for (const btn in PAD) if (PAD[btn].includes(key)) return btn;
  return null;
}

/* ── screen graph ────────────────────────────────────────────────────────
   `back` is what B does. null means B is inert on that screen.             */
const BACK = { main:null, difficulty:'main', saves:'main', prefs:'main',
               credits:'main', term:'pause', pause:'term' };

const S = { screen:'main', cursor:{}, modal:null, prevScreen:null };

const $  = (s,r=document) => r.querySelector(s);
const $$ = (s,r=document) => [...r.querySelectorAll(s)];

const sectionOf = name => $(`[data-screen="${name}"]`);
const itemsOf   = name => $$('[data-nav]', sectionOf(name)).filter(el => !el.classList.contains('off'));

/* ── render ──────────────────────────────────────────────────────────────*/
function paint(){
  $$('[data-screen]').forEach(el => el.hidden = el.dataset.screen !== S.screen
    && !(S.screen === 'pause' && el.dataset.screen === 'term'));

  const items = S.modal ? $$('[data-nav]', $('.modal .mf')) : itemsOf(S.screen);
  const key   = S.modal ? '@modal' : S.screen;
  const i     = clamp(S.cursor[key] ?? 0, 0, items.length - 1);
  S.cursor[key] = i;

  $$('.row.on, .srow.on').forEach(el => el.classList.remove('on'));
  if (items[i]) items[i].classList.add('on');

  $('.modal').classList.toggle('up', !!S.modal);
  document.body.dataset.screen = S.screen;
}
const clamp = (v,a,b) => b < a ? a : Math.max(a, Math.min(b, v));

/* ── operations ──────────────────────────────────────────────────────────*/
function move(d){
  const key   = S.modal ? '@modal' : S.screen;
  const items = S.modal ? $$('[data-nav]', $('.modal .mf')) : itemsOf(S.screen);
  if (!items.length) return;
  const n = items.length;
  S.cursor[key] = ((S.cursor[key] ?? 0) + d + n) % n;   /* wraps, as a pad menu should */
  paint();
}

/* Left/Right adjusts the focused control in place — no sub-dialog, because a
   pad has nowhere good to put one. */
function adjust(dir){
  const items = itemsOf(S.screen);
  const el = items[S.cursor[S.screen] ?? 0];
  if (!el) return;
  const kind = el.dataset.kind;
  if (kind === 'slider'){
    const max = +el.dataset.max || 8;
    let v = clamp((+el.dataset.val || 0) + dir, 0, max);
    el.dataset.val = v;
    $('.sld', el).innerHTML = Array.from({length:max}, (_,k) =>
      `<i class="${k < v ? 'f' : ''}"></i>`).join('');
  } else if (kind === 'toggle'){
    const v = el.dataset.val === '1' ? '0' : '1';
    el.dataset.val = v;
    $('.vlabel', el).textContent = v === '1' ? el.dataset.labelOn : el.dataset.labelOff;
  } else if (kind === 'choice'){
    const opts = el.dataset.opts.split('|');
    let k = clamp((+el.dataset.val || 0) + dir, 0, opts.length - 1);
    el.dataset.val = k;
    $('.vlabel', el).textContent = opts[k];
  }
}

function activate(){
  if (S.modal){
    const items = $$('[data-nav]', $('.modal .mf'));
    const el = items[S.cursor['@modal'] ?? 0];
    const yes = el && el.dataset.act === 'confirm';
    const job = S.modal;
    S.modal = null;
    if (yes && job.onYes) job.onYes();
    paint();
    return;
  }
  const el = itemsOf(S.screen)[S.cursor[S.screen] ?? 0];
  if (!el) return;
  const act = el.dataset.act || '';
  if (act.startsWith('goto:'))  go(act.slice(5));
  else if (act === 'delete')    confirmDelete(el);
  else if (act === 'quitmain')  confirmQuit();
  else if (act === 'save')      saveBlocked();
  else if (el.dataset.kind)     adjust(+1);      /* A on a control nudges it */
}

function back(){
  if (S.modal){ S.modal = null; paint(); return; }
  const t = BACK[S.screen];
  if (t) go(t);
}

function go(name){
  S.prevScreen = S.screen;
  S.screen = name;
  if (name === 'credits') creditsReset();
  paint();
}

/* ── confirms ────────────────────────────────────────────────────────────*/
function openModal({title, body, note, yes, no, onYes}){
  $('.modal .mh').textContent = title;
  $('.modal .mb').innerHTML = body + (note ? `<small>${note}</small>` : '');
  const f = $('.modal .mf');
  f.innerHTML =
    `<div class="row" data-nav data-act="cancel">${no}</div>` +
    `<div class="row" data-nav data-act="confirm">${yes}</div>`;
  S.cursor['@modal'] = 0;          /* defaults to the safe answer */
  S.modal = { onYes };
  paint();
}

function confirmDelete(el){
  const name = $('.nm', el).textContent.trim();
  openModal({
    title:'DELETE SAVE', body:`Delete &ldquo;${name}&rdquo;?`,
    note:'This frees 12 blocks on VMU A1. It cannot be undone.',
    no:'KEEP', yes:'DELETE',
    onYes(){
      el.classList.add('off','empty');
      $('.nm', el).textContent = 'EMPTY SLOT';
      $('.tm', el).textContent = ''; $('.vm', el).textContent = '';
      S.cursor.saves = 0; paint();
    }});
}

function confirmQuit(){
  openModal({
    title:'QUIT TO MAIN MENU', body:'Leave the current game?',
    note:'Progress since your last save is lost. Saves live on the VMU and survive power-off; recorded films do not.',
    no:'STAY', yes:'QUIT', onYes(){ go('main'); }});
}

function saveBlocked(){
  openModal({
    title:'SAVE GAME', body:'Saving needs a name, and a pad cannot type one.',
    note:'w_text_entry exists but nothing drives it from a controller. Until there is an on-screen keyboard, the choice is an auto-generated name (level plus timestamp) or overwriting a slot you pick from a list.',
    no:'BACK', yes:'USE AUTO NAME',
    onYes(){ go('term'); }});
}

/* ── credits scroll ──────────────────────────────────────────────────────*/
let creditsY = 0, creditsRun = false;
function creditsReset(){ creditsY = 0; creditsRun = true; }
function creditsTick(){
  if (S.screen !== 'credits' || !creditsRun) return;
  const box = $('.credits'), sc = $('.credits .scroll');
  creditsY += 0.35;
  if (creditsY > sc.offsetHeight) creditsY = -box.offsetHeight;
  sc.style.transform = `translateY(${-creditsY}px)`;
}

/* ── input ───────────────────────────────────────────────────────────────*/
addEventListener('keydown', e => {
  const t = e.target;
  if (t && t.tagName === 'INPUT' && !['checkbox','radio','button'].includes(t.type)) return;
  const btn = padOf(e.key);
  if (!btn) return;
  e.preventDefault();
  flash(btn);
  switch (btn){
    case 'up':    move(-1); break;
    case 'down':  move(+1); break;
    case 'left':  if(!S.modal) adjust(-1); else move(-1); break;
    case 'right': if(!S.modal) adjust(+1); else move(+1); break;
    case 'A':     activate(); break;
    case 'B':     back(); break;
    case 'X':     if (S.screen === 'saves' && !S.modal) {
                    const el = itemsOf('saves')[S.cursor.saves ?? 0];
                    if (el && !el.classList.contains('empty')) confirmDelete(el);
                  } break;
    case 'start': if (S.screen === 'term') go('pause');
                  else if (S.screen === 'pause') go('term');
                  break;
  }
});

addEventListener('click', e => {
  const el = e.target.closest('[data-nav]');
  if (!el || el.classList.contains('off')) return;
  const inModal = !!el.closest('.modal');
  if (!!S.modal !== inModal) return;
  const key   = S.modal ? '@modal' : S.screen;
  const items = S.modal ? $$('[data-nav]', $('.modal .mf')) : itemsOf(S.screen);
  const i = items.indexOf(el);
  if (i < 0) return;
  S.cursor[key] = i;
  paint();
  activate();
});

function flash(btn){
  const el = $(`.padmap [data-btn="${btn}"]`);
  if (!el) return;
  el.classList.add('lit');
  setTimeout(() => el.classList.remove('lit'), 110);
}

/* ── debug overlays ──────────────────────────────────────────────────────*/
const page = $('.wrap');
[['safe','showsafe'],['overscan','showoverscan'],['crt','showcrt'],
 ['field','showfield'],['map','showmap'],['zoom','zoom2']].forEach(([id,cls]) => {
  const box = document.getElementById('t-'+id);
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

/* ?screen=prefs deep-links a screen, for review and screenshots */
{ const q = new URLSearchParams(location.search).get('screen');
  if (q && BACK.hasOwnProperty(q)) S.screen = q; }

paint();
