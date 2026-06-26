import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import { 
    getAuth, 
    signInWithEmailAndPassword, 
    onAuthStateChanged, 
    signOut,
    sendPasswordResetEmail
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-auth.js";
import { 
    getFirestore, 
    collection, 
    getDocs, 
    doc, 
    getDoc, 
    setDoc, 
    deleteDoc,
    addDoc, 
    updateDoc, 
    query,
    orderBy,
    limit,
    where,
    startAfter,
    getCountFromServer,
    getAggregateFromServer,
    sum,
    average,
    writeBatch,
    serverTimestamp
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-firestore.js";
import {
    getStorage,
    ref as storageRef,
    uploadBytesResumable,
    listAll,
    getDownloadURL,
    getMetadata,
    deleteObject
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-storage.js";

// Firebase Configuration from User
const firebaseConfig = {
  apiKey: "AIzaSyC-hdDg1NAcTli4Pck0IwYomPpMF-tLO9s",
  authDomain: "foglesting.firebaseapp.com",
  projectId: "foglesting",
  storageBucket: "foglesting.firebasestorage.app",
  messagingSenderId: "340614706642",
  appId: "1:340614706642:web:0886f5fc03d6fb5b5e440b",
  measurementId: "G-BTQBKQV6ML"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const auth = getAuth(app);
const db = getFirestore(app);
const storage = getStorage(app);

// DOM Elements
const loginScreen = document.getElementById('login-screen');
const dashboardScreen = document.getElementById('dashboard-screen');
const loginForm = document.getElementById('login-form');
const loginError = document.getElementById('login-error');
const logoutBtn = document.getElementById('logout-btn');
const deleteOldMessagesBtn = document.getElementById('delete-old-messages-btn');

const navItems = document.querySelectorAll('.nav-item');
const views = document.querySelectorAll('.view');
let currentAdminUid = null;
let currentAdminEmail = null;
const emergencyAdminEmails = new Set([
    'ignacio.foglar@gmail.com',
    'ignacio_ggirard@hotmail.com',
    'francisco.foglar@gmail.com'
]);

const adminTranslations = {
    es: {
        'messages.nav': 'Mensajes',
        'messages.title': 'Mensajes de Contacto',
        'messages.loading': 'Cargando mensajes...',
        'messages.empty': 'No hay mensajes todavía.',
        'messages.unknownDate': 'Fecha desconocida',
        'messages.noName': 'Sin nombre',
        'messages.noEmail': 'sin email',
        'messages.delete': 'Borrar',
        'messages.deleting': 'Borrando...',
        'messages.deleteConfirm': '¿Borrar este mensaje?',
        'messages.deleteError': 'No se pudo borrar el mensaje.',
        'messages.loadError': 'Error al cargar mensajes (requiere crear índices o reglas).',
        'messages.deleteOld': 'Borrar mensajes antiguos',
        'messages.deleteOldPrompt': '¿Borrar mensajes con más de cuántos días?',
        'messages.deleteOldInvalid': 'Ingresá una cantidad de días válida. Ejemplo: 30',
        'messages.deleteOldConfirm': 'Esto va a borrar todos los mensajes con más de {days} días. ¿Continuar?',
        'messages.cleaning': 'Limpiando...',
        'messages.deleteOldDone': 'Listo. Se borraron {count} mensaje(s) antiguo(s).',
        'messages.deleteOldError': 'No se pudieron borrar los mensajes antiguos.'
    },
    en: {
        'messages.nav': 'Messages',
        'messages.title': 'Contact Messages',
        'messages.loading': 'Loading messages...',
        'messages.empty': 'No messages yet.',
        'messages.unknownDate': 'Unknown date',
        'messages.noName': 'No name',
        'messages.noEmail': 'no email',
        'messages.delete': 'Delete',
        'messages.deleting': 'Deleting...',
        'messages.deleteConfirm': 'Delete this message?',
        'messages.deleteError': 'Could not delete the message.',
        'messages.loadError': 'Error loading messages (indexes or rules may be required).',
        'messages.deleteOld': 'Delete old messages',
        'messages.deleteOldPrompt': 'Delete messages older than how many days?',
        'messages.deleteOldInvalid': 'Enter a valid number of days. Example: 30',
        'messages.deleteOldConfirm': 'This will delete all messages older than {days} days. Continue?',
        'messages.cleaning': 'Cleaning...',
        'messages.deleteOldDone': 'Done. Deleted {count} old message(s).',
        'messages.deleteOldError': 'Could not delete old messages.'
    }
};

function getAdminLang() {
    return localStorage.getItem('foglesting_lang') === 'en' ? 'en' : 'es';
}

function adminT(key, vars = {}) {
    const pack = adminTranslations[getAdminLang()] || adminTranslations.es;
    let text = pack[key] || adminTranslations.es[key] || key;
    Object.entries(vars).forEach(([name, value]) => {
        text = text.replaceAll(`{${name}}`, String(value));
    });
    return text;
}

function applyAdminTranslations() {
    document.documentElement.lang = getAdminLang();
    document.querySelectorAll('[data-admin-i18n]').forEach((el) => {
        el.textContent = adminT(el.dataset.adminI18n);
    });
}

applyAdminTranslations();
window.addEventListener('storage', (event) => {
    if (event.key === 'foglesting_lang') {
        applyAdminTranslations();
        if (document.getElementById('view-messages')?.classList.contains('active')) loadMessages();
    }
});

async function isAuthorizedAdmin(user) {
    if (!user) return false;
    const email = String(user.email || '').trim().toLowerCase();
    if (emergencyAdminEmails.has(email)) return true;

    // Camino principal: verificar rol desde el servidor con Firebase Admin.
    // Evita que el acceso al panel dependa de reglas Firestore del cliente.
    try {
        const idToken = await user.getIdToken(true);
        const response = await fetch('/api/web-telemetry?adminCheck=1', {
            method: 'GET',
            headers: { Authorization: `Bearer ${idToken}` },
            cache: 'no-store'
        });
        const payload = await response.json().catch(() => ({}));
        if (response.ok && payload.ok === true) return true;
        if (response.status === 403) return false;
        console.warn('admin-check fallback:', payload.error || response.statusText);
    } catch (error) {
        console.warn('admin-check unavailable, using Firestore fallback:', error);
    }

    try {
        const userRef = doc(db, 'users', user.uid);
        const userSnap = await getDoc(userRef);
        const userData = userSnap.exists() ? userSnap.data() : {};
        if (userData.role === 'admin' || userData.isAdmin === true) return true;

        // Bootstrap de transicion: si todavia no hay ningun administrador marcado,
        // dejamos entrar para que Ignacio pueda asignar el primer rol admin desde el panel.
        const usersSnap = await getDocs(collection(db, 'users'));
        let hasAnyAdmin = false;
        usersSnap.forEach((docSnap) => {
            const data = docSnap.data();
            if (data.role === 'admin' || data.isAdmin === true) hasAnyAdmin = true;
        });
        return !hasAnyAdmin;
    } catch (error) {
        console.warn('Firestore admin fallback failed:', error);
        return emergencyAdminEmails.has(email);
    }
}

async function adminApi(path, options = {}) {
    const user = auth.currentUser;
    if (!user) throw new Error('Sesion admin no disponible.');
    const idToken = await user.getIdToken(true);
    const headers = {
        ...(options.headers || {}),
        Authorization: `Bearer ${idToken}`,
    };
    if (options.body && !headers['Content-Type']) headers['Content-Type'] = 'application/json';
    const response = await fetch(path, {
        ...options,
        headers,
        cache: 'no-store',
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(payload.error || response.statusText || `HTTP ${response.status}`);
    return payload;
}

// 1. Authentication State Observer
onAuthStateChanged(auth, async (user) => {
    if (user) {
        try {
            const allowed = await isAuthorizedAdmin(user);
            if (!allowed) {
                loginError.textContent = 'Tu cuenta no tiene rol de administrador.';
                await signOut(auth);
                return;
            }

            currentAdminUid = user.uid;
            currentAdminEmail = user.email || '';
            loginScreen.classList.remove('active');
            dashboardScreen.classList.add('active');
            loadDashboardData();
        } catch (error) {
            console.error('Error checking admin role:', error);
            loginError.textContent = 'No se pudo verificar el rol administrador.';
            await signOut(auth);
        }
    } else {
        // User is logged out
        currentAdminUid = null;
        dashboardScreen.classList.remove('active');
        loginScreen.classList.add('active');
    }
});

// 2. Login Handler
loginForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    loginError.textContent = '';
    const email = document.getElementById('email').value;
    const password = document.getElementById('password').value;
    const submitBtn = loginForm.querySelector('button');
    submitBtn.textContent = 'Iniciando...';

    try {
        await signInWithEmailAndPassword(auth, email, password);
        // onAuthStateChanged will handle the transition
    } catch (error) {
        loginError.textContent = 'Error: Credenciales incorrectas o usuario no encontrado.';
    } finally {
        submitBtn.textContent = 'Entrar';
    }
});

// 3. Logout Handler
logoutBtn.addEventListener('click', () => {
    signOut(auth);
});

// 4. Navigation Handler
navItems.forEach(item => {
    item.addEventListener('click', (e) => {
        e.preventDefault();
        
        // Remove active from all
        navItems.forEach(n => n.classList.remove('active'));
        views.forEach(v => v.classList.remove('active'));
        
        // Add active to clicked
        item.classList.add('active');
        const targetId = `view-${item.dataset.target}`;
        document.getElementById(targetId).classList.add('active');
        if (item.dataset.target === 'audit') loadAudit();
        if (item.dataset.target === 'files') loadTestFiles();
        if (item.dataset.target === 'webtraffic') loadWebTraffic();
    });
});

// 5. Load Dashboard Data
function loadDashboardData() {
    loadMetrics();
    loadMessages();
    loadUsers();
    loadGlobalSettings();
    loadReleaseCandidates();
    loadDiagnostics();
}

// ---- Diagnósticos DXF ----
function escapeHtmlDiag(s) {
    return String(s == null ? '' : s).replace(/[&<>"']/g, (c) => (
        { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]
    ));
}

// ============================================================
//  ARCHIVOS DE PRUEBA — subir DXF/STEP para descargar en otra PC
//  Usa Firebase Storage (carpeta test-files/). Las URLs de descarga
//  llevan token y funcionan en cualquier PC sin login.
// ============================================================
const TEST_FILES_DIR = 'test-files';

function humanSize(bytes) {
    const b = Number(bytes) || 0;
    if (b < 1024) return b + ' B';
    if (b < 1024 * 1024) return (b / 1024).toFixed(1) + ' KB';
    return (b / (1024 * 1024)).toFixed(2) + ' MB';
}

async function loadTestFiles() {
    const container = document.getElementById('files-list');
    if (!container) return;
    container.innerHTML = '<p class="loading">Cargando archivos...</p>';
    try {
        const res = await listAll(storageRef(storage, TEST_FILES_DIR));
        if (!res.items.length) {
            container.innerHTML = '<p class="loading">Todavía no subiste archivos de prueba. Usá el botón de arriba para cargar DXF o STEP.</p>';
            return;
        }
        const rows = await Promise.all(res.items.map(async (itemRef) => {
            let meta = {}, url = '#';
            try { meta = await getMetadata(itemRef); } catch (_) {}
            try { url = await getDownloadURL(itemRef); } catch (_) {}
            return {
                name: itemRef.name,
                url,
                when: meta.timeCreated ? new Date(meta.timeCreated).toLocaleString('es-AR') : '—',
                size: humanSize(meta.size),
                created: meta.timeCreated || '',
            };
        }));
        rows.sort((a, b) => String(b.created).localeCompare(String(a.created)));
        const btn = 'padding:6px 12px;font-size:13px;width:auto;display:inline-block;';
        let html = `<table class="diag-table" style="width:100%;border-collapse:collapse;">
            <thead><tr style="text-align:left;"><th>Archivo</th><th>Tamaño</th><th>Subido</th><th style="min-width:260px;">Acciones</th></tr></thead><tbody>`;
        for (const r of rows) {
            html += `<tr>
                <td>${escapeHtmlDiag(r.name)}</td>
                <td>${r.size}</td>
                <td>${escapeHtmlDiag(r.when)}</td>
                <td>
                    <a class="btn-primary" href="${r.url}" download="${escapeHtmlDiag(r.name)}" style="${btn}text-decoration:none;">Descargar</a>
                    <button class="btn-secondary files-copy" data-url="${escapeHtmlDiag(r.url)}" style="${btn}">Copiar link</button>
                    <button class="btn-danger files-del" data-name="${escapeHtmlDiag(r.name)}" style="${btn}">Borrar</button>
                </td></tr>`;
        }
        html += '</tbody></table>';
        container.innerHTML = html;
        container.querySelectorAll('.files-del').forEach((b) => b.addEventListener('click', () => deleteTestFile(b.dataset.name)));
        container.querySelectorAll('.files-copy').forEach((b) => b.addEventListener('click', () => {
            navigator.clipboard.writeText(b.dataset.url).then(() => { const t = b.textContent; b.textContent = '¡Copiado!'; setTimeout(() => { b.textContent = t; }, 1500); });
        }));
    } catch (e) {
        container.innerHTML = `<p class="loading" style="color:#ff8a8a;">No se pudieron listar los archivos: ${escapeHtmlDiag(e.message || e)}.<br>Si el error menciona permisos/unauthorized, hay que permitir la carpeta <b>test-files/</b> en las reglas de Storage de Firebase.</p>`;
        console.error(e);
    }
}

function uploadTestFiles(fileList) {
    const files = Array.from(fileList || []);
    if (!files.length) return;
    const status = document.getElementById('files-upload-status');
    const allowed = /\.(dxf|step|stp|stl|zip)$/i;
    let done = 0;
    const valid = files.filter((f) => allowed.test(f.name));
    if (valid.length !== files.length && status) status.textContent = 'Se omitieron archivos que no son DXF/STEP/STL/ZIP.';
    valid.forEach((file) => {
        const r = storageRef(storage, `${TEST_FILES_DIR}/${file.name}`);
        const task = uploadBytesResumable(r, file, { contentType: file.type || 'application/octet-stream' });
        task.on('state_changed',
            (snap) => { if (status) status.textContent = `Subiendo ${file.name}... ${Math.round(snap.bytesTransferred / snap.totalBytes * 100)}%`; },
            (err) => { if (status) status.textContent = `Error al subir ${file.name}: ${err.message}`; console.error(err); },
            () => { done++; if (status) status.textContent = `Subido ${file.name} (${done}/${valid.length}).`; loadTestFiles(); logAudit('test_file_upload', `Subió archivo de prueba: ${file.name}`); }
        );
    });
}

async function deleteTestFile(name) {
    if (!confirm(`¿Borrar el archivo de prueba "${name}"?`)) return;
    try {
        await deleteObject(storageRef(storage, `${TEST_FILES_DIR}/${name}`));
        logAudit('test_file_delete', `Borró archivo de prueba: ${name}`);
        loadTestFiles();
    } catch (e) {
        alert('No se pudo borrar: ' + (e.message || e));
    }
}

document.getElementById('files-upload-input')?.addEventListener('change', (e) => {
    uploadTestFiles(e.target.files);
    e.target.value = '';
});
document.getElementById('files-refresh-btn')?.addEventListener('click', loadTestFiles);

// ============================================================
//  TRÁFICO WEB — telemetría de cada visitante del sitio
// ============================================================
function flagEmoji(cc) {
    return (cc && cc.length === 2)
        ? cc.toUpperCase().replace(/./g, (c) => String.fromCodePoint(127397 + c.charCodeAt(0)))
        : '🌐';
}
function fmtDur(ms) {
    const s = Math.round((ms || 0) / 1000);
    if (s < 60) return s + 's';
    const m = Math.floor(s / 60);
    return m + 'm ' + (s % 60) + 's';
}
// Nombre legible de la página a partir del path ("/" = Inicio, etc.)
function webPageName(path) {
    const p = String(path || '').split('?')[0].replace(/\/$/, '') || '/';
    if (p === '/' || p === '/index.html') return 'Inicio';
    if (p.startsWith('/online')) return 'Nesting online';
    if (p.startsWith('/manual')) return 'Manual';
    if (p.startsWith('/auth')) return 'Login / Registro';
    if (p.startsWith('/privacidad')) return 'Privacidad';
    return p;
}
function webEventLabel(e) {
    const path = escapeHtmlDiag(e.path || '');
    const name = escapeHtmlDiag(webPageName(e.path));
    const det = escapeHtmlDiag(e.detail || '');
    const where = `<b>${name}</b> <span class="release-muted">(${path})</span>`;
    switch (e.event) {
        case 'page_view': return `📄 Entró a ${where}${e.title ? ' · ' + escapeHtmlDiag(e.title) : ''}`;
        case 'click': return `🖱️ Click: <b>${det}</b>`;
        case 'download': return `⬇️ <b>Descarga</b>: ${det}`;
        case 'scroll': return `📜 Scroll ${det}`;
        case 'page_leave': return `🚪 Salió de ${where} <span class="release-muted">· ${fmtDur(e.ms)} en la página</span>`;
        default: return `• ${escapeHtmlDiag(e.event)} ${det}`;
    }
}

let lastWebVisits = [];

function buildWebVisits(events) {
    // events vienen ordenados desc por ts; agrupamos por sessionId.
    const map = new Map();
    for (const e of events) {
        const sid = e.sessionId || ('?' + (e.visitorId || ''));
        let v = map.get(sid);
        if (!v) {
            v = { sid, visitorId: e.visitorId || '', events: [], country: '', city: '',
                  device: '', browser: '', os: '', lang: '', referrer: '', utm: '', ua: '',
                  isNew: false, start: Infinity, end: -Infinity };
            map.set(sid, v);
        }
        v.events.push(e);
        const t = e.ts ? new Date(e.ts).getTime() : 0;
        if (t) { v.start = Math.min(v.start, t); v.end = Math.max(v.end, t); }
        if (!v.country && e.country) { v.country = e.country; v.city = e.city; }
        if (!v.device && e.device) { v.device = e.device; v.browser = e.browser; v.os = e.os; }
        if (!v.ua && e.ua) v.ua = e.ua;
        if (!v.lang && e.lang) v.lang = e.lang;
        if (!v.referrer && e.referrer) v.referrer = e.referrer;
        if (!v.utm && e.utm) v.utm = e.utm;
        if (e.isNew) v.isNew = true;
    }
    const visits = [...map.values()];
    visits.forEach((v) => {
        v.events.sort((a, b) => new Date(a.ts) - new Date(b.ts));
        v.pages = [...new Set(v.events.filter((e) => e.event === 'page_view').map((e) => e.path))];
        v.downloads = v.events.filter((e) => e.event === 'download').length;
        v.clicks = v.events.filter((e) => e.event === 'click').length;
        v.duration = (v.start !== Infinity && v.end > v.start) ? v.end - v.start : 0;
        v.isBot = looksLikeBot(v);
    });
    visits.sort((a, b) => b.start - a.start);
    return visits;
}

// Heuristica de bot/monitor: user-agent conocido de bot, O navegador+SO desconocidos
// con cero interaccion (no clickeo, no scrolleo, 0-1 pagina, casi 0 segundos).
const WEB_BOT_RE = /bot|crawl|spider|slurp|headless|monitor|preview|facebookexternalhit|facebot|embedly|whatsapp|telegram|slackbot|discord|twitterbot|linkedinbot|bingpreview|curl|wget|python-requests|axios|node-fetch|http-client|go-http|java\/|okhttp|libwww|uptime|pingdom|lighthouse|gtmetrix|datadog|newrelic|phantom|puppeteer|playwright|selenium|scrapy|googlebot|applebot|yandex|baiduspider|duckduckbot|ahrefs|semrush|petalbot|amazonbot|gptbot|claudebot|ccbot|bytespider/i;
function looksLikeBot(v) {
    if (v.ua && WEB_BOT_RE.test(v.ua)) return true;
    const unknownClient = (v.browser === 'Otro' || !v.browser) && (v.os === 'Otro' || !v.os);
    const noEngagement = v.clicks === 0 && v.downloads === 0 && (v.pages.length <= 1) && v.duration < 2000;
    return unknownClient && noEngagement;
}

function renderWebSummary(events, visits) {
    const visitors = new Set(events.map((e) => e.visitorId).filter(Boolean)).size;
    const pageViews = events.filter((e) => e.event === 'page_view').length;
    const downloads = events.filter((e) => e.event === 'download').length;
    const newVisitors = visits.filter((v) => v.isNew).length;
    const tally = (arr) => {
        const m = {};
        arr.forEach((k) => { if (k) m[k] = (m[k] || 0) + 1; });
        return Object.entries(m).sort((a, b) => b[1] - a[1]);
    };
    const topCountries = tally(visits.map((v) => v.country)).slice(0, 5);
    const devices = tally(visits.map((v) => v.device));
    const topPages = tally(events.filter((e) => e.event === 'page_view').map((e) => e.path)).slice(0, 6);
    const card = (label, val) => `<div style="background:rgba(255,255,255,0.03);border:1px solid var(--border,#2a2a2a);border-radius:10px;padding:12px 16px;min-width:130px;">
        <div class="release-muted" style="font-size:11px;">${label}</div><div style="font-size:22px;font-weight:700;">${val}</div></div>`;
    const list = (label, rows, fmt) => `<div style="background:rgba(255,255,255,0.03);border:1px solid var(--border,#2a2a2a);border-radius:10px;padding:12px 16px;flex:1;min-width:220px;">
        <div class="release-muted" style="font-size:11px;margin-bottom:6px;">${label}</div>
        ${rows.length ? rows.map(fmt).join('') : '<span class="release-muted">—</span>'}</div>`;
    const el = document.getElementById('webtraffic-summary');
    if (!el) return;
    el.innerHTML = `
        <div style="display:flex;gap:10px;flex-wrap:wrap;margin-bottom:10px;">
            ${card('Visitantes únicos', visitors)}
            ${card('Visitas', visits.length)}
            ${card('Nuevos', newVisitors)}
            ${card('Páginas vistas', pageViews)}
            ${card('Descargas', downloads)}
        </div>
        <div style="display:flex;gap:10px;flex-wrap:wrap;">
            ${list('Países', topCountries, ([c, n]) => `<div style="font-size:13px;">${flagEmoji(c)} ${escapeHtmlDiag(c)} · <b>${n}</b></div>`)}
            ${list('Dispositivos', devices, ([d, n]) => `<div style="font-size:13px;">${escapeHtmlDiag(d)} · <b>${n}</b></div>`)}
            ${list('Páginas más vistas', topPages, ([p, n]) => `<div style="font-size:13px;">${escapeHtmlDiag(p)} · <b>${n}</b></div>`)}
        </div>`;
}

async function loadWebTraffic() {
    const container = document.getElementById('webtraffic-container');
    if (!container) return;
    container.innerHTML = '<p class="loading">Cargando tráfico...</p>';
    try {
        const idToken = await auth.currentUser.getIdToken(true);
        const resp = await fetch('/api/web-telemetry?limit=4000', { headers: { Authorization: `Bearer ${idToken}` } });
        if (!resp.ok) { const e = await resp.json().catch(() => ({})); throw new Error(e.error || `HTTP ${resp.status}`); }
        const { items } = await resp.json();
        if (!items || items.length === 0) {
            document.getElementById('webtraffic-summary').innerHTML = '';
            container.innerHTML = '<p class="loading">Todavía no hay tráfico registrado. (Se empieza a registrar cuando los visitantes entren al sitio con el tracker ya publicado.)</p>';
            return;
        }
        const visits = buildWebVisits(items);
        lastWebVisits = visits;
        const realVisits = visits.filter((v) => !v.isBot);
        const botVisits = visits.filter((v) => v.isBot);
        const realEvents = realVisits.reduce((acc, v) => acc.concat(v.events), []);
        // El resumen cuenta solo visitantes REALES (sin bots/monitores).
        renderWebSummary(realEvents, realVisits);

        const cardHtml = (v) => {
            const started = v.start !== Infinity ? new Date(v.start).toLocaleString('es-AR') : '—';
            const ref = v.referrer ? (() => { try { return new URL(v.referrer).hostname; } catch (e) { return v.referrer; } })() : 'directo';
            const dev = [v.device, v.browser, v.os].filter(Boolean).join(' · ');
            const rows = v.events.map((e) => {
                const hora = e.ts ? new Date(e.ts).toLocaleTimeString('es-AR') : '—';
                return `<div style="display:flex;gap:10px;padding:5px 0;border-bottom:1px solid rgba(255,255,255,0.05);">
                    <span class="mono release-muted" style="white-space:nowrap;font-size:12px;">${hora}</span>
                    <span style="font-size:13px;">${webEventLabel(e)}</span></div>`;
            }).join('');
            const botTag = v.isBot ? '<span style="background:#5a3a18;color:#ffe;font-size:11px;padding:2px 8px;border-radius:10px;">🤖 bot/monitor</span>' : '';
            return `
                <details class="session-group" style="margin-bottom:10px;border:1px solid var(--border,#2a2a2a);border-radius:10px;overflow:hidden;${v.isBot ? 'opacity:0.7;' : ''}">
                    <summary style="cursor:pointer;padding:12px 14px;display:flex;align-items:center;gap:12px;flex-wrap:wrap;background:rgba(255,255,255,0.02);">
                        <span style="font-size:16px;">${flagEmoji(v.country)}</span>
                        <span class="audit-tag">${escapeHtmlDiag((v.city ? v.city + ', ' : '') + (v.country || '—'))}</span>
                        ${botTag}
                        ${v.isNew ? '<span style="background:#2e6547;color:#dff;font-size:11px;padding:2px 8px;border-radius:10px;">nuevo</span>' : '<span class="release-muted" style="font-size:11px;">recurrente</span>'}
                        <span class="release-muted" style="font-size:12px;">${escapeHtmlDiag(dev || '—')}</span>
                        <span class="release-muted" style="font-size:12px;">· ${started}</span>
                        <span class="release-muted" style="font-size:12px;">· ${v.pages.length} pág · ${v.events.length} eventos · ${fmtDur(v.duration)}${v.downloads ? ' · ⬇️' + v.downloads : ''}</span>
                        <span class="release-muted" style="font-size:12px;margin-left:auto;">desde: ${escapeHtmlDiag(ref)}</span>
                    </summary>
                    <div style="padding:8px 16px 12px;">
                        <div class="release-muted" style="font-size:12px;margin-bottom:6px;">
                            Visitante <span class="mono">${escapeHtmlDiag(v.visitorId.slice(0, 12))}</span>
                            ${v.lang ? ' · idioma ' + escapeHtmlDiag(v.lang) : ''}${v.utm ? ' · ' + escapeHtmlDiag(v.utm) : ''}
                            ${v.ua ? '<br>' + escapeHtmlDiag(v.ua.slice(0, 160)) : ''}
                        </div>
                        ${rows || '<span class="release-muted">Sin eventos.</span>'}
                    </div>
                </details>`;
        };

        const realHtml = realVisits.map(cardHtml).join('');
        const botHtml = botVisits.map(cardHtml).join('');
        container.innerHTML = `
            <div style="display:flex;align-items:center;gap:12px;margin-bottom:12px;flex-wrap:wrap;">
                <label style="display:flex;align-items:center;gap:6px;font-size:13px;cursor:pointer;">
                    <input type="checkbox" id="wt-hide-bots" checked> Ocultar bots / monitores
                </label>
                <span class="release-muted" style="font-size:12px;">${realVisits.length} visita(s) real(es)${botVisits.length ? ` · ${botVisits.length} bot(s)/monitor(es)` : ''}</span>
            </div>
            <div id="wt-real">${realHtml || '<p class="loading">Sin visitas reales todavía.</p>'}</div>
            <div id="wt-bots" style="display:none;">${botHtml}</div>`;

        const hideBots = document.getElementById('wt-hide-bots');
        const botsBox = document.getElementById('wt-bots');
        if (hideBots && botsBox) hideBots.addEventListener('change', () => {
            botsBox.style.display = hideBots.checked ? 'none' : 'block';
        });
    } catch (e) {
        container.innerHTML = `<p class="loading" style="color:#ff8a8a;">No se pudo cargar el tráfico: ${escapeHtmlDiag(e.message || e)}.</p>`;
        console.error(e);
    }
}
document.getElementById('webtraffic-refresh-btn')?.addEventListener('click', loadWebTraffic);

// ============================================================
//  AUDITORÍA — registro de acciones del panel
// ============================================================
async function logAudit(action, description, details = null) {
    try {
        await adminApi('/api/web-telemetry', {
            method: 'POST',
            body: JSON.stringify({
                adminOp: 'logAudit',
            action,
            description,
            admin: currentAdminEmail || '(desconocido)',
            details: details || null,
            })
        });
    } catch (e) {
        // No bloquear la acción principal si falla el registro.
        console.error('No se pudo registrar la auditoría:', e);
    }
}

// Paginacion de la auditoria: trae de a 300 (el registro crece para siempre y
// leerlo completo se vuelve lento y caro en lecturas de Firestore).
const AUDIT_PAGE = 300;
let auditLastDoc = null;
let auditShown = 0;
let auditNextOffset = 0;

function auditRowsHtml(rows) {
    let html = '';
    for (const r of rows) {
        let when = '—';
        if (r.ts && r.ts.toDate) when = r.ts.toDate().toLocaleString('es-AR');
        else if (r.dateISO) when = new Date(r.dateISO).toLocaleString('es-AR');
        html += `
            <tr>
                <td style="white-space:nowrap;">${escapeHtmlDiag(when)}</td>
                <td><span class="audit-tag">${escapeHtmlDiag(r.action || '—')}</span></td>
                <td>${escapeHtmlDiag(r.description || '—')}</td>
                <td class="mono" style="font-size:12px;">${escapeHtmlDiag(r.admin || '—')}</td>
            </tr>`;
    }
    return html;
}

async function loadAudit(append = false) {
    const container = document.getElementById('audit-container');
    if (!container) return;
    if (!append) {
        container.innerHTML = '<p class="loading">Cargando registro...</p>';
        auditLastDoc = null;
        auditShown = 0;
        auditNextOffset = 0;
    }
    try {
        const data = await adminApi(`/api/web-telemetry?adminAudit=1&limit=${AUDIT_PAGE}&offset=${append ? auditNextOffset : 0}`);
        const rows = data.items || [];
        auditNextOffset = data.nextOffset ?? 0;
        const hasMore = data.nextOffset !== null && data.nextOffset !== undefined;

        if (!append && rows.length === 0) {
            container.innerHTML = '<p class="loading">Todavía no hay acciones registradas.</p>';
            return;
        }

        auditShown += rows.length;

        if (!append) {
            container.innerHTML = `
                <div style="display:flex;align-items:center;gap:12px;margin-bottom:14px;">
                    <span class="release-muted" id="audit-count">${auditShown} acción(es) mostrada(s)</span>
                </div>
                <div style="overflow-x:auto;">
                <table class="diag-table" style="width:100%;border-collapse:collapse;">
                    <thead><tr style="text-align:left;">
                        <th>Fecha y hora</th><th>Acción</th><th>Descripción</th><th>Admin</th>
                    </tr></thead>
                    <tbody id="audit-tbody">${auditRowsHtml(rows)}</tbody>
                </table>
                </div>
                <div style="margin-top:14px;text-align:center;">
                    <button class="btn-secondary" id="audit-more-btn" style="display:${hasMore ? 'inline-block' : 'none'};">Cargar ${AUDIT_PAGE} más</button>
                </div>`;
            const moreBtn = document.getElementById('audit-more-btn');
            if (moreBtn) moreBtn.addEventListener('click', () => {
                moreBtn.disabled = true;
                loadAudit(true).finally(() => { moreBtn.disabled = false; });
            });
        } else {
            const tbody = document.getElementById('audit-tbody');
            if (tbody) tbody.insertAdjacentHTML('beforeend', auditRowsHtml(rows));
            const count = document.getElementById('audit-count');
            if (count) count.textContent = `${auditShown} acción(es) mostrada(s)`;
            const moreBtn = document.getElementById('audit-more-btn');
            if (moreBtn) moreBtn.style.display = hasMore ? 'inline-block' : 'none';
        }
    } catch (e) {
        console.error('Error loading audit log:', e);
        container.innerHTML = `<p class="error-msg">No se pudo cargar la auditoría: ${escapeHtmlDiag(e.message)}</p>`;
    }
}

// Sesiones armadas en la ultima carga (para el boton Informe).
let lastDiagSessions = [];

const SESSION_GAP_MS = 30 * 60 * 1000; // 30 min sin actividad => sesion nueva

function runTime(r) {
    if (r.timestampISO) return new Date(r.timestampISO).getTime();
    if (r.timestamp && r.timestamp.toDate) return r.timestamp.toDate().getTime();
    if (r.date) return new Date(r.date + 'T12:00:00').getTime();
    return 0;
}

// Agrupa diagnosticos (DXF) y corridas en SESIONES.
//  - Si traen sessionId (datos nuevos), se agrupan por ese id.
//  - Si no (datos viejos), se clusteriza por maquina + huecos de 30 min.
function buildSessions(items, runs) {
    const byKey = new Map();
    const keyOf = (sid, machine, t) => {
        if (sid) return 'sid:' + sid;
        // cluster temporal por maquina (se ajusta al insertar)
        return 'tmp:' + (machine || '?') + ':' + Math.floor(t / SESSION_GAP_MS);
    };

    // Helper para conseguir/crear la sesion de un evento viejo (sin sid):
    // busca una sesion de la misma maquina cuyo rango este a < 30 min.
    const tmpSessions = [];
    function placeLegacy(machine, t) {
        for (const s of tmpSessions) {
            if (s.machine === machine && t >= s.start - SESSION_GAP_MS && t <= s.end + SESSION_GAP_MS) {
                s.start = Math.min(s.start, t);
                s.end = Math.max(s.end, t);
                return s;
            }
        }
        const s = { id: 'legacy_' + tmpSessions.length, sid: '', machine, start: t, end: t, dxfs: [], runs: [], country: '' };
        tmpSessions.push(s);
        return s;
    }

    const sidSessions = new Map();
    function placeSid(sid, machine, t) {
        let s = sidSessions.get(sid);
        if (!s) { s = { id: sid, sid, machine, start: t, end: t, dxfs: [], runs: [], country: '' }; sidSessions.set(sid, s); }
        s.start = Math.min(s.start, t); s.end = Math.max(s.end, t);
        if (!s.machine || s.machine === '?') s.machine = machine;
        return s;
    }

    // 1) DXFs
    for (const it of items) {
        const m = it.meta || {};
        const sid = it.sessionId || m.sessionId || '';
        const machine = m.machine || '—';
        const t = it.createdAt ? new Date(it.createdAt).getTime() : 0;
        const s = sid ? placeSid(sid, machine, t) : placeLegacy(machine, t);
        s.dxfs.push(it);
        // El pais ahora tambien viene en el diagnostico (al cargar), asi ubicamos
        // a los que cargan piezas pero nunca corren el solver.
        if (!s.country && it.country) s.country = it.country;
    }
    // 2) Corridas
    for (const r of runs) {
        const sid = r.sessionId || '';
        const machine = r.machine || (r.source === 'online' ? 'navegador (online)' : '—');
        const t = runTime(r);
        const s = sid ? placeSid(sid, machine, t) : placeLegacy(machine, t);
        s.runs.push(r);
        if (!s.country && r.country) s.country = r.country;
    }

    const grouped = [...sidSessions.values(), ...tmpSessions];

    // El sessionId del programa es por-LANZAMIENTO, pero si el usuario deja el
    // programa abierto varios dias, una sola "sesion" termina abarcando dias y
    // mezcla cargas de fechas distintas (ej: archivos del 24 bajo una sesion del
    // 22). Partimos cada sesion en SENTADAS reales: cortamos donde hay un hueco
    // de mas de 30 min sin actividad (misma nocion que usa el resto del panel).
    function splitByGaps(s) {
        const ev = [];
        s.dxfs.forEach((d) => ev.push({ t: d.createdAt ? new Date(d.createdAt).getTime() : 0, dxf: d }));
        s.runs.forEach((r) => ev.push({ t: runTime(r), run: r }));
        ev.sort((a, b) => a.t - b.t);
        if (!ev.length) return [];
        const parts = [];
        let cur = null;
        for (const e of ev) {
            if (!cur || (e.t - cur.end) > SESSION_GAP_MS) {
                cur = { sid: s.sid || '', machine: s.machine, country: s.country,
                        start: e.t, end: e.t, dxfs: [], runs: [] };
                parts.push(cur);
            }
            cur.end = e.t;
            if (e.dxf) cur.dxfs.push(e.dxf); else cur.runs.push(e.run);
        }
        parts.forEach((g, k) => {
            // id unico por sentada (para el DOM); sid conserva el id real de la
            // sesion del programa, asi el Informe puede traer los eventos.
            g.id = parts.length > 1 ? `${s.id}__p${k + 1}` : s.id;
            g.part = parts.length > 1 ? (k + 1) : 0;
            g.partsTotal = parts.length;
            if (!g.country && s.country) g.country = s.country;
        });
        return parts;
    }

    let all = [];
    grouped.forEach((s) => { all = all.concat(splitByGaps(s)); });
    // Ordenar todo por hora dentro de cada sentada y las sentadas por inicio desc.
    all.forEach((s) => {
        s.dxfs.sort((a, b) => String(a.createdAt).localeCompare(String(b.createdAt)));
        s.runs.sort((a, b) => runTime(a) - runTime(b));
    });
    all.sort((a, b) => b.start - a.start);
    // Este panel es de Diagnosticos DXF: solo mostramos sentadas que tengan al
    // menos un archivo. Las corridas sin DXF asociado (datos viejos previos al
    // diagnostico, subidas que fallaron, o re-corridas online sin archivos
    // nuevos) generaban banners vacios "0 DXF" que aca no aportan nada.
    return all.filter((s) => s.dxfs.length > 0);
}

// Modal con el informe de lo que hizo el usuario en una sesion (timeline + resumen).
// Nombres legibles de las acciones que registra el programa (track_event).
const ACTION_LABELS = {
    abrir_programa: '🟢 Abrió el programa',
    cerrar_programa: '🔴 Cerró el programa',
    cargar_dxf: '📄 Cargó',
    agregar_carpeta: '📁 Agregar carpeta',
    agregar_dxf: '➕ Abrió "Agregar DXF"',
    agregar_step: '➕ Abrió "Agregar STEP"',
    quitar_seleccionado: '🗑️ Quitó una pieza',
    aplicar_cantidad: '🔢 Aplicó cantidad',
    llenar_chapa: '🧩 Llenar chapa',
    'boton_ejecutar/detener': '▶️ Ejecutar / Detener',
    pausar: '⏸️ Pausar',
    guardar_dxf: '💾 Guardar DXF',
    reporte: '📋 Abrió el reporte',
    tool_seleccionar: '🛠️ Herramienta: Seleccionar',
    tool_pan: '🛠️ Herramienta: Pan',
    tool_fit: '🛠️ Herramienta: Ajustar vista',
    tool_medir: '📐 Herramienta: Medir',
    tool_linea: '🛠️ Dibujó: Línea',
    tool_rectangulo: '🛠️ Dibujó: Rectángulo',
    tool_circulo: '🛠️ Dibujó: Círculo',
    tool_arco: '🛠️ Dibujó: Arco',
    tool_polilinea: '🛠️ Dibujó: Polilínea',
    tool_texto: '🛠️ Dibujó: Texto',
    tool_borrar: '🗑️ Borró anotación',
    tool_limpiar: '🧹 Limpió anotaciones',
    tool_grilla: '▦ Grilla on/off',
    tool_undo: '↩️ Deshacer',
    feedback: '💬 Feedback',
    idioma_es: '🌐 Cambió a Español',
    idioma_en: '🌐 Cambió a English',
    menu_grafico: '📊 Gráfico de ahorro',
    menu_arbol: '🌳 Árbol evolutivo',
    // Módulo Desplegado de chapa (STEP → DXF)
    abrir_modulo_step: '📐 Abrió módulo Desplegado de chapa',
    volver_a_nesting: '↩️ Volvió a Nesting',
    step_cargar: '📥 Cargó STEP',
    step_transformar: '🔄 Transformó STEP a DXF',
    step_enviar_nesting: '📤 Envió DXF al nesting',
    step_quitar: '🗑️ Quitó el STEP',
};
function eventLabel(action, detail) {
    const base = ACTION_LABELS[action] || ('• ' + action);
    return detail ? `${base} <b>${escapeHtmlDiag(detail)}</b>` : base;
}

async function showSessionReport(s) {
    if (!s) return;
    const fmt = (t) => t ? new Date(t).toLocaleString('es-AR') : '—';
    const flag = s.country && s.country.length === 2
        ? s.country.replace(/./g, (c) => String.fromCodePoint(127397 + c.charCodeAt(0))) : '';
    const mins = s.start && s.end ? Math.round((s.end - s.start) / 60000) : 0;

    // Eventos del timeline (cargas + corridas) ordenados por hora.
    const events = [];
    s.dxfs.forEach((it) => {
        const m = it.meta || {};
        const bbox = (m.bboxW && m.bboxH) ? `${Math.round(m.bboxW)}×${Math.round(m.bboxH)} mm` : '';
        events.push({ t: it.createdAt ? new Date(it.createdAt).getTime() : 0, kind: 'dxf',
            txt: `📄 Cargó <b>${escapeHtmlDiag(it.filename)}</b> ${bbox ? '· ' + bbox : ''}` });
    });
    s.runs.forEach((r) => {
        const util = (typeof r.final_utilization === 'number') ? r.final_utilization.toFixed(1) + '%' : '—';
        const opt = r.optimization_type === 'bounding-box' ? 'Bounding box' : (r.optimization_type === 'compact-area' ? 'Área compacta' : (r.optimization_type || ''));
        const tag = r.fill_sheet ? '🧩 Llenar chapa' : '▶ Corrida';
        const saved = r.saved ? ' · 💾 <b>guardó</b>' : '';
        const bestT = (typeof r.best_solution_time_sec === 'number' && r.best_solution_time_sec > 0) ? ` · mejor en ${r.best_solution_time_sec.toFixed(2)}s` : '';
        events.push({ t: runTime(r), kind: 'run',
            txt: `${tag}: ${r.placed_count ?? '?'} ubicadas / ${(r.placed_count||0)+(r.unplaced_count||0)} · ${r.sheets_used ?? '?'} chapa(s) · aprov. <b>${util}</b> · ${opt}${bestT}${saved}` });
    });
    events.sort((a, b) => a.t - b.t);

    let timeline = '';
    events.forEach((ev) => {
        const hora = ev.t ? new Date(ev.t).toLocaleTimeString('es-AR') : '—';
        timeline += `<div style="display:flex;gap:10px;padding:6px 0;border-bottom:1px solid rgba(255,255,255,0.05);">
            <span class="mono release-muted" style="white-space:nowrap;font-size:12px;">${hora}</span>
            <span style="font-size:13px;">${ev.txt}</span></div>`;
    });

    const saves = s.runs.filter((r) => r.saved).length;
    const bestUtil = s.runs.reduce((mx, r) => Math.max(mx, (typeof r.final_utilization === 'number' ? r.final_utilization : 0)), 0);
    const ver = (s.runs.find((r) => r.app_version) || {}).app_version || (s.dxfs.find((d) => d.meta && d.meta.appVersion) || {meta:{}}).meta.appVersion || '—';

    let overlay = document.getElementById('session-report-overlay');
    if (overlay) overlay.remove();
    overlay = document.createElement('div');
    overlay.id = 'session-report-overlay';
    overlay.style.cssText = 'position:fixed;inset:0;background:rgba(0,0,0,0.6);z-index:9999;display:flex;align-items:center;justify-content:center;padding:20px;';
    overlay.innerHTML = `
        <div style="background:var(--panel,#161616);border:1px solid var(--border,#2a2a2a);border-radius:14px;max-width:680px;width:100%;max-height:86vh;overflow:auto;box-shadow:0 18px 60px rgba(0,0,0,0.5);">
            <div style="padding:16px 20px;border-bottom:1px solid rgba(255,255,255,0.06);display:flex;align-items:center;gap:10px;">
                <span style="font-size:20px;">📋</span>
                <div style="flex:1;">
                    <div style="font-weight:700;font-size:15px;">Informe de sesión — ${flag} ${escapeHtmlDiag(s.machine)}${(s.partsTotal && s.partsTotal > 1) ? ` <span class="release-muted" style="font-weight:500;">· sentada ${s.part}/${s.partsTotal}</span>` : ''}</div>
                    <div class="release-muted" style="font-size:12px;">${fmt(s.start)} → ${fmt(s.end)} · ${mins} min · versión ${escapeHtmlDiag(ver)}</div>
                </div>
                <button id="session-report-close" class="btn-secondary" style="width:auto;padding:6px 12px;">Cerrar</button>
            </div>
            <div style="padding:14px 20px;display:flex;gap:18px;flex-wrap:wrap;border-bottom:1px solid rgba(255,255,255,0.06);">
                <div><div class="release-muted" style="font-size:11px;">DXF cargados</div><div style="font-size:18px;font-weight:700;">${s.dxfs.length}</div></div>
                <div><div class="release-muted" style="font-size:11px;">Corridas</div><div style="font-size:18px;font-weight:700;">${s.runs.length}</div></div>
                <div><div class="release-muted" style="font-size:11px;">Exportó (guardó)</div><div style="font-size:18px;font-weight:700;">${saves}</div></div>
                <div><div class="release-muted" style="font-size:11px;">Mejor aprovech.</div><div style="font-size:18px;font-weight:700;">${bestUtil ? bestUtil.toFixed(1) + '%' : '—'}</div></div>
            </div>
            <div style="padding:14px 20px;">
                <div class="release-muted" style="font-size:12px;margin-bottom:8px;">LÍNEA DE TIEMPO <span id="session-tl-note"></span></div>
                <div id="session-timeline">${timeline || '<span class="release-muted">Sin eventos.</span>'}</div>
            </div>
        </div>`;
    document.body.appendChild(overlay);
    const close = () => overlay.remove();
    overlay.addEventListener('click', (e) => { if (e.target === overlay) close(); });
    const closeBtn = document.getElementById('session-report-close');
    if (closeBtn) closeBtn.addEventListener('click', close);

    // Caja negra: si la sesion tiene eventos registrados (track_event), reconstruyo
    // el timeline COMPLETO (abrir, botones, cargas, cerrar) + las corridas. Las
    // sesiones viejas sin eventos siguen mostrando cargas + corridas como antes.
    // s.sid es el id REAL de la sesion del programa (vacio en datos legacy).
    const realSid = s.sid || '';
    if (!realSid) return;
    try {
        const snap = await getDocs(query(collection(db, 'session_events'), where('sessionId', '==', realSid), limit(1000)));
        const evs = [];
        snap.forEach((d) => evs.push(d.data()));
        if (evs.length === 0) return;

        const evTime = (e) => (e.ts && e.ts.toDate) ? e.ts.toDate().getTime() : (e.serverISO ? new Date(e.serverISO).getTime() : 0);
        // Si la sesion del programa se partio en varias sentadas (estuvo abierta
        // dias), mostramos solo los eventos de la ventana de ESTA sentada.
        const margin = 2 * 60 * 1000;
        const inWindow = (s.partsTotal && s.partsTotal > 1)
            ? (t) => t >= s.start - margin && t <= s.end + margin
            : () => true;
        const merged = [];
        evs.forEach((e) => { const t = evTime(e); if (inWindow(t)) merged.push({ t, txt: eventLabel(e.action, e.detail) }); });
        if (merged.length === 0 && s.runs.length === 0) return;
        // Las corridas (con métricas) vienen de solver_runs.
        s.runs.forEach((r) => {
            const util = (typeof r.final_utilization === 'number') ? r.final_utilization.toFixed(1) + '%' : '—';
            const opt = r.optimization_type === 'bounding-box' ? 'Bounding box' : (r.optimization_type === 'compact-area' ? 'Área compacta' : (r.optimization_type || ''));
            const tag = r.fill_sheet ? '🧩 Resultado Llenar chapa' : '🏁 Resultado corrida';
            const saved = r.saved ? ' · 💾 <b>guardó</b>' : '';
            merged.push({ t: runTime(r),
                txt: `${tag}: ${r.placed_count ?? '?'} ubicadas · ${r.sheets_used ?? '?'} chapa(s) · aprov. <b>${util}</b> · ${opt}${saved}` });
        });
        merged.sort((a, b) => a.t - b.t);

        let html = '';
        merged.forEach((ev) => {
            const hora = ev.t ? new Date(ev.t).toLocaleTimeString('es-AR') : '—';
            html += `<div style="display:flex;gap:10px;padding:6px 0;border-bottom:1px solid rgba(255,255,255,0.05);">
                <span class="mono release-muted" style="white-space:nowrap;font-size:12px;">${hora}</span>
                <span style="font-size:13px;">${ev.txt}</span></div>`;
        });
        const tl = document.getElementById('session-timeline');
        const note = document.getElementById('session-tl-note');
        if (tl) tl.innerHTML = html;
        if (note) note.textContent = `· ${evs.length} acción(es) registrada(s)`;
    } catch (e) {
        console.warn('No se pudieron cargar los eventos de la sesión:', e);
    }
}

async function loadDiagnostics() {
    const container = document.getElementById('diagnostics-container');
    if (!container) return;
    container.innerHTML = '<p class="loading">Cargando diagnósticos...</p>';
    try {
        const idToken = await auth.currentUser.getIdToken(true);
        const resp = await fetch('/api/list-diagnostics', {
            headers: { Authorization: `Bearer ${idToken}` },
        });
        if (!resp.ok) {
            const e = await resp.json().catch(() => ({}));
            throw new Error(e.error || `HTTP ${resp.status}`);
        }
        const { items } = await resp.json();
        if (!items || items.length === 0) {
            container.innerHTML = '<p class="loading">Todavía no hay DXF cargados.</p>';
            return;
        }

        // Traigo las corridas del solver para cruzarlas con cada sesion.
        let runs = [];
        try {
            const rs = await getDocs(query(collection(db, 'solver_runs'), orderBy('timestamp', 'desc'), limit(1500)));
            rs.forEach((d) => runs.push(d.data()));
        } catch (e) { console.warn('No se pudieron leer corridas para las sesiones:', e); }

        const sessions = buildSessions(items, runs);
        lastDiagSessions = sessions;

        // Aviso de "DXF nuevos": marcamos como nuevo todo lo subido despues de la
        // ultima vez que el admin apreto "Marcar como visto" (guardado en este
        // navegador). Un puntito naranja en cada archivo/sesion lo resalta.
        const SEEN_KEY = 'foglesting_diag_seen_ts';
        // La PRIMERA vez (nunca se marco nada) arrancamos con TODO como visto, asi
        // no aparecen como nuevos los archivos que ya viste. De ahi en mas, solo se
        // resalta lo que se suba despues de la ultima vez que apretaste "Marcar visto".
        const storedSeen = localStorage.getItem(SEEN_KEY);
        const firstTimeSeen = storedSeen === null;
        const seenTs = firstTimeSeen ? Infinity : Number(storedSeen || 0);
        const NEW_DOT = '<span title="Nuevo" style="display:inline-block;width:9px;height:9px;border-radius:50%;background:#ff7a18;box-shadow:0 0 6px #ff7a18;margin-right:6px;vertical-align:middle;"></span>';
        let newestTs = 0;
        let totalNew = 0;

        const btnStyle = 'padding:6px 12px;font-size:13px;width:auto;display:inline-block;';
        let groupsHtml = '';
        sessions.forEach((s, si) => {
            let rows = '';
            let sessionNew = 0;
            for (const it of s.dxfs) {
                const m = it.meta || {};
                const kb = (it.sizeBytes / 1024).toFixed(1);
                const bbox = (m.bboxW && m.bboxH) ? `${Math.round(m.bboxW)} × ${Math.round(m.bboxH)} mm` : '—';
                const date = it.createdAt ? new Date(it.createdAt).toLocaleString('es-AR') : '—';
                const itTs = it.createdAt ? new Date(it.createdAt).getTime() : 0;
                if (itTs > newestTs) newestTs = itTs;
                const isNew = itTs > seenTs;
                if (isNew) { sessionNew++; totalNew++; }
                const dl = it.truncated
                    ? '<span class="release-muted">muy grande</span>'
                    : `<button class="btn-primary diag-dl" data-id="${escapeHtmlDiag(it.id)}" data-name="${escapeHtmlDiag(it.filename)}" style="${btnStyle}">Descargar</button>`;
                const del = `<button class="btn-danger diag-del" data-id="${escapeHtmlDiag(it.id)}" data-name="${escapeHtmlDiag(it.filename)}" style="${btnStyle}margin-left:6px;">Borrar</button>`;
                rows += `
                    <tr${isNew ? ' style="background:rgba(255,122,24,0.06);"' : ''}>
                        <td style="text-align:center;"><input type="checkbox" class="diag-check" data-id="${escapeHtmlDiag(it.id)}" data-name="${escapeHtmlDiag(it.filename)}" data-truncated="${it.truncated ? '1' : '0'}"></td>
                        <td>${isNew ? NEW_DOT : ''}${escapeHtmlDiag(it.filename)}</td>
                        <td>${bbox}</td>
                        <td>${kb} KB</td>
                        <td>${escapeHtmlDiag(date)}</td>
                    </tr>`;
            }
            const started = s.start ? new Date(s.start).toLocaleString('es-AR') : '—';
            const flag = s.country && s.country.length === 2
                ? s.country.replace(/./g, (c) => String.fromCodePoint(127397 + c.charCodeAt(0))) : '';
            const mins = s.start && s.end ? Math.round((s.end - s.start) / 60000) : 0;
            const durTxt = mins >= 1 ? `${mins} min` : '<1 min';
            groupsHtml += `
                <details class="session-group" style="margin-bottom:10px;border:1px solid var(--border, #2a2a2a);border-radius:10px;overflow:hidden;">
                    <summary style="cursor:pointer;padding:12px 14px;display:flex;align-items:center;gap:12px;flex-wrap:wrap;background:rgba(255,255,255,0.02);">
                        ${sessionNew > 0 ? NEW_DOT : ''}<span class="audit-tag">${flag} ${escapeHtmlDiag(s.machine)}</span>
                        <span class="release-muted">${escapeHtmlDiag(started)}</span>
                        <span class="release-muted">· ${s.dxfs.length} DXF · ${s.runs.length} corrida(s) · ${durTxt}</span>
                        ${sessionNew > 0 ? `<span style="background:#ff7a18;color:#1a1205;font-weight:700;font-size:11px;padding:2px 8px;border-radius:10px;">${sessionNew} nuevo${sessionNew > 1 ? 's' : ''}</span>` : ''}
                        <button class="btn-primary diag-report" data-si="${si}" type="button" style="${btnStyle}margin-left:auto;">📋 Informe</button>
                    </summary>
                    <div style="overflow-x:auto;padding:6px 10px 12px;">
                    <table class="diag-table" style="width:100%;border-collapse:collapse;">
                        <thead><tr style="text-align:left;">
                            <th style="width:34px;text-align:center;"><input type="checkbox" class="diag-check-group" title="Seleccionar la sesión"></th>
                            <th>Archivo</th><th>Tamaño pieza</th><th>Peso</th><th>Fecha</th>
                        </tr></thead>
                        <tbody>${rows}</tbody>
                    </table>
                    </div>
                </details>`;
        });

        // Primera vez: dejamos registrado lo mas nuevo como "visto" para que de ahora
        // en mas solo se resalten las cargas posteriores (no aparece todo como nuevo).
        if (firstTimeSeen) localStorage.setItem(SEEN_KEY, String(newestTs || Date.now()));

        container.innerHTML = `
            <div style="display:flex;align-items:center;gap:12px;margin-bottom:14px;flex-wrap:wrap;">
                <button class="btn-primary" id="diag-download-selected" type="button" style="width:auto;" disabled>
                    Descargar seleccionados
                </button>
                <button class="btn-danger" id="diag-delete-selected" type="button" style="width:auto;" disabled>
                    Borrar seleccionados (<span id="diag-selected-count">0</span>)
                </button>
                ${totalNew > 0 ? `<button class="btn-secondary" id="diag-mark-seen" type="button" style="width:auto;">✓ Marcar como visto</button>
                <span style="background:#ff7a18;color:#1a1205;font-weight:700;font-size:12px;padding:3px 10px;border-radius:10px;">${totalNew} DXF nuevo${totalNew > 1 ? 's' : ''}</span>` : ''}
                <span class="release-muted" id="diag-total-count">${items.length} archivo(s) · ${sessions.length} sesión(es)</span>
            </div>
            ${groupsHtml}`;

        const markSeenBtn = document.getElementById('diag-mark-seen');
        if (markSeenBtn) markSeenBtn.addEventListener('click', () => {
            localStorage.setItem(SEEN_KEY, String(newestTs || Date.now()));
            loadDiagnostics();
        });

        container.querySelectorAll('.diag-dl').forEach((btn) => {
            btn.addEventListener('click', () => downloadDiagnostic(btn.dataset.id, btn.dataset.name));
        });
        container.querySelectorAll('.diag-del').forEach((btn) => {
            btn.addEventListener('click', () => deleteDiagnostic(btn.dataset.id, btn.dataset.name));
        });
        container.querySelectorAll('.diag-report').forEach((btn) => {
            btn.addEventListener('click', (ev) => { ev.preventDefault(); showSessionReport(lastDiagSessions[Number(btn.dataset.si)]); });
        });
        // Casilla "seleccionar toda la sesion"
        container.querySelectorAll('.session-group').forEach((grp) => {
            const groupCheck = grp.querySelector('.diag-check-group');
            const groupChecks = Array.from(grp.querySelectorAll('.diag-check'));
            if (groupCheck) groupCheck.addEventListener('change', () => {
                groupChecks.forEach((c) => { c.checked = groupCheck.checked; });
                groupChecks.forEach((c) => c.dispatchEvent(new Event('change', { bubbles: true })));
            });
        });

        // --- Casillas de seleccion + borrado multiple ---
        const checks = Array.from(container.querySelectorAll('.diag-check'));
        const checkAll = container.querySelector('#diag-check-all');
        const selBtn = container.querySelector('#diag-delete-selected');
        const dlBtn = container.querySelector('#diag-download-selected');
        const countEl = container.querySelector('#diag-selected-count');
        const refreshSelection = () => {
            const selected = checks.filter((c) => c.checked);
            if (countEl) countEl.textContent = String(selected.length);
            if (selBtn) selBtn.disabled = selected.length === 0;
            if (dlBtn) dlBtn.disabled = selected.length === 0;
            if (checkAll) checkAll.checked = selected.length > 0 && selected.length === checks.length;
        };
        checks.forEach((c) => c.addEventListener('change', refreshSelection));
        if (checkAll) {
            checkAll.addEventListener('change', () => {
                checks.forEach((c) => { c.checked = checkAll.checked; });
                refreshSelection();
            });
        }
        if (selBtn) {
            selBtn.addEventListener('click', () => {
                const ids = checks.filter((c) => c.checked).map((c) => c.dataset.id);
                deleteSelectedDiagnostics(ids);
            });
        }
        if (dlBtn) {
            dlBtn.addEventListener('click', () => {
                const sel = checks.filter((c) => c.checked && c.dataset.truncated !== '1');
                downloadManyDiagnostics(sel.map((c) => ({ id: c.dataset.id, name: c.dataset.name })));
            });
        }
    } catch (e) {
        console.error('Error loading diagnostics:', e);
        container.innerHTML = `<p class="error-msg">No se pudieron cargar los diagnósticos: ${escapeHtmlDiag(e.message)}</p>`;
    }
}

async function deleteDiagnostic(id, filename) {
    if (!confirm(`¿Borrar el diagnóstico "${filename || id}"?`)) return;
    try {
        const idToken = await auth.currentUser.getIdToken(true);
        const resp = await fetch('/api/delete-diagnostic', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${idToken}` },
            body: JSON.stringify({ id }),
        });
        if (!resp.ok) {
            const e = await resp.json().catch(() => ({}));
            alert('No se pudo borrar: ' + (e.error || resp.status));
            return;
        }
        logAudit('Borrar diagnóstico', `Borró el diagnóstico DXF "${filename || id}".`);
        loadDiagnostics();
    } catch (e) {
        console.error('Error deleting diagnostic:', e);
        alert('Error al borrar el diagnóstico.');
    }
}

async function deleteAllDiagnostics() {
    if (!confirm('¿Borrar TODOS los DXF de diagnóstico? Esta acción no se puede deshacer.')) return;
    try {
        const idToken = await auth.currentUser.getIdToken(true);
        const resp = await fetch('/api/delete-diagnostic', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${idToken}` },
            body: JSON.stringify({ all: true }),
        });
        if (!resp.ok) {
            const e = await resp.json().catch(() => ({}));
            alert('No se pudo borrar: ' + (e.error || resp.status));
            return;
        }
        logAudit('Borrar diagnósticos', 'Borró TODOS los DXF de diagnóstico.');
        loadDiagnostics();
    } catch (e) {
        console.error('Error deleting all diagnostics:', e);
        alert('Error al borrar los diagnósticos.');
    }
}

async function deleteSelectedDiagnostics(ids) {
    if (!ids || ids.length === 0) return;
    if (!confirm(`¿Borrar ${ids.length} DXF de diagnóstico seleccionado(s)? Esta acción no se puede deshacer.`)) return;
    try {
        const idToken = await auth.currentUser.getIdToken(true);
        const resp = await fetch('/api/delete-diagnostic', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${idToken}` },
            body: JSON.stringify({ ids }),
        });
        if (!resp.ok) {
            const e = await resp.json().catch(() => ({}));
            alert('No se pudo borrar: ' + (e.error || resp.status));
            return;
        }
        logAudit('Borrar diagnósticos', `Borró ${ids.length} DXF de diagnóstico seleccionado(s).`);
        loadDiagnostics();
    } catch (e) {
        console.error('Error deleting selected diagnostics:', e);
        alert('Error al borrar los diagnósticos seleccionados.');
    }
}

async function downloadManyDiagnostics(list) {
    if (!list || list.length === 0) return;
    const btn = document.getElementById('diag-download-selected');
    if (btn) { btn.disabled = true; btn.textContent = `Descargando 0/${list.length}...`; }
    for (let i = 0; i < list.length; i++) {
        await downloadDiagnostic(list[i].id, list[i].name);
        if (btn) btn.textContent = `Descargando ${i + 1}/${list.length}...`;
        // Pausa para que el navegador no bloquee descargas multiples.
        await new Promise((r) => setTimeout(r, 350));
    }
    if (btn) { btn.textContent = 'Descargar seleccionados'; btn.disabled = false; }
}

async function downloadDiagnostic(id, filename) {
    try {
        const idToken = await auth.currentUser.getIdToken(true);
        const resp = await fetch(`/api/list-diagnostics?id=${encodeURIComponent(id)}`, {
            headers: { Authorization: `Bearer ${idToken}` },
        });
        if (!resp.ok) { alert('No se pudo descargar el DXF.'); return; }
        const blob = await resp.blob();
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename || 'pieza.dxf';
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(url);
    } catch (e) {
        console.error('Error downloading diagnostic:', e);
        alert('Error al descargar el DXF.');
    }
}

document.getElementById('diagnostics-refresh-btn')?.addEventListener('click', loadDiagnostics);
document.getElementById('diagnostics-delete-all-btn')?.addEventListener('click', deleteAllDiagnostics);
document.getElementById('audit-refresh-btn')?.addEventListener('click', loadAudit);

// 5a. Load Metrics and draw Chart
let metricsChartInstance = null;
let solverUsageChartInstance = null;
let solverUtilChartInstance = null;
let solverSourceChartInstance = null;
let solverCountryChartInstance = null;

function renderSolverSourceChart(onlineN, desktopN) {
    const canvas = document.getElementById('solverSourceChart');
    if (!canvas || typeof Chart === 'undefined') return;
    const ctx = canvas.getContext('2d');
    if (solverSourceChartInstance) solverSourceChartInstance.destroy();
    solverSourceChartInstance = new Chart(ctx, {
        type: 'doughnut',
        data: {
            labels: ['Online', 'Descargable'],
            datasets: [{ data: [onlineN, desktopN], backgroundColor: ['#e85b2e', '#3d8760'], borderColor: '#1a1a1a', borderWidth: 2 }]
        },
        options: {
            responsive: true, maintainAspectRatio: false,
            plugins: { legend: { position: 'bottom', labels: { color: '#f0f0f0' } } }
        }
    });
}

function renderSolverCountryChart(labels, counts) {
    const canvas = document.getElementById('solverCountryChart');
    if (!canvas || typeof Chart === 'undefined') return;
    const ctx = canvas.getContext('2d');
    if (solverCountryChartInstance) solverCountryChartInstance.destroy();
    solverCountryChartInstance = new Chart(ctx, {
        type: 'bar',
        data: {
            labels,
            datasets: [{ label: 'Corridas', data: counts, backgroundColor: 'rgba(232,91,46,0.6)', borderColor: '#e85b2e', borderWidth: 1 }]
        },
        options: {
            indexAxis: 'y',
            responsive: true, maintainAspectRatio: false,
            scales: {
                x: { beginAtZero: true, grid: { color: '#333' }, ticks: { color: '#a0a8a3', precision: 0 } },
                y: { grid: { color: '#333' }, ticks: { color: '#a0a8a3' } }
            },
            plugins: { legend: { display: false } }
        }
    });
}

function renderSolverUsageChart(labels, runs, saves) {
    const canvas = document.getElementById('solverUsageChart');
    if (!canvas || typeof Chart === 'undefined') return;
    const ctx = canvas.getContext('2d');
    if (solverUsageChartInstance) solverUsageChartInstance.destroy();
    solverUsageChartInstance = new Chart(ctx, {
        type: 'bar',
        data: {
            labels,
            datasets: [
                { label: 'Corridas', data: runs, backgroundColor: 'rgba(232,91,46,0.6)', borderColor: '#e85b2e', borderWidth: 1 },
                { label: 'Guardadas', data: saves, backgroundColor: 'rgba(40,167,69,0.6)', borderColor: '#28a745', borderWidth: 1 }
            ]
        },
        options: {
            responsive: true, maintainAspectRatio: false,
            scales: {
                y: { beginAtZero: true, grid: { color: '#333' }, ticks: { color: '#a0a8a3', precision: 0 } },
                x: { grid: { color: '#333' }, ticks: { color: '#a0a8a3' } }
            },
            plugins: { legend: { labels: { color: '#f0f0f0' } } }
        }
    });
}

function renderSolverUtilChart(labels, util) {
    const canvas = document.getElementById('solverUtilChart');
    if (!canvas || typeof Chart === 'undefined') return;
    const ctx = canvas.getContext('2d');
    if (solverUtilChartInstance) solverUtilChartInstance.destroy();
    solverUtilChartInstance = new Chart(ctx, {
        type: 'line',
        data: {
            labels,
            datasets: [
                { label: 'Aprovechamiento %', data: util, borderColor: '#3b82f6', backgroundColor: 'rgba(59,130,246,0.1)', borderWidth: 2, tension: 0.3, fill: true, spanGaps: true }
            ]
        },
        options: {
            responsive: true, maintainAspectRatio: false,
            scales: {
                y: { beginAtZero: true, max: 100, grid: { color: '#333' }, ticks: { color: '#a0a8a3', callback: (v) => v + '%' } },
                x: { grid: { color: '#333' }, ticks: { color: '#a0a8a3' } }
            },
            plugins: { legend: { labels: { color: '#f0f0f0' } } }
        }
    });
}

function renderMetricsFromServer(data) {
    const m = data.metrics || {};
    const s = data.solver || {};
    document.getElementById('stat-views').textContent = m.totalViews || 0;
    document.getElementById('stat-downloads').textContent = m.totalDownloads || 0;
    const totalTimeSpent = Number(m.totalTimeSpent || 0);
    const hours = Math.floor(totalTimeSpent / 3600);
    const minutes = Math.floor((totalTimeSpent % 3600) / 60);
    document.getElementById('stat-time').textContent = hours > 0 ? `${hours}h ${minutes}m` : `${minutes}m`;

    const setText = (id, val) => { const el = document.getElementById(id); if (el) el.textContent = val; };
    const pct = (num, den) => den > 0 ? Math.round((num / den) * 100) : 0;
    const optLabel = (k) => k === 'compact-area' ? 'Área compacta' : (k === 'bounding-box' ? 'Bounding box' : k);
    const flag = (cc) => cc && cc.length === 2 ? cc.replace(/./g, c => String.fromCodePoint(127397 + c.charCodeAt(0))) : '';

    setText('stat-solver-uses', s.total || 0);
    setText('stat-solver-dxfs', s.dxfs || 0);
    setText('stat-solver-best-time', `${s.avgBest > 0 ? Number(s.avgBest).toFixed(1) : 0}s`);
    setText('stat-solver-save-time', `${s.avgSave > 0 ? Number(s.avgSave).toFixed(1) : 0}s`);
    setText('stat-solver-save-rate', `${pct(s.saved || 0, s.total || 0)}%`);
    setText('stat-solver-save-rate-sub', `${s.saved || 0} de ${s.total || 0} corridas terminó en Guardar`);
    setText('stat-solver-avg-util', s.avgUtil > 0 ? `${Number(s.avgUtil).toFixed(1)}%` : '—');
    setText('stat-solver-avg-sheets', s.avgSheets > 0 ? Number(s.avgSheets).toFixed(1) : '—');
    setText('stat-solver-avg-dxfs', s.total > 0 ? (Number(s.dxfs || 0) / Number(s.total)).toFixed(1) : '—');
    setText('stat-solver-avg-unplaced', s.withResult > 0 ? (Number(s.unplacedSum || 0) / Number(s.withResult)).toFixed(1) : '—');
    setText('stat-solver-fail-rate', s.withResult > 0 ? `${pct(s.withUnplaced || 0, s.withResult || 0)}%` : '—');

    const optEntries = s.optEntries || [];
    if (optEntries.length > 0) {
        setText('stat-solver-top-opt', optLabel(optEntries[0][0]));
        setText('stat-solver-opt-breakdown', optEntries.map(([k, v]) => `${optLabel(k)}: ${v}`).join('  ·  '));
    }
    if (s.topSheet) setText('stat-solver-top-sheet', `${s.topSheet} mm`);

    setText('stat-solver-online', String(s.online || 0));
    setText('stat-solver-desktop', String(s.desktop || 0));
    setText('stat-solver-legacy', String(s.legacy || 0));
    setText('stat-solver-uses-sub', `${s.online || 0} online · ${s.desktop || 0} escritorio${s.legacy ? ` · ${s.legacy} sin origen` : ''}`);

    const countryLabels = (s.countryLabels || []).map((cc) => `${flag(cc)} ${cc}`);
    if ((s.countryLabels || []).length > 0) {
        const top = s.countryLabels[0];
        setText('stat-solver-top-country', `${flag(top)} ${top}`);
        setText('stat-solver-countries-count', `${s.countryCount || s.countryLabels.length} país(es)`);
    } else {
        setText('stat-solver-top-country', '—');
        setText('stat-solver-countries-count', '—');
    }

    renderSolverUsageChart(s.solverDays || [], s.runsSeries || [], s.savesSeries || []);
    renderSolverUtilChart(s.solverDays || [], s.utilSeries || []);
    renderSolverSourceChart(s.online || 0, s.desktop || 0);
    renderSolverCountryChart(countryLabels, s.countryCounts || []);

    const ctx = document.getElementById('metricsChart').getContext('2d');
    if (metricsChartInstance) metricsChartInstance.destroy();
    metricsChartInstance = new Chart(ctx, {
        type: 'line',
        data: {
            labels: m.labels || [],
            datasets: [
                { label: 'Visitas', data: m.viewsData || [], borderColor: '#e85b2e', backgroundColor: 'rgba(232, 91, 46, 0.1)', borderWidth: 2, tension: 0.3, fill: true },
                { label: 'Descargas', data: m.downloadsData || [], borderColor: '#28a745', backgroundColor: 'rgba(40, 167, 69, 0.1)', borderWidth: 2, tension: 0.3, fill: true }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: { beginAtZero: true, grid: { color: '#333' }, ticks: { color: '#a0a8a3' } },
                x: { grid: { color: '#333' }, ticks: { color: '#a0a8a3' } }
            },
            plugins: { legend: { labels: { color: '#f0f0f0' } } }
        }
    });
}

async function loadMetrics() {
    try {
        try {
            const serverMetrics = await adminApi('/api/web-telemetry?adminMetrics=1');
            renderMetricsFromServer(serverMetrics);
            return;
        } catch (serverError) {
            console.warn('Metricas por servidor no disponibles, usando fallback cliente:', serverError);
        }

        const querySnapshot = await getDocs(query(collection(db, 'metrics')));
        
        let totalViews = 0;
        let totalDownloads = 0;
        let totalTimeSpent = 0;
        
        const labels = [];
        const viewsData = [];
        const downloadsData = [];
        const timeData = []; // in minutes
        
        // Sort documents by ID (which is YYYY-MM-DD)
        const docs = [];
        querySnapshot.forEach(doc => {
            if(doc.id !== 'general') docs.push({ id: doc.id, ...doc.data() });
        });
        docs.sort((a, b) => a.id.localeCompare(b.id));

        // Aggregate data
        docs.forEach(data => {
            const v = data.page_views || 0;
            const d = data.downloads || 0;
            const t = data.time_spent || 0;
            
            totalViews += v;
            totalDownloads += d;
            totalTimeSpent += t;
            
            labels.push(data.id);
            viewsData.push(v);
            downloadsData.push(d);
            timeData.push((t / 60).toFixed(1)); // Convert seconds to minutes
        });

        // Update Stat Boxes
        document.getElementById('stat-views').textContent = totalViews;
        document.getElementById('stat-downloads').textContent = totalDownloads;
        
        const hours = Math.floor(totalTimeSpent / 3600);
        const minutes = Math.floor((totalTimeSpent % 3600) / 60);
        document.getElementById('stat-time').textContent = hours > 0 ? `${hours}h ${minutes}m` : `${minutes}m`;

        // Load Solver Metrics
        // ESCALA: antes esto descargaba la coleccion solver_runs COMPLETA en cada
        // visita (lento y caro a medida que crece). Ahora:
        //  - Totales exactos con consultas de agregacion (count/sum/average) que se
        //    resuelven en el servidor sin descargar documentos.
        //  - Una ventana con las ultimas 1000 corridas para los graficos y desgloses
        //    (por dia, pais, tamano de chapa, tipo de optimizacion).
        const runsCol = collection(db, 'solver_runs');

        const recentDocs = [];
        try {
            const recentSnap = await getDocs(query(runsCol, orderBy('timestamp', 'desc'), limit(1000)));
            recentSnap.forEach(d => recentDocs.push(d.data()));
        } catch (e) {
            console.error('No se pudo leer la ventana reciente de solver_runs:', e);
        }

        // Totales globales por agregacion; si algo falla (p.ej. reglas), caemos a la ventana.
        let agg = null;
        try {
            const [cAll, cDesktop, cSaved, sDxf, aBest, aSave, aUtil, aSheets, cWithResult, sUnplaced, cUnplacedPos, cOnline] = await Promise.all([
                getCountFromServer(runsCol),
                getCountFromServer(query(runsCol, where('source', '==', 'desktop'))),
                getCountFromServer(query(runsCol, where('saved', '==', true))),
                getAggregateFromServer(runsCol, { v: sum('dxf_count') }),
                getAggregateFromServer(query(runsCol, where('best_solution_time_sec', '>', 0)), { v: average('best_solution_time_sec') }),
                getAggregateFromServer(query(runsCol, where('total_time_to_save_sec', '>', 0)), { v: average('total_time_to_save_sec') }),
                getAggregateFromServer(query(runsCol, where('final_utilization', '>', 0)), { v: average('final_utilization') }),
                getAggregateFromServer(query(runsCol, where('sheets_used', '>', 0)), { v: average('sheets_used') }),
                getCountFromServer(query(runsCol, where('unplaced_count', '>=', 0))),
                getAggregateFromServer(query(runsCol, where('unplaced_count', '>=', 0)), { v: sum('unplaced_count') }),
                getCountFromServer(query(runsCol, where('unplaced_count', '>', 0))),
                getCountFromServer(query(runsCol, where('source', '==', 'online'))),
            ]);
            agg = {
                total: cAll.data().count,
                desktop: cDesktop.data().count,
                online: cOnline.data().count,
                saved: cSaved.data().count,
                dxfs: sDxf.data().v || 0,
                avgBest: aBest.data().v || 0,
                avgSave: aSave.data().v || 0,
                avgUtil: aUtil.data().v || 0,
                avgSheets: aSheets.data().v || 0,
                withResult: cWithResult.data().count,
                unplacedSum: sUnplaced.data().v || 0,
                withUnplaced: cUnplacedPos.data().count,
            };
        } catch (e) {
            console.error('Agregacion en servidor no disponible, usando la ventana reciente:', e);
        }
        let totalSolverUses = 0;
        let totalDxfs = 0;
        let bestTimeSum = 0;
        let bestTimeCount = 0;
        let saveTimeSum = 0;
        let saveTimeCount = 0;
        let savedCount = 0;
        let utilSum = 0, utilCount = 0;
        let sheetsSum = 0, sheetsCount = 0;
        let unplacedSum = 0, runsWithUnplaced = 0, runsWithResult = 0;
        const optCounts = {};
        const sheetSizeCounts = {};
        const runsByDay = {};
        const savesByDay = {};
        const utilByDay = {}; // day -> { sum, count }
        const bySource = { online: 0, desktop: 0 };
        const byCountry = {};

        recentDocs.forEach(data => {
            totalSolverUses++;
            totalDxfs += (data.dxf_count || 0);
            // Origen: online vs descargable
            const src = (data.source === 'desktop') ? 'desktop' : 'online';
            bySource[src] = (bySource[src] || 0) + 1;
            // Pais (geo)
            const cc = (data.country || '').toUpperCase();
            if (cc) byCountry[cc] = (byCountry[cc] || 0) + 1;
            const day = data.date || (data.timestamp && data.timestamp.toDate ? data.timestamp.toDate().toISOString().split('T')[0] : null);
            if (day) runsByDay[day] = (runsByDay[day] || 0) + 1;

            if (data.best_solution_time_sec > 0) {
                bestTimeSum += data.best_solution_time_sec;
                bestTimeCount++;
            }
            if (data.saved) {
                savedCount++;
                if (day) savesByDay[day] = (savesByDay[day] || 0) + 1;
                if (data.total_time_to_save_sec > 0) {
                    saveTimeSum += data.total_time_to_save_sec;
                    saveTimeCount++;
                }
            }
            if (typeof data.final_utilization === 'number' && data.final_utilization > 0) {
                utilSum += data.final_utilization;
                utilCount++;
                if (day) {
                    if (!utilByDay[day]) utilByDay[day] = { sum: 0, count: 0 };
                    utilByDay[day].sum += data.final_utilization;
                    utilByDay[day].count++;
                }
            }
            if (typeof data.sheets_used === 'number' && data.sheets_used > 0) {
                sheetsSum += data.sheets_used;
                sheetsCount++;
            }
            // Piezas sin ubicar: solo contamos corridas que dejaron un resultado
            if (data.placed_count !== undefined || data.unplaced_count !== undefined) {
                runsWithResult++;
                const up = Number(data.unplaced_count || 0);
                unplacedSum += up;
                if (up > 0) runsWithUnplaced++;
            }
            if (data.optimization_type) {
                optCounts[data.optimization_type] = (optCounts[data.optimization_type] || 0) + 1;
            }
            if (data.sheet_width && data.sheet_height) {
                const key = `${Math.round(data.sheet_width)}×${Math.round(data.sheet_height)}`;
                sheetSizeCounts[key] = (sheetSizeCounts[key] || 0) + 1;
            }
        });

        // Totales finales: agregacion del servidor si esta disponible, si no la ventana.
        const T = {
            total: agg ? agg.total : totalSolverUses,
            dxfs: agg ? agg.dxfs : totalDxfs,
            saved: agg ? agg.saved : savedCount,
            avgBest: agg ? agg.avgBest : (bestTimeCount > 0 ? bestTimeSum / bestTimeCount : 0),
            avgSave: agg ? agg.avgSave : (saveTimeCount > 0 ? saveTimeSum / saveTimeCount : 0),
            avgUtil: agg ? agg.avgUtil : (utilCount > 0 ? utilSum / utilCount : 0),
            avgSheets: agg ? agg.avgSheets : (sheetsCount > 0 ? sheetsSum / sheetsCount : 0),
            withResult: agg ? agg.withResult : runsWithResult,
            unplacedSum: agg ? agg.unplacedSum : unplacedSum,
            withUnplaced: agg ? agg.withUnplaced : runsWithUnplaced,
        };

        document.getElementById('stat-solver-uses').textContent = T.total;
        document.getElementById('stat-solver-dxfs').textContent = T.dxfs;

        document.getElementById('stat-solver-best-time').textContent = `${T.avgBest > 0 ? T.avgBest.toFixed(1) : 0}s`;
        document.getElementById('stat-solver-save-time').textContent = `${T.avgSave > 0 ? T.avgSave.toFixed(1) : 0}s`;

        // --- Metricas diferenciadas nuevas ---
        const setText = (id, val) => { const el = document.getElementById(id); if (el) el.textContent = val; };
        const pct = (num, den) => den > 0 ? Math.round((num / den) * 100) : 0;

        setText('stat-solver-save-rate', `${pct(T.saved, T.total)}%`);
        setText('stat-solver-save-rate-sub', `${T.saved} de ${T.total} corridas terminó en Guardar`);
        setText('stat-solver-avg-util', T.avgUtil > 0 ? `${T.avgUtil.toFixed(1)}%` : '—');
        setText('stat-solver-avg-sheets', T.avgSheets > 0 ? T.avgSheets.toFixed(1) : '—');
        setText('stat-solver-avg-dxfs', T.total > 0 ? (T.dxfs / T.total).toFixed(1) : '—');
        setText('stat-solver-avg-unplaced', T.withResult > 0 ? (T.unplacedSum / T.withResult).toFixed(1) : '—');
        setText('stat-solver-fail-rate', T.withResult > 0 ? `${pct(T.withUnplaced, T.withResult)}%` : '—');

        // Optimizacion mas usada + desglose
        const optEntries = Object.entries(optCounts).sort((a, b) => b[1] - a[1]);
        const optLabel = (k) => k === 'compact-area' ? 'Área compacta' : (k === 'bounding-box' ? 'Bounding box' : k);
        if (optEntries.length > 0) {
            setText('stat-solver-top-opt', optLabel(optEntries[0][0]));
            setText('stat-solver-opt-breakdown', optEntries.map(([k, v]) => `${optLabel(k)}: ${v}`).join('  ·  '));
        }
        const sheetEntries = Object.entries(sheetSizeCounts).sort((a, b) => b[1] - a[1]);
        if (sheetEntries.length > 0) {
            setText('stat-solver-top-sheet', `${sheetEntries[0][0]} mm`);
        }

        // --- Graficos por dia (ordenados) ---
        const solverDays = Array.from(new Set([...Object.keys(runsByDay), ...Object.keys(utilByDay)])).sort();
        const runsSeries = solverDays.map(d => runsByDay[d] || 0);
        const savesSeries = solverDays.map(d => savesByDay[d] || 0);
        const utilSeries = solverDays.map(d => utilByDay[d] ? +(utilByDay[d].sum / utilByDay[d].count).toFixed(1) : null);
        renderSolverUsageChart(solverDays, runsSeries, savesSeries);
        renderSolverUtilChart(solverDays, utilSeries);

        // --- Origen del uso (online vs descargable vs viejos sin etiqueta) ---
        // OJO: la tarjeta grande "Usos del Solver" es el TOTAL (online+escritorio).
        // Aca desglosamos para no confundir: el online real puede ser bajo aunque el
        // total sea alto (la mayoria de las corridas suelen ser del descargable).
        const desktopN = agg ? agg.desktop : (bySource.desktop || 0);
        const onlineN = agg ? (agg.online || 0) : (bySource.online || 0);
        const legacyN = agg ? Math.max(0, agg.total - agg.desktop - (agg.online || 0)) : 0;
        setText('stat-solver-online', String(onlineN));
        setText('stat-solver-desktop', String(desktopN));
        setText('stat-solver-legacy', String(legacyN));
        setText('stat-solver-uses-sub',
            `${onlineN} online · ${desktopN} escritorio${legacyN ? ` · ${legacyN} sin origen` : ''}`);
        renderSolverSourceChart(onlineN, desktopN);

        // --- Países (de dónde usan el solver) ---
        const countryEntries = Object.entries(byCountry).sort((a, b) => b[1] - a[1]);
        const flag = (cc) => cc.length === 2 ? cc.replace(/./g, c => String.fromCodePoint(127397 + c.charCodeAt(0))) : '';
        if (countryEntries.length > 0) {
            setText('stat-solver-top-country', `${flag(countryEntries[0][0])} ${countryEntries[0][0]}`);
            setText('stat-solver-countries-count', `${countryEntries.length} país(es)`);
        } else {
            setText('stat-solver-top-country', '—');
        }
        const topCountries = countryEntries.slice(0, 10);
        renderSolverCountryChart(topCountries.map(([k]) => `${flag(k)} ${k}`), topCountries.map(([, v]) => v));

        // Render Chart
        const ctx = document.getElementById('metricsChart').getContext('2d');
        if(metricsChartInstance) metricsChartInstance.destroy(); // clear previous
        
        metricsChartInstance = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [
                    {
                        label: 'Visitas',
                        data: viewsData,
                        borderColor: '#e85b2e',
                        backgroundColor: 'rgba(232, 91, 46, 0.1)',
                        borderWidth: 2,
                        tension: 0.3,
                        fill: true
                    },
                    {
                        label: 'Descargas',
                        data: downloadsData,
                        borderColor: '#28a745',
                        backgroundColor: 'rgba(40, 167, 69, 0.1)',
                        borderWidth: 2,
                        tension: 0.3,
                        fill: true
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        grid: { color: '#333' },
                        ticks: { color: '#a0a8a3' }
                    },
                    x: {
                        grid: { color: '#333' },
                        ticks: { color: '#a0a8a3' }
                    }
                },
                plugins: {
                    legend: { labels: { color: '#f0f0f0' } }
                }
            }
        });

    } catch (e) {
        console.error("Error loading metrics:", e);
    }
}

// 5b. Load Messages
async function loadMessages() {
    const container = document.getElementById('messages-container');
    try {
        const data = await adminApi('/api/web-telemetry?adminMessages=1');
        const messages = data.items || [];
        
        if (messages.length === 0) {
            container.innerHTML = `<p class="loading">${adminT('messages.empty')}</p>`;
            return;
        }
        
        let html = '';
        messages.forEach((msg) => {
            const dateLocale = getAdminLang() === 'en' ? 'en-US' : 'es-AR';
            const date = msg.timestampISO ? new Date(msg.timestampISO).toLocaleString(dateLocale) : adminT('messages.unknownDate');
            html += `
                <div class="message-card" data-id="${msg.id}">
                    <div class="message-header">
                        <div>
                            <span class="message-name">${msg.name || adminT('messages.noName')} (${msg.email || adminT('messages.noEmail')})</span>
                            <span class="message-date">${date}</span>
                        </div>
                        <button class="btn-danger delete-message-btn" data-id="${msg.id}" style="padding: 6px 12px; font-size: 13px;">${adminT('messages.delete')}</button>
                    </div>
                    <div class="message-body">${msg.message || ''}</div>
                </div>
            `;
        });
        container.innerHTML = html;

        document.querySelectorAll('.delete-message-btn').forEach(btn => {
            btn.addEventListener('click', async (e) => {
                const messageId = e.target.dataset.id;
                if (!messageId) return;
                if (!confirm(adminT('messages.deleteConfirm'))) return;

                try {
                    e.target.disabled = true;
                    e.target.textContent = adminT('messages.deleting');
                    await adminApi('/api/web-telemetry', {
                        method: 'POST',
                        body: JSON.stringify({ adminOp: 'deleteMessage', id: messageId })
                    });
                    logAudit('Borrar mensaje', `Borró un mensaje de contacto (id ${messageId}).`);
                    loadMessages();
                } catch (error) {
                    console.error('Error deleting message:', error);
                    alert(adminT('messages.deleteError'));
                    e.target.disabled = false;
                    e.target.textContent = adminT('messages.delete');
                }
            });
        });
    } catch (e) {
        console.error("Error loading messages:", e);
        container.innerHTML = `<p class="error-msg">${adminT('messages.loadError')}</p>`;
    }
}

async function deleteOldMessages(days) {
    const result = await adminApi('/api/web-telemetry', {
        method: 'POST',
        body: JSON.stringify({ adminOp: 'deleteOldMessages', days })
    });
    const deleted = Number(result.deleted || 0);
    if (deleted > 0) {
        logAudit('Borrar mensajes', `BorrÃ³ ${deleted} mensaje(s) con mÃ¡s de ${days} dÃ­as.`);
    }
    return deleted;

    const cutoff = Date.now() - (days * 24 * 60 * 60 * 1000);
    const snapshot = await getDocs(collection(db, 'messages'));
    const docsToDelete = [];

    snapshot.forEach((docSnap) => {
        const data = docSnap.data();
        if (!data.timestamp || !data.timestamp.toDate) return;
        if (data.timestamp.toDate().getTime() < cutoff) docsToDelete.push(docSnap.ref);
    });

    for (let i = 0; i < docsToDelete.length; i += 450) {
        const batch = writeBatch(db);
        docsToDelete.slice(i, i + 450).forEach((ref) => batch.delete(ref));
        await batch.commit();
    }

    if (docsToDelete.length > 0) {
        logAudit('Borrar mensajes', `Borró ${docsToDelete.length} mensaje(s) con más de ${days} días.`);
    }
    return docsToDelete.length;
}

if (deleteOldMessagesBtn) {
    deleteOldMessagesBtn.addEventListener('click', async () => {
        const rawDays = prompt(adminT('messages.deleteOldPrompt'), '30');
        if (rawDays === null) return;

        const days = Number.parseInt(rawDays, 10);
        if (!Number.isFinite(days) || days < 1) {
            alert(adminT('messages.deleteOldInvalid'));
            return;
        }

        if (!confirm(adminT('messages.deleteOldConfirm', { days }))) return;

        const previousText = deleteOldMessagesBtn.textContent;
        deleteOldMessagesBtn.disabled = true;
        deleteOldMessagesBtn.textContent = adminT('messages.cleaning');

        try {
            const deletedCount = await deleteOldMessages(days);
            alert(adminT('messages.deleteOldDone', { count: deletedCount }));
            loadMessages();
        } catch (error) {
            console.error('Error deleting old messages:', error);
            alert(adminT('messages.deleteOldError'));
        } finally {
            deleteOldMessagesBtn.disabled = false;
            deleteOldMessagesBtn.textContent = previousText;
        }
    });
}

// 5c. Load Users
async function loadUsers() {
    const container = document.getElementById('users-container');
    try {
        const data = await adminApi('/api/web-telemetry?adminUsers=1');
        const users = data.items || [];
        
        if (users.length === 0) {
            container.innerHTML = '<tr><td colspan="7" class="loading">No hay usuarios registrados.</td></tr>';
            return;
        }
        
        container.innerHTML = '';
        users.forEach((user) => {
            const userId = user.id || user.uid;
            const date = user.createdAtISO ? new Date(user.createdAtISO).toLocaleDateString('es-AR') : 'N/A';
            const lastLogin = user.lastLoginISO ? new Date(user.lastLoginISO).toLocaleString('es-AR') : 'Nunca';
            const name = user.name ? user.name : '<span style="color:#777;font-style:italic;">Sin nombre</span>';
            const role = user.role === 'admin' || user.isAdmin === true ? 'admin' : 'user';
            
            let statusClass = 'status-revoked';
            let statusText = 'SIN LICENCIA';
            let hasLicense = false;

            // Check if Sheet Metal Nesting is active in the new map or fallback to hasActiveLicense
            if (user.licenses && user.licenses['Sheet Metal Nesting'] && user.licenses['Sheet Metal Nesting'].status === 'active') {
                statusClass = 'status-active';
                statusText = 'LICENCIA ACTIVA';
                hasLicense = true;
            } else if (user.hasActiveLicense) {
                statusClass = 'status-active';
                statusText = 'LICENCIA ACTIVA';
                hasLicense = true;
            }
            
            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td>${name}</td>
                <td class="mono">${user.email || userId}</td>
                <td>${date}</td>
                <td>${lastLogin}</td>
                <td>
                    <select class="role-select" data-id="${userId}" ${userId === currentAdminUid && role === 'admin' ? 'title="Cuidado: esta es tu cuenta actual"' : ''}>
                        <option value="user" ${role === 'user' ? 'selected' : ''}>Usuario</option>
                        <option value="admin" ${role === 'admin' ? 'selected' : ''}>Administrador</option>
                    </select>
                </td>
                <td><span class="status-badge ${statusClass}">${statusText}</span></td>
                <td>
                    <div style="display:flex; gap:8px; flex-wrap:wrap;">
                        ${hasLicense 
                            ? `<button class="btn-danger toggle-license-btn" data-id="${userId}" data-action="revoke" style="padding: 6px 12px; font-size: 13px;">Revocar</button>` 
                            : `<button class="btn-primary toggle-license-btn" data-id="${userId}" data-action="grant" style="padding: 6px 12px; font-size: 13px;">Otorgar</button>`}
                        <button class="btn-primary reset-pwd-btn" data-email="${user.email}" style="padding: 6px 12px; font-size: 13px; background: #444; border: 1px solid #555;">Reset Clave</button>
                        <button class="btn-danger delete-user-btn" data-id="${userId}" data-email="${user.email || ''}" ${userId === currentAdminUid ? 'disabled title="No podes borrar tu propia cuenta desde esta sesion"' : ''} style="padding: 6px 12px; font-size: 13px;">Borrar</button>
                    </div>
                </td>
            `;
            container.appendChild(tr);
        });

        document.querySelectorAll('.role-select').forEach(select => {
            select.addEventListener('change', async (e) => {
                const userId = e.target.dataset.id;
                const nextRole = e.target.value;
                const previousRole = nextRole === 'admin' ? 'user' : 'admin';
                const label = nextRole === 'admin' ? 'administrador' : 'usuario';

                if (!confirm(`Â¿Cambiar rol a ${label}?`)) {
                    e.target.value = previousRole;
                    return;
                }

                try {
                    await adminApi('/api/web-telemetry', {
                        method: 'POST',
                        body: JSON.stringify({ adminOp: 'updateUserRole', uid: userId, role: nextRole })
                    });
                    logAudit('Cambiar rol', `Cambió el rol del usuario ${userId} a ${label}.`);
                    loadUsers();
                } catch (error) {
                    console.error('Error updating role:', error);
                    alert('No se pudo cambiar el rol del usuario.');
                    e.target.value = previousRole;
                }
            });
        });
        
        // Add toggle listeners
        document.querySelectorAll('.toggle-license-btn').forEach(btn => {
            btn.addEventListener('click', async (e) => {
                const action = e.target.dataset.action;
                if(confirm(action === 'revoke' ? '¿Seguro que quieres revocar esta licencia?' : '¿Otorgar licencia a este usuario?')) {
                    const status = action === 'grant' ? 'active' : 'inactive';
                    try {
                        await adminApi('/api/web-telemetry', {
                            method: 'POST',
                            body: JSON.stringify({ adminOp: 'setUserLicense', uid: e.target.dataset.id, status })
                        });
                        logAudit(action === 'grant' ? 'Otorgar licencia' : 'Revocar licencia',
                            `${action === 'grant' ? 'Otorgó' : 'Revocó'} la licencia de Sheet Metal Nesting al usuario ${e.target.dataset.id}.`);
                        loadUsers();
                    } catch (error) {
                        console.error('Error updating license:', error);
                        alert('No se pudo actualizar la licencia del usuario.');
                    }
                }
            });
        });

        // Add reset password listeners
        document.querySelectorAll('.reset-pwd-btn').forEach(btn => {
            btn.addEventListener('click', async (e) => {
                const email = e.target.dataset.email;
                if (!email) return;
                if(confirm(`¿Enviar email oficial de Foglesting para restablecer la contraseña a: ${email}?`)) {
                    try {
                        await sendPasswordResetEmail(auth, email);
                        logAudit('Reset de clave', `Envió email de restablecimiento de contraseña a ${email}.`);
                        alert(`Mail de restablecimiento enviado exitosamente a ${email}`);
                    } catch (error) {
                        console.error(error);
                        alert(`Error al enviar el mail: ${error.message}`);
                    }
                }
            });
        });

        // Add delete user listeners
        document.querySelectorAll('.delete-user-btn').forEach(btn => {
            btn.addEventListener('click', async (e) => {
                const userId = e.target.dataset.id;
                const email = e.target.dataset.email || userId;
                if (!userId || userId === currentAdminUid) return;

                const confirmed = confirm(
                    `Borrar el usuario ${email} del panel?\n\n` +
                    'Esto elimina su perfil, rol y licencia del panel de Foglesting. ' +
                    'La cuenta de login de Firebase Auth puede seguir existiendo hasta borrarla desde Firebase.'
                );
                if (!confirmed) return;

                try {
                    e.target.disabled = true;
                    e.target.textContent = 'Borrando...';
                    const currentUser = auth.currentUser;
                    if (!currentUser) throw new Error('Sesion admin no disponible.');
                    const idToken = await currentUser.getIdToken(true);
                    const response = await fetch('/api/delete-user', {
                        method: 'POST',
                        headers: {
                            'Content-Type': 'application/json',
                            'Authorization': `Bearer ${idToken}`
                        },
                        body: JSON.stringify({ uid: userId })
                    });
                    const payload = await response.json().catch(() => ({}));
                    if (!response.ok) {
                        throw new Error(payload.error || 'No se pudo borrar el usuario.');
                    }
                    logAudit('Borrar usuario', `Borró el usuario ${email} del panel.`);
                    loadUsers();
                } catch (error) {
                    console.error('Error deleting user:', error);
                    alert(error.message || 'No se pudo borrar el usuario.');
                    e.target.disabled = false;
                    e.target.textContent = 'Borrar';
                }
            });
        });
        
    } catch (e) {
        console.error("Error loading users:", e);
        container.innerHTML = '<tr><td colspan="7" class="error-msg">Error al cargar usuarios.</td></tr>';
    }
}

// 5d. Load Global Settings
async function loadGlobalSettings() {
    const toggle = document.getElementById('global-license-toggle');
    if (!toggle) return;
    try {
        const data = await adminApi('/api/web-telemetry?adminSettings=1');
        toggle.checked = data.requireLicense === true;
    } catch(e) {
        console.error("Error loading settings", e);
    }

    if (toggle.dataset.bound === '1') return;
    toggle.dataset.bound = '1';
    toggle.addEventListener('change', async (e) => {
        try {
            await adminApi('/api/web-telemetry', {
                method: 'POST',
                body: JSON.stringify({ adminOp: 'setAppConfig', requireLicense: e.target.checked })
            });
            logAudit('Configurar licencia global', `Modo pago ${e.target.checked ? 'activado' : 'desactivado'}.`);
        } catch(err) {
            console.error("Error updating settings", err);
            alert("Error al actualizar la configuracion");
            e.target.checked = !e.target.checked;
        }
    });
}
// 5e. Load desktop release candidates
async function loadReleaseCandidates() {
    const container = document.getElementById('release-candidates');
    const currentPublic = document.getElementById('release-current-public');
    const select = document.getElementById('release-version-select');
    const publishSelected = document.getElementById('release-selected-btn');
    if (!container) return;

    try {
        const response = await fetch('/admin-releases/releases.json', { cache: 'no-store' });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const data = await response.json();
        const candidates = Array.isArray(data.candidates) ? data.candidates : [];
        const activeVersion = data.active_public_version || '';
        candidates.sort((a, b) => String(b.version || '').localeCompare(String(a.version || ''), undefined, { numeric: true }));

        if (currentPublic) {
            const active = candidates.find(item => item.version === activeVersion);
            currentPublic.textContent = active
                ? `${active.name} (${active.file})`
                : activeVersion
                    ? `FOGLESTING V${activeVersion}`
                    : 'No informado';
        }

        if (select) {
            select.innerHTML = candidates.map((release) => `
                <option value="${release.version}" ${release.version === activeVersion ? 'selected' : ''}>
                    ${release.name || `FOGLESTING ${release.version}`} ${release.version === activeVersion ? ' - publica actual' : ''}
                </option>
            `).join('');
        }

        if (publishSelected) {
            publishSelected.onclick = () => {
                const release = candidates.find(item => item.version === select.value);
                if (release) promoteRelease(release, publishSelected);
            };
            publishSelected.textContent = activeVersion
                ? 'Publicar versión elegida'
                : 'No informado';
        }

        if (candidates.length === 0) {
            container.innerHTML = '<p class="loading">Todavia no hay versiones para probar.</p>';
            return;
        }

        container.innerHTML = '';
        candidates.forEach((release) => {
            const card = document.createElement('article');
            card.className = 'release-card';
            if (release.version === activeVersion) card.classList.add('release-card-active');
            const sizeMb = release.size_bytes ? (release.size_bytes / (1024 * 1024)).toFixed(2) : 'N/A';
            const downloadUrl = release.download_url || `/admin-releases/${release.file}`;
            const isActive = release.version === activeVersion;
            const isPublicListed = release.public_listed === true;
            const badgeText = isActive ? 'Principal actual' : isPublicListed ? 'En historial publico' : 'Solo guardada';
            card.innerHTML = `
                <div class="release-card-header">
                    <div>
                        <h2>${release.name || `FOGLESTING ${release.version}`}</h2>
                        <p>Version ${release.version || 'sin numero'} · ${sizeMb} MB · ${release.created_at || 'sin fecha'}</p>
                    </div>
                    <span class="release-badge">${badgeText}</span>
                </div>
                <p class="release-notes">${release.notes || 'Sin notas.'}</p>
                <div class="release-hash mono">${release.sha256 || ''}</div>
                <div class="release-actions">
                    <a class="btn-primary release-download" href="${downloadUrl}" download>Descargar archivo</a>
                    <button class="btn-secondary promote-release-btn" data-version="${release.version}" ${isActive ? 'disabled' : ''}>
                        ${isActive ? 'Ya es principal' : 'Hacer principal'}
                    </button>
                    <button class="btn-secondary toggle-release-visibility-btn" data-version="${release.version}" data-listed="${isPublicListed ? '1' : '0'}">
                        ${isPublicListed ? 'Quitar del historial publico' : 'Mostrar en historial publico'}
                    </button>
                </div>
            `;
            container.appendChild(card);
        });

        document.querySelectorAll('.promote-release-btn').forEach((button) => {
            button.addEventListener('click', () => {
                const release = candidates.find(item => item.version === button.dataset.version);
                if (release) promoteRelease(release, button);
            });
        });
        document.querySelectorAll('.toggle-release-visibility-btn').forEach((button) => {
            button.addEventListener('click', () => {
                const release = candidates.find(item => item.version === button.dataset.version);
                if (release) toggleReleaseVisibility(release, button);
            });
        });
    } catch (error) {
        console.error('Error loading release candidates:', error);
        container.innerHTML = '<p class="error-msg">No se pudieron cargar las versiones del admin.</p>';
    }
}

async function toggleReleaseVisibility(release, button) {
    const nextListed = release.public_listed !== true;
    const action = nextListed ? 'mostrar en el historial publico' : 'quitar del historial publico';
    if (!confirm(`¿Querés ${action} ${release.name}?`)) return;

    let releaseSecret = sessionStorage.getItem('foglesting_release_secret') || '';
    if (!releaseSecret) {
        releaseSecret = prompt('Clave privada de release:') || '';
        if (!releaseSecret) return;
        sessionStorage.setItem('foglesting_release_secret', releaseSecret);
    }

    const previousText = button.textContent;
    button.disabled = true;
    button.textContent = 'Guardando...';

    try {
        const response = await fetch('/api/update-release-visibility', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'x-release-secret': releaseSecret
            },
            body: JSON.stringify({
                version: release.version,
                publicListed: nextListed
            })
        });
        const result = await response.json().catch(() => ({}));
        if (!response.ok) throw new Error(result.error || `HTTP ${response.status}`);
        logAudit('Visibilidad de versión', `${nextListed ? 'Mostró' : 'Quitó'} la versión ${release.version} ${nextListed ? 'en' : 'del'} historial público.`);
        alert('Visibilidad actualizada. Puede tardar unos minutos en reflejarse.');
        loadReleaseCandidates();
    } catch (error) {
        alert(`No se pudo actualizar la visibilidad.\n\nDetalle: ${error.message}`);
    } finally {
        button.disabled = false;
        button.textContent = previousText;
    }
}

