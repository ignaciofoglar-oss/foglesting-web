import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import { 
    getAuth, 
    signInWithEmailAndPassword, 
    onAuthStateChanged, 
    signOut 
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-auth.js";
import { 
    getFirestore, 
    collection, 
    getDocs, 
    doc, 
    getDoc, 
    setDoc, 
    addDoc, 
    updateDoc, 
    query, 
    orderBy, 
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

const navItems = document.querySelectorAll('.nav-item');
const views = document.querySelectorAll('.view');

// 1. Authentication State Observer
onAuthStateChanged(auth, (user) => {
    if (user) {
        // User is logged in
        loginScreen.classList.remove('active');
        dashboardScreen.classList.add('active');
        loadDashboardData();
    } else {
        // User is logged out
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
    loadLicenses();
}

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
            container.innerHTML = '<p class="loading">No hay mensajes todavía.</p>';
            return;
        }
        
        let html = '';
        querySnapshot.forEach((doc) => {
            const msg = doc.data();
            const date = msg.timestamp ? msg.timestamp.toDate().toLocaleString('es-AR') : 'Fecha desconocida';
            html += `
                <div class="message-card">
                    <div class="message-header">
                        <span class="message-name">${msg.name} (${msg.email})</span>
                        <span class="message-date">${date}</span>
                    </div>
                    <div class="message-body">${msg.message}</div>
                </div>
            `;
        });
        container.innerHTML = html;
    } catch (e) {
        console.error("Error loading messages:", e);
        container.innerHTML = '<p class="error-msg">Error al cargar mensajes (requiere crear índices o reglas).</p>';
    }
}

// 5c. Load and Generate Licenses
async function loadLicenses() {
    const container = document.getElementById('licenses-container');
    try {
        const q = query(collection(db, 'licenses'), orderBy('createdAt', 'desc'));
        const querySnapshot = await getDocs(q);
        
        if (querySnapshot.empty) {
            container.innerHTML = '<tr><td colspan="5" class="loading">No hay licencias generadas.</td></tr>';
            return;
        }
        
        container.innerHTML = '';
        querySnapshot.forEach((docSnap) => {
            const lic = docSnap.data();
            const date = lic.createdAt ? lic.createdAt.toDate().toLocaleDateString('es-AR') : 'N/A';
            const statusClass = lic.active ? 'status-active' : 'status-revoked';
            const statusText = lic.active ? 'ACTIVA' : 'REVOCADA';
            
            const tr = document.createElement('tr');
            tr.innerHTML = `
                <td class="mono">${lic.key}</td>
                <td>${lic.client}</td>
                <td>${date}</td>
                <td><span class="status-badge ${statusClass}">${statusText}</span></td>
                <td>
                    ${lic.active ? `<button class="btn-danger revoke-btn" data-id="${docSnap.id}">Revocar</button>` : '-'}
                </td>
            `;
            container.appendChild(tr);
        });
        
        // Add revoke listeners
        document.querySelectorAll('.revoke-btn').forEach(btn => {
            btn.addEventListener('click', async (e) => {
                if(confirm('¿Seguro que quieres revocar esta licencia? El programa dejará de funcionar con ella.')) {
                    await updateDoc(doc(db, 'licenses', e.target.dataset.id), { active: false });
                    loadLicenses(); // Reload
                }
            });
        });
        
    } catch (e) {
        console.error("Error loading licenses:", e);
        container.innerHTML = '<tr><td colspan="5" class="error-msg">Error al cargar licencias.</td></tr>';
    }
}

// Generate New License
document.getElementById('generate-license-btn').addEventListener('click', async () => {
    const clientName = document.getElementById('license-client').value.trim();
    if (!clientName) {
        alert('Por favor ingresa un nombre para el cliente.');
        return;
    }
    
    const btn = document.getElementById('generate-license-btn');
    btn.textContent = 'Generando...';
    btn.disabled = true;
    
    // Generate a random 16 character key (XXXX-XXXX-XXXX-XXXX)
    const charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    let key = "";
    for(let i=0; i<16; i++) {
        key += charset.charAt(Math.floor(Math.random() * charset.length));
        if((i+1) % 4 === 0 && i !== 15) key += "-";
    }
    
    try {
        await setDoc(doc(db, 'licenses', key), {
            key: key,
            client: clientName,
            active: true,
            createdAt: serverTimestamp()
        });
        document.getElementById('license-client').value = '';
        loadLicenses();
    } catch (e) {
        console.error("Error generating license:", e);
        alert('Error al generar licencia.');
    } finally {
        btn.textContent = 'Generar Nueva Clave';
        btn.disabled = false;
    }
});
