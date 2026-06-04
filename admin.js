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
        let rows = '';
        for (const it of items) {
            const m = it.meta || {};
            const kb = (it.sizeBytes / 1024).toFixed(1);
            const bbox = (m.bboxW && m.bboxH) ? `${Math.round(m.bboxW)} × ${Math.round(m.bboxH)} mm` : '—';
            const date = it.createdAt ? new Date(it.createdAt).toLocaleString('es-AR') : '—';
            const dl = it.truncated
                ? '<span class="release-muted">muy grande</span>'
                : `<button class="btn-primary diag-dl" data-id="${escapeHtmlDiag(it.id)}" data-name="${escapeHtmlDiag(it.filename)}" style="padding:6px 12px;font-size:13px;">Descargar</button>`;
            rows += `
                <tr>
                    <td>${escapeHtmlDiag(it.filename)}</td>
                    <td>${bbox}</td>
                    <td>${kb} KB</td>
                    <td>${escapeHtmlDiag(m.appVersion || '—')}</td>
                    <td>${escapeHtmlDiag(m.machine || '—')}</td>
                    <td>${escapeHtmlDiag(date)}</td>
                    <td>${dl}</td>
                </tr>`;
        }
        container.innerHTML = `
            <table class="diag-table" style="width:100%;border-collapse:collapse;">
                <thead><tr style="text-align:left;">
                    <th>Archivo</th><th>Tamaño pieza</th><th>Peso</th><th>Versión</th><th>Equipo</th><th>Fecha</th><th></th>
                </tr></thead>
                <tbody>${rows}</tbody>
            </table>`;
        container.querySelectorAll('.diag-dl').forEach((btn) => {
            btn.addEventListener('click', () => downloadDiagnostic(btn.dataset.id, btn.dataset.name));
        });
    } catch (e) {
        console.error('Error loading diagnostics:', e);
        container.innerHTML = `<p class="error-msg">No se pudieron cargar los diagnósticos: ${escapeHtmlDiag(e.message)}</p>`;
    }
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

// 5a. Load Metrics and draw Chart
let metricsChartInstance = null;

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
        const solverSnapshot = await getDocs(query(collection(db, 'solver_runs')));
        let totalSolverUses = 0;
        let totalDxfs = 0;
        let bestTimeSum = 0;
        let bestTimeCount = 0;
        let saveTimeSum = 0;
        let saveTimeCount = 0;

        solverSnapshot.forEach(doc => {
            const data = doc.data();
            totalSolverUses++;
            totalDxfs += (data.dxf_count || 0);
            if (data.best_solution_time_sec > 0) {
                bestTimeSum += data.best_solution_time_sec;
                bestTimeCount++;
            }
            if (data.saved && data.total_time_to_save_sec > 0) {
                saveTimeSum += data.total_time_to_save_sec;
                saveTimeCount++;
            }
        });

        document.getElementById('stat-solver-uses').textContent = totalSolverUses;
        document.getElementById('stat-solver-dxfs').textContent = totalDxfs;
        
        const avgBestTime = bestTimeCount > 0 ? (bestTimeSum / bestTimeCount).toFixed(1) : 0;
        const avgSaveTime = saveTimeCount > 0 ? (saveTimeSum / saveTimeCount).toFixed(1) : 0;
        
        document.getElementById('stat-solver-best-time').textContent = `${avgBestTime}s`;
        document.getElementById('stat-solver-save-time').textContent = `${avgSaveTime}s`;

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