async function promoteRelease(release, button) {
    const version = release.version;
    const file = release.file;
    const name = release.name || `FOGLESTING V${version}`;
    const sourcePath = release.source_path || release.download_url || `/admin-releases/${file}`;
    const publicPath = sourcePath.startsWith('/downloads/') ? sourcePath : `/downloads/${file}`;

    const ok = confirm(
        `Esto publicaria ${name} como descarga principal.\n\n` +
        `No copies archivos a mano si todavia no lo probaste en otra PC.\n\n` +
        `¿Querés continuar?`
    );
    if (!ok) return;

    let releaseSecret = sessionStorage.getItem('foglesting_release_secret') || '';
    if (!releaseSecret) {
        releaseSecret = prompt('Clave privada de release:') || '';
        if (!releaseSecret) return;
        sessionStorage.setItem('foglesting_release_secret', releaseSecret);
    }

    const previousText = button.textContent;
    button.disabled = true;
    button.textContent = 'Publicando...';

    try {
        const response = await fetch('/api/promote-release', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'x-release-secret': releaseSecret
            },
            body: JSON.stringify({
                version,
                name,
                sourceFile: file,
                sourcePath,
                publicFile: file,
                downloadUrl: publicPath,
                notes: `${name} disponible. Descarga directa del ejecutable.`
            })
        });
        const result = await response.json().catch(() => ({}));
        if (!response.ok) {
            throw new Error(result.error || `HTTP ${response.status}`);
        }
        logAudit('Publicar versión', `Hizo principal (pública) la versión ${version} (${file}).`);
        alert('Release publicado. Vercel puede tardar unos minutos en reflejar el cambio.');
        loadReleaseCandidates();
    } catch (error) {
        alert(
            'No se pudo publicar automaticamente.\n\n' +
            'El .exe para probar ya esta disponible en admin, pero para activar el boton de release falta configurar las variables seguras del servidor.\n\n' +
            `Detalle: ${error.message}`
        );
    } finally {
        button.disabled = false;
        button.textContent = previousText;
    }
}
