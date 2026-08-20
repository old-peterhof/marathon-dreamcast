/* Injected by check.py. Audits the live DOM of whichever screen is showing
   against the rules in UI-HANDOFF.md. Reports, never fixes. */
window.__audit = () => {
  const r2 = n => Math.round(n * 10) / 10;
  const SAFE = { l:40, t:40, r:600, b:440 };
  const TRANSPARENT = 'rgba(0, 0, 0, 0)';

  const desc = el => {
    let s = el.tagName.toLowerCase();
    if (el.className && typeof el.className === 'string')
      s += '.' + el.className.trim().split(/\s+/).slice(0, 3).join('.');
    const txt = [...el.childNodes].filter(n => n.nodeType === 3)
      .map(n => n.textContent.trim()).join(' ').slice(0, 28);
    return txt ? `${s} "${txt}"` : s;
  };

  /* Does this element put ink on the screen itself? Wrappers that only
     position their children are not violations when they overhang. */
  const paints = (el, cs) => {
    if (el.tagName === 'path' || el.tagName === 'svg') return true;
    if (cs.backgroundColor !== TRANSPARENT) return true;
    if (cs.backgroundImage !== 'none') return true;
    if (parseFloat(cs.borderTopWidth) || parseFloat(cs.borderLeftWidth)) return true;
    if (cs.boxShadow !== 'none') return true;
    return [...el.childNodes].some(n => n.nodeType === 3 && n.textContent.trim());
  };

  const skip = el =>
    el.closest('[hidden]') || el.closest('.ov') || el.closest('defs') ||
    el.hasAttribute('data-bleed') ||        /* this element only */
    el.closest('[data-bleed-all]') ||       /* and everything under it */
    el.closest('.modal:not(.up)') ||
    el.closest('.capture:not(.up)');

  /* What the viewer actually sees: the box intersected with every ancestor
     that clips. A masked credits roll is not a safe-area breach. */
  const clipped = (el, b) => {
    let r = { left:b.left, top:b.top, right:b.right, bottom:b.bottom };
    for (let p = el.parentElement; p; p = p.parentElement) {
      const pcs = getComputedStyle(p);
      const clips = ['hidden','scroll','auto','clip'];
      if (!clips.includes(pcs.overflowX) && !clips.includes(pcs.overflowY)
          && pcs.maskImage === 'none' && pcs.webkitMaskImage === 'none') continue;
      const pb = p.getBoundingClientRect();
      r = { left:Math.max(r.left, pb.left), top:Math.max(r.top, pb.top),
            right:Math.min(r.right, pb.right), bottom:Math.min(r.bottom, pb.bottom) };
      if (r.right <= r.left || r.bottom <= r.top) return null;
    }
    return r;
  };

  const screen = document.querySelector('.screen');
  const sb = screen.getBoundingClientRect();
  const out = { screen: S.screen, page: S.page, safe: [], thin: [], icon: [], hit: [], over: [] };
  const text = [];   /* text-bearing boxes, for the collision pass */

  for (const el of screen.querySelectorAll('*')) {
    if (skip(el)) continue;
    const cs = getComputedStyle(el);
    if (cs.display === 'none' || cs.visibility === 'hidden' || +cs.opacity === 0) continue;
    let b = el.getBoundingClientRect();
    if (!b.width || !b.height) continue;
    if (!paints(el, cs)) continue;
    b = clipped(el, b);
    if (!b || b.right - b.left < 0.5 || b.bottom - b.top < 0.5) continue;

    const x0 = b.left - sb.left, y0 = b.top - sb.top;
    const x1 = b.right - sb.left, y1 = b.bottom - sb.top;
    if (x0 < SAFE.l - 0.5 || y0 < SAFE.t - 0.5 || x1 > SAFE.r + 0.5 || y1 > SAFE.b + 0.5)
      out.safe.push({ el: desc(el), box: [r2(x0), r2(y0), r2(x1), r2(y1)] });

    /* 480i: an isolated one-scanline horizontal buzzes at 30Hz */
    if (Math.round(b.height) === 1 && b.width > 20)
      out.thin.push({ el: desc(el), w: r2(b.width), y: r2(y0) });

    const own = [...el.childNodes].filter(n => n.nodeType === 3)
      .map(n => n.textContent.trim()).join(' ');
    if (own) {
      text.push({ el, box: { x0, y0, x1, y1 }, d: desc(el) });
      /* Two hints sharing one inline box overlapped each other here; a pairwise
         pass cannot see it, because there is only one element involved. */
      if (el.scrollWidth > el.clientWidth + 1 && cs.overflow !== 'hidden')
        out.over.push({ el: desc(el), need: el.scrollWidth, has: el.clientWidth });
    }
  }

  /* Two labels sharing pixels. Titles and right-aligned kickers grew into each
     other twice before this pass existed. Text nodes only, and never a pair
     related by containment or living on different screens. */
  const sect = el => { const s = el.closest('section[data-screen]');
                       return s ? s.dataset.screen : '*'; };
  /* An overlay covering the rows behind it is doing its job, not colliding.
     Only compare labels that share a layer. */
  const layer = el => el.closest('.capture') ? 'capture'
                    : el.closest('.modal')   ? 'modal' : 'base';
  for (let i = 0; i < text.length; i++)
    for (let k = i + 1; k < text.length; k++) {
      const a = text[i], c = text[k];
      if (a.el.contains(c.el) || c.el.contains(a.el)) continue;
      if (sect(a.el) !== sect(c.el)) continue;
      if (layer(a.el) !== layer(c.el)) continue;
      const ox = Math.min(a.box.x1, c.box.x1) - Math.max(a.box.x0, c.box.x0);
      const oy = Math.min(a.box.y1, c.box.y1) - Math.max(a.box.y0, c.box.y0);
      if (ox > 1 && oy > 1)
        out.hit.push({ a: a.d, b: c.d, overlap: [r2(ox), r2(oy)] });
    }

  /* The Marathon symbol is negative space; its ring gap is 5.03% of width,
     so under 60px a CRT closes it into a lozenge. */
  for (const svg of screen.querySelectorAll('svg')) {
    if (skip(svg)) continue;
    if (!(svg.getAttribute('viewBox') || '').startsWith('0 0 750')) continue;
    const w = svg.getBoundingClientRect().width;
    if (w && w < 60) out.icon.push({ w: r2(w) });
  }
  return out;
};
