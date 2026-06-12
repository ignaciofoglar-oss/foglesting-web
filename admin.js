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
//  AUDITORÍA — registro de acciones del panel
// ============================================================
async function logAudit(action, description, details = null) {
    try {
        await addDoc(collection(db, 'audit_log'), {
            ts: serverTimestamp(),
            dateISO: new Date().toISOString(),
            action,
            description,
            admin: currentAdminEmail || '(desconocido)',
            details: details || null,
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
    }
    try {
        const base = collection(db, 'audit_log');
        const q = (append && auditLastDoc)
            ? query(base, orderBy('ts', 'desc'), startAfter(auditLastDoc), limit(AUDIT_PAGE))
            : query(base, orderBy('ts', 'desc'), limit(AUDIT_PAGE));
        const snap = await getDocs(q);
        const rows = [];
        snap.forEach((d) => rows.push({ id: d.id, ...d.data() }));
        if (snap.docs.length > 0) auditLastDoc = snap.docs[snap.docs.length - 1];
        const hasMore = rows.length === AUDIT_PAGE;

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
        const s = { id: 'legacy_' + tmpSessions.length, machine, start: t, end: t, dxfs: [], runs: [], country: '' };
        tmpSessions.push(s);
        return s;
    }

    const sidSessions = new Map();
    function placeSid(sid, machine, t) {
        let s = sidSessions.get(sid);
        if (!s) { s = { id: sid, machine, start: t, end: t, dxfs: [], runs: [], country: '' }; sidSessions.set(sid, s); }
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

    const all = [...sidSessions.values(), ...tmpSessions];
    // Ordenar todo por hora dentro de cada sesion y las sesiones por inicio desc.
    all.forEach((s) => {
        s.dxfs.sort((a, b) => String(a.createdAt).localeCompare(String(b.createdAt)));
        s.runs.sort((a, b) => runTime(a) - runTime(b));
    });
    all.sort((a, b) => b.start - a.start);
    return all;
}

// Modal con el informe de lo que hizo el usuario en una sesion (timeline + resumen).
function showSessionReport(s) {
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
                    <div style="font-weight:700;font-size:15px;">Informe de sesión — ${flag} ${escapeHtmlDiag(s.machine)}</div>
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
                <div class="release-muted" style="font-size:12px;margin-bottom:8px;">LÍNEA DE TIEMPO</div>
                ${timeline || '<span class="release-muted">Sin eventos.</span>'}
            </div>
        </div>`;
    document.body.appendChild(overlay);
    const close = () => overlay.remove();
    overlay.addEventListener('click', (e) => { if (e.target === overlay) close(); });
    const closeBtn = document.getElementById('session-report-close');
    if (closeBtn) closeBtn.addEventListener('click', close);
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

        const btnStyle = 'padding:6px 12px;font-size:13px;width:auto;display:inline-block;';
        let groupsHtml = '';
        sessions.forEach((s, si) => {
            let rows = '';
            for (const it of s.dxfs) {
                const m = it.meta || {};
                const kb = (it.sizeBytes / 1024).toFixed(1);
                const bbox = (m.bboxW && m.bboxH) ? `${Math.round(m.bboxW)} × ${Math.round(m.bboxH)} mm` : '—';
                const date = it.createdAt ? new Date(it.createdAt).toLocaleString('es-AR') : '—';
                const dl = it.truncated
                    ? '<span class="release-muted">muy grande</span>'
                    : `<button class="btn-primary diag-dl" data-id="${escapeHtmlDiag(it.id)}" data-name="${escapeHtmlDiag(it.filename)}" style="${btnStyle}">Descargar</button>`;
                const del = `<button class="btn-danger diag-del" data-id="${escapeHtmlDiag(it.id)}" data-name="${escapeHtmlDiag(it.filename)}" style="${btnStyle}margin-left:6px;">Borrar</button>`;
                rows += `
                    <tr>
                        <td style="text-align:center;"><input type="checkbox" class="diag-check" data-id="${escapeHtmlDiag(it.id)}" data-name="${escapeHtmlDiag(it.filename)}" data-truncated="${it.truncated ? '1' : '0'}"></td>
                        <td>${escapeHtmlDiag(it.filename)}</td>
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
                        <span class="audit-tag">${flag} ${escapeHtmlDiag(s.machine)}</span>
                        <span class="release-muted">${escapeHtmlDiag(started)}</span>
                        <span class="release-muted">· ${s.dxfs.length} DXF · ${s.runs.length} corrida(s) · ${durTxt}</span>
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

        container.innerHTML = `
            <div style="display:flex;align-items:center;gap:12px;margin-bottom:14px;flex-wrap:wrap;">
                <button class="btn-primary" id="diag-download-selected" type="button" style="width:auto;" disabled>
                    Descargar seleccionados
                </button>
                <button class="btn-danger" id="diag-delete-selected" type="button" style="width:auto;" disabled>
                    Borrar seleccionados (<span id="diag-selected-count">0</span>)
                </button>
                <span class="release-muted" id="diag-total-count">${items.length} archivo(s) · ${sessions.length} sesión(es)</span>
            </div>
            ${groupsHtml}`;

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
        const resp = await fetch(`/api/get-diagnostic?id=${encodeURIComponent(id)}`, {
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

async function loadMetrics() {
    try {
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
            const [cAll, cDesktop, cSaved, sDxf, aBest, aSave, aUtil, aSheets, cWithResult, sUnplaced, cUnplacedPos] = await Promise.all([
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
            ]);
            agg = {
                total: cAll.data().count,
                desktop: cDesktop.data().count,
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

        // --- Origen del uso (online vs descargable) ---
        const desktopN = agg ? agg.desktop : (bySource.desktop || 0);
        const onlineN = agg ? Math.max(0, agg.total - agg.desktop) : (bySource.online || 0);
        setText('stat-solver-online', String(onlineN));
        setText('stat-solver-desktop', String(desktopN));
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
        const q = query(collection(db, 'messages'), orderBy('timestamp', 'desc'));
        const querySnapshot = await getDocs(q);
        
        if (querySnapshot.empty) {
            container.innerHTML = `<p class="loading">${adminT('messages.empty')}</p>`;
            return;
        }
        
        let html = '';
        querySnapshot.forEach((docSnap) => {
            const msg = docSnap.data();
            const dateLocale = getAdminLang() === 'en' ? 'en-US' : 'es-AR';
            const date = msg.timestamp ? msg.timestamp.toDate().toLocaleString(dateLocale) : adminT('messages.unknownDate');
            html += `
                <div class="message-card" data-id="${docSnap.id}">
                    <div class="message-header">
                        <div>
                            <span class="message-name">${msg.name || adminT('messages.noName')} (${msg.email || adminT('messages.noEmail')})</span>
                            <span class="message-date">${date}</span>
                        </div>
                        <button class="btn-danger delete-message-btn" data-id="${docSnap.id}" style="padding: 6px 12px; font-size: 13px;">${adminT('messages.delete')}</button>
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
                    await deleteDoc(doc(db, 'messages', messageId));
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
        // Assume users collection exists
        const q = query(collection(db, 'users'), orderBy('createdAt', 'desc'));
        const querySnapshot = await getDocs(q);
        
        if (querySnapshot.empty) {
            container.innerHTML = '<tr><td colspan="7" class="loading">No hay usuarios registrados.</td></tr>';
            return;
        }
        
        container.innerHTML = '';
        querySnapshot.forEach((docSnap) => {
            const user = docSnap.data();
            const date = user.createdAt ? (user.createdAt.toDate ? user.createdAt.toDate().toLocaleDateString('es-AR') : 'N/A') : 'N/A';
            const lastLogin = user.lastLogin ? (user.lastLogin.toDate ? user.lastLogin.toDate().toLocaleString('es-AR') : 'N/A') : 'Nunca';
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
                <td class="mono">${user.email || docSnap.id}</td>
                <td>${date}</td>
                <td>${lastLogin}</td>
                <td>
                    <select class="role-select" data-id="${docSnap.id}" ${docSnap.id === currentAdminUid && role === 'admin' ? 'title="Cuidado: esta es tu cuenta actual"' : ''}>
                        <option value="user" ${role === 'user' ? 'selected' : ''}>Usuario</option>
                        <option value="admin" ${role === 'admin' ? 'selected' : ''}>Administrador</option>
                    </select>
                </td>
                <td><span class="status-badge ${statusClass}">${statusText}</span></td>
                <td>
                    <div style="display:flex; gap:8px; flex-wrap:wrap;">
                        ${hasLicense 
                            ? `<button class="btn-danger toggle-license-btn" data-id="${docSnap.id}" data-action="revoke" style="padding: 6px 12px; font-size: 13px;">Revocar</button>` 
                            : `<button class="btn-primary toggle-license-btn" data-id="${docSnap.id}" data-action="grant" style="padding: 6px 12px; font-size: 13px;">Otorgar</button>`}
                        <button class="btn-primary reset-pwd-btn" data-email="${user.email}" style="padding: 6px 12px; font-size: 13px; background: #444; border: 1px solid #555;">Reset Clave</button>
                        <button class="btn-danger delete-user-btn" data-id="${docSnap.id}" data-email="${user.email || ''}" ${docSnap.id === currentAdminUid ? 'disabled title="No podes borrar tu propia cuenta desde esta sesion"' : ''} style="padding: 6px 12px; font-size: 13px;">Borrar</button>
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
                    await updateDoc(doc(db, 'users', userId), {
                        role: nextRole,
                        isAdmin: nextRole === 'admin'
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
                    await updateDoc(doc(db, 'users', e.target.dataset.id), {
                        'licenses.Sheet Metal Nesting.status': status,
                        hasActiveLicense: action === 'grant' // Fallback flag kept in sync just in case
                    });
                    logAudit(action === 'grant' ? 'Otorgar licencia' : 'Revocar licencia',
                        `${action === 'grant' ? 'Otorgó' : 'Revocó'} la licencia de Sheet Metal Nesting al usuario ${e.target.dataset.id}.`);
                    loadUsers(); // Reload
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
    try {
        const docSnap = await getDoc(doc(db, 'settings', 'app_config'));
        if (docSnap.exists()) {
            toggle.checked = docSnap.data().requireLicense === true;
        } else {
            toggle.checked = false;
        }
    } catch(e) {
        console.error("Error loading settings", e);
    }

    toggle.addEventListener('change', async (e) => {
        try {
            await setDoc(doc(db, 'settings', 'app_config'), { requireLicense: e.target.checked }, { merge: true });
        } catch(err) {
            console.error("Error updating settings", err);
            alert("Error al actualizar la configuración");
            e.target.checked = !e.target.checked; // Revert
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
