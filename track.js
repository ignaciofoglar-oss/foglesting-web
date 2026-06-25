/*
 * Foglesting — telemetría web por visitante (anónima).
 * Registra qué hace cada visitante en el sitio: página vista, clicks en
 * botones/links, profundidad de scroll, descargas y tiempo en la página.
 * Manda los eventos a /api/track-web (que les agrega país por IP y los guarda
 * en Firestore -> visible en el admin, pestaña "Tráfico web").
 * Anónimo: un id aleatorio en localStorage, sin datos personales.
 */
(function () {
  'use strict';
  if (window.__fgTrackLoaded) return;
  window.__fgTrackLoaded = true;

  var ENDPOINT = '/api/track-web';
  var KEY = 'fglstg_diag_K7m2Qx9pR4tZ8vL1nB6wY3'; // misma key pública que el resto de la telemetría
  var SESSION_GAP_MS = 30 * 60 * 1000;

  // --- id de visitante (persistente) y de sesión (por visita, corta a 30 min) ---
  function uid() {
    return 'v' + Date.now().toString(36) + Math.random().toString(36).slice(2, 8);
  }
  var isNew = false;
  var visitorId;
  try {
    visitorId = localStorage.getItem('fg_vid');
    if (!visitorId) { visitorId = uid(); localStorage.setItem('fg_vid', visitorId); isNew = true; }
  } catch (e) { visitorId = uid(); isNew = true; }

  var sessionId;
  try {
    var last = Number(localStorage.getItem('fg_sid_ts') || 0);
    sessionId = localStorage.getItem('fg_sid') || '';
    if (!sessionId || (Date.now() - last) > SESSION_GAP_MS) {
      sessionId = 's' + Date.now().toString(36) + Math.random().toString(36).slice(2, 6);
      localStorage.setItem('fg_sid', sessionId);
    }
    localStorage.setItem('fg_sid_ts', String(Date.now()));
  } catch (e) { sessionId = 's' + uid(); }

  // --- contexto del dispositivo ---
  var ua = navigator.userAgent || '';
  function detectDevice() {
    if (/Mobi|Android|iPhone|iPod/i.test(ua)) return 'mobile';
    if (/iPad|Tablet/i.test(ua)) return 'tablet';
    return 'desktop';
  }
  function detectBrowser() {
    if (/Edg\//.test(ua)) return 'Edge';
    if (/OPR\//.test(ua)) return 'Opera';
    if (/Chrome\//.test(ua)) return 'Chrome';
    if (/Firefox\//.test(ua)) return 'Firefox';
    if (/Safari\//.test(ua)) return 'Safari';
    return 'Otro';
  }
  function detectOS() {
    if (/Windows/.test(ua)) return 'Windows';
    if (/Android/.test(ua)) return 'Android';
    if (/iPhone|iPad|iPod/.test(ua)) return 'iOS';
    if (/Mac OS X/.test(ua)) return 'macOS';
    if (/Linux/.test(ua)) return 'Linux';
    return 'Otro';
  }
  function lang() {
    try { return localStorage.getItem('foglesting_lang') || (navigator.language || '').slice(0, 2); } catch (e) { return ''; }
  }
  function utm() {
    try {
      var p = new URLSearchParams(location.search);
      var keys = ['utm_source', 'utm_medium', 'utm_campaign'];
      var out = [];
      keys.forEach(function (k) { if (p.get(k)) out.push(k.replace('utm_', '') + '=' + p.get(k)); });
      return out.join(' ');
    } catch (e) { return ''; }
  }

  var base = {
    visitorId: visitorId,
    sessionId: sessionId,
    isNew: isNew,
    device: detectDevice(),
    browser: detectBrowser(),
    os: detectOS(),
    screen: (screen.width || 0) + 'x' + (screen.height || 0),
    ua: ua.slice(0, 300)
  };

  // --- envío (lote + keepalive para que sobreviva al cierre de pestaña) ---
  var queue = [];
  var flushTimer = null;

  function send(events, useBeacon) {
    if (!events.length) return;
    var body = JSON.stringify({ k: KEY, events: events });
    try {
      if (useBeacon && navigator.sendBeacon) {
        navigator.sendBeacon(ENDPOINT, new Blob([body], { type: 'application/json' }));
        return;
      }
    } catch (e) {}
    try {
      fetch(ENDPOINT, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'x-fogl-key': KEY },
        body: body,
        keepalive: true
      }).catch(function () {});
    } catch (e) {}
  }

  function flush(useBeacon) {
    if (flushTimer) { clearTimeout(flushTimer); flushTimer = null; }
    if (!queue.length) return;
    var batch = queue.splice(0, queue.length);
    send(batch, useBeacon);
  }

  function track(event, detail, extra) {
    var ev = {
      event: event,
      detail: detail ? String(detail).slice(0, 300) : '',
      path: location.pathname + location.search,
      title: (document.title || '').slice(0, 160),
      referrer: (document.referrer || '').slice(0, 300),
      lang: lang(),
      utm: utm(),
      tsClientISO: new Date().toISOString()
    };
    if (extra) for (var k in extra) ev[k] = extra[k];
    for (var b in base) ev[b] = base[b];
    queue.push(ev);
    if (!flushTimer) flushTimer = setTimeout(function () { flush(false); }, 1500);
    if (queue.length >= 15) flush(false);
  }
  window.fgTrack = track; // por si querés disparar eventos a mano

  // --- page view inicial ---
  track('page_view');

  // --- clicks en elementos relevantes (delegado) ---
  document.addEventListener('click', function (e) {
    var el = e.target;
    for (var i = 0; i < 4 && el && el !== document.body; i++) {
      var tag = (el.tagName || '').toLowerCase();
      if (tag === 'a' || tag === 'button' || el.getAttribute('role') === 'button' ||
          (el.className && /\bbtn\b/.test(el.className))) {
        var label = (el.innerText || el.textContent || el.getAttribute('aria-label') || '').trim().replace(/\s+/g, ' ').slice(0, 60);
        var href = el.getAttribute && el.getAttribute('href');
        var isDownload = href && /\.(exe|zip|dxf)(\?|$)/i.test(href);
        track(isDownload ? 'download' : 'click', (label || tag) + (href ? ' → ' + href : ''));
        return;
      }
      el = el.parentElement;
    }
  }, true);

  // --- profundidad de scroll (hitos 25/50/75/100) ---
  var marks = { 25: false, 50: false, 75: false, 100: false };
  function onScroll() {
    var h = document.documentElement;
    var max = (h.scrollHeight - h.clientHeight);
    if (max <= 0) return;
    var pct = Math.round((h.scrollTop || window.pageYOffset || 0) / max * 100);
    [25, 50, 75, 100].forEach(function (m) {
      if (pct >= m && !marks[m]) { marks[m] = true; track('scroll', m + '%'); }
    });
  }
  window.addEventListener('scroll', onScroll, { passive: true });

  // --- tiempo en la página + cierre ---
  var start = Date.now();
  function leave() {
    track('page_leave', '', { ms: Date.now() - start });
    flush(true);
  }
  document.addEventListener('visibilitychange', function () {
    if (document.visibilityState === 'hidden') leave();
  });
  window.addEventListener('pagehide', leave);
})();
