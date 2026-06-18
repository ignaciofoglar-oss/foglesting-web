import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import { 
    getAuth, 
    signInWithEmailAndPassword, 
    createUserWithEmailAndPassword,
    onAuthStateChanged,
    signOut,
    updatePassword
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-auth.js";
import { 
    getStorage, 
    ref, 
    uploadBytes, 
    getDownloadURL 
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-storage.js";
import { 
    getFirestore, 
    doc, 
    setDoc, 
    getDoc,
    serverTimestamp 
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-firestore.js";

// Firebase Configuration from User (Same as admin.js)
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
const authForm = document.getElementById('auth-form');
const toggleBtn = document.getElementById('toggle-auth-btn');
const authTitle = document.getElementById('auth-title');
const submitBtn = document.getElementById('submit-btn');
const errorMsg = document.getElementById('auth-error');
const successBox = document.getElementById('success-box');

// State
let isLoginMode = true;
const urlParams = new URLSearchParams(window.location.search);
const deviceCode = urlParams.get('code');

// Toggle Mode (Login <-> Register)
toggleBtn.addEventListener('click', () => {
    isLoginMode = !isLoginMode;
    if (isLoginMode) {
        authTitle.textContent = 'Iniciar Sesión';
        submitBtn.textContent = 'Ingresar';
        toggleBtn.innerHTML = '¿No tenés cuenta? <span>Registrate acá</span>';
    } else {
        authTitle.textContent = 'Crear Cuenta';
        submitBtn.textContent = 'Registrarse';
        toggleBtn.innerHTML = '¿Ya tenés cuenta? <span>Iniciá Sesión</span>';
    }
    errorMsg.textContent = '';
});

// Dashboard Elements
const loginScreen = document.getElementById('login-screen');
const dashboardScreen = document.getElementById('dashboard-screen');
const dashName = document.getElementById('dash-name');
const dashPhone = document.getElementById('dash-phone');
const profileImg = document.getElementById('profile-img');
const profilePlaceholder = document.getElementById('profile-placeholder');
const profileUpload = document.getElementById('profile-upload');
const profilePicContainer = document.getElementById('profile-pic-container');
const licensesList = document.getElementById('licenses-list');
const dashDeviceMsg = document.getElementById('dash-device-msg');
const logoutBtn = document.getElementById('logout-btn');

// Topbar/Header info
const topbarName = document.getElementById('topbar-name');
const headerName = document.getElementById('header-name');
const headerEmail = document.getElementById('header-email');

// Profile Buttons
const saveProfileBtn = document.getElementById('save-profile-btn');
const changePasswordBtn = document.getElementById('change-password-btn');
const dashNewPassword = document.getElementById('dash-new-password');
const profileMsg = document.getElementById('profile-msg');
const passwordMsg = document.getElementById('password-msg');
const adminMasterLink = document.getElementById('admin-master-link');
const adminAccessBox = document.getElementById('admin-access-box');

// Process authentication success
async function showDashboard(user) {
    // Switch screens
    loginScreen.classList.remove('active');
    dashboardScreen.classList.add('active');
    
    headerEmail.textContent = user.email;
    topbarName.textContent = user.email;
    
    // Check user profile and licenses
    try {
        const userDocRef = doc(db, 'users', user.uid);
        const userDoc = await getDoc(userDocRef);
        
        if (userDoc.exists()) {
            const data = userDoc.data();
            const isAdmin = data.role === 'admin' || data.isAdmin === true;
            if (adminMasterLink) adminMasterLink.style.display = isAdmin ? 'flex' : 'none';
            if (adminAccessBox) adminAccessBox.style.display = isAdmin ? 'block' : 'none';
            if (data.name) {
                dashName.value = data.name;
                topbarName.textContent = data.name;
                headerName.textContent = data.name;
            } else {
                dashName.value = '';
                headerName.textContent = 'Usuario';
            }
            dashPhone.value = data.phone || '';
            if (data.photoURL) {
                profileImg.src = data.photoURL;
                profileImg.style.display = 'block';
                profilePlaceholder.style.display = 'none';
            }

            licensesList.innerHTML = ''; // Clear loading text
            
            // Render multiple products/licenses
            // Fallback to legacy `hasActiveLicense` if `licenses` object is not present
            const products = data.licenses || {};
            if (!data.licenses && data.hasActiveLicense !== undefined) {
                products['Sheet Metal Nesting'] = { status: data.hasActiveLicense ? 'active' : 'inactive' };
            }

            if (Object.keys(products).length === 0) {
                licensesList.innerHTML = '<div style="background: rgba(0,0,0,0.3); padding: 15px; border-radius: 8px; border: 1px solid #333; text-align: left;"><p style="color: var(--text-muted);">No posees ninguna licencia.</p></div>';
            } else {
                for (const [productName, licenseInfo] of Object.entries(products)) {
                    const isActive = licenseInfo.status === 'active';
                    const badgeClass = isActive ? 'status-active' : 'status-inactive';
                    const badgeText = isActive ? 'ACTIVA' : 'INACTIVA / PENDIENTE';

                    licensesList.innerHTML += `
                    <div style="background: rgba(0,0,0,0.3); padding: 15px; border-radius: 8px; border: 1px solid #333; text-align: left; display: flex; justify-content: space-between; align-items: center;">
                        <span style="color: #fff; font-weight: bold;">${productName}</span>
                        <div class="status-badge ${badgeClass}" style="margin-top: 0;">${badgeText}</div>
                    </div>`;
                }
            }

            // Plan / Suscripcion (no afecta a los usuarios actuales: solo muestra
            // el plan y permite probar el flujo de pago cuando el modo test esta on).
            renderSubscription(user, products).catch((e) => console.error('subscription:', e));
        } else {
            if (adminMasterLink) adminMasterLink.style.display = 'none';
            if (adminAccessBox) adminAccessBox.style.display = 'none';
        }
    } catch (e) {
        console.error("Error fetching user data:", e);
        licensesList.innerHTML = '<div style="background: rgba(184, 60, 60, 0.2); padding: 15px; border-radius: 8px; border: 1px solid #ff6b6b; text-align: left; color: #ff6b6b;">Error al verificar licencias</div>';
    }

    if (deviceCode) {
        dashDeviceMsg.style.display = 'block';
    }
}

// Muestra el plan del programa de escritorio y, en modo de prueba, permite
// simular un pago para activar la licencia (sin tocar a los usuarios actuales).
async function renderSubscription(user, products) {
    const box = document.getElementById('subscription-box');
    if (!box) return;
    const PRODUCT = 'Sheet Metal Nesting';

    // Config de facturacion (precio, modo prueba). La controla el admin.
    let cfg = {};
    try {
        const snap = await getDoc(doc(db, 'settings', 'billing'));
        if (snap.exists()) cfg = snap.data();
    } catch (e) { console.error('billing cfg:', e); }

    const lic = (products && products[PRODUCT]) || {};
    const isActive = lic.status === 'active';
    const price = Number(cfg.priceArs) || 0;
    const priceTxt = price > 0 ? `$${price.toLocaleString('es-AR')} ARS / mes` : 'Suscripción mensual';
    const testMode = cfg.testMode === true;

    let html = `<h4 style="color: var(--cream, #ffdfb8); margin-bottom: 6px;">Plan — Programa de escritorio</h4>`;

    if (isActive) {
        const vto = lic.expiresAt ? new Date(lic.expiresAt).toLocaleDateString('es-AR') : '—';
        html += `<p style="color:#7fce9b; margin:0 0 4px;">✅ Suscripción activa (${lic.plan || 'mensual'}).</p>
                 <p style="color: var(--text-muted); font-size:13px; margin:0;">Vence el ${vto}.</p>`;
    } else {
        html += `<p style="color: var(--text-muted); margin:0 0 14px; font-size:14px;">
                    Suscribite para usar el programa de escritorio sin límites. <strong style="color:#fff;">${priceTxt}</strong>.
                 </p>
                 <button class="btn-primary" id="subscribe-btn" style="width:auto;">Suscribirme</button>`;
    }

    if (testMode) {
        html += `<div style="margin-top:14px; padding-top:14px; border-top:1px dashed rgba(255,255,255,0.12);">
                    <p style="color:#ffd2a8; font-size:12px; margin:0 0 8px;">🧪 Modo de prueba activo</p>
                    <button id="sim-pay-btn" style="width:auto; padding:8px 14px; font-size:13px; background:#3a4a2e; color:#cfe9b0; border:1px solid #5c7a3a; border-radius:6px; cursor:pointer;">
                        Simular pago aprobado (TEST)
                    </button>
                    <span id="sim-pay-msg" style="margin-left:10px; font-size:13px;"></span>
                 </div>`;
    }

    box.innerHTML = html;
    box.style.display = 'block';

    const subBtn = document.getElementById('subscribe-btn');
    if (subBtn) subBtn.addEventListener('click', () => {
        alert('La pasarela de pago se conecta en el próximo paso (Mercado Pago / Stripe). Por ahora, en modo de prueba podés usar "Simular pago aprobado".');
    });

    const simBtn = document.getElementById('sim-pay-btn');
    if (simBtn) simBtn.addEventListener('click', async () => {
        const msg = document.getElementById('sim-pay-msg');
        simBtn.disabled = true;
        if (msg) { msg.style.color = '#cfe9b0'; msg.textContent = 'Procesando...'; }
        try {
            const token = await user.getIdToken();
            const r = await fetch('/api/activate-license', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
                body: JSON.stringify({ simulate: true }),
            });
            const data = await r.json();
            if (r.ok && data.ok) {
                if (msg) { msg.style.color = '#7fce9b'; msg.textContent = '✅ Licencia activada. Recargando...'; }
                setTimeout(() => location.reload(), 1200);
            } else {
                if (msg) { msg.style.color = '#ff8888'; msg.textContent = data.error || ('Error ' + r.status); }
                simBtn.disabled = false;
            }
        } catch (e) {
            if (msg) { msg.style.color = '#ff8888'; msg.textContent = e.message; }
            simBtn.disabled = false;
        }
    });
}

async function handleAuthSuccess(user) {
    // Show UI immediately to prevent hanging
    showDashboard(user);

    if (deviceCode) {
        // Link device
        try {
            setDoc(doc(db, 'device_logins', deviceCode), {
                uid: user.uid,
                email: user.email,
                timestamp: serverTimestamp()
            }).catch(e => console.error(e));
        } catch (error) {
            console.error("Error linking device:", error);
        }
    } else {
        // Just logged in on web without a device code
        // Update lastLogin timestamp without blocking UI
        setDoc(doc(db, 'users', user.uid), {
            lastLogin: serverTimestamp()
        }, { merge: true }).catch(e => console.error(e));
    }
}

// Form Submit
authForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    errorMsg.textContent = '';
    const email = document.getElementById('email').value;
    const password = document.getElementById('password').value;
    
    submitBtn.disabled = true;
    submitBtn.textContent = 'Cargando...';

    try {
        if (isLoginMode) {
            // LOGIN
            const userCredential = await signInWithEmailAndPassword(auth, email, password);
            await handleAuthSuccess(userCredential.user);
        } else {
            // REGISTER
            const userCredential = await createUserWithEmailAndPassword(auth, email, password);
            const user = userCredential.user;
            
            // Create user doc in Firestore but don't await to prevent UI hang
            setDoc(doc(db, 'users', user.uid), {
                email: user.email,
                name: '',
                phone: '',
                photoURL: '',
                licenses: {
                    'Sheet Metal Nesting': { status: 'inactive' }
                },
                role: 'user',
                createdAt: serverTimestamp()
            }).catch(e => console.error("Firestore write error:", e));
            
            handleAuthSuccess(user);
        }
    } catch (error) {
        console.error("Auth error:", error);
        submitBtn.disabled = false;
        submitBtn.textContent = isLoginMode ? 'Ingresar' : 'Registrarse';
        
        if (error.code === 'auth/email-already-in-use') {
            errorMsg.textContent = 'El email ya está registrado. Iniciá sesión.';
        } else if (error.code === 'auth/wrong-password' || error.code === 'auth/user-not-found' || error.code === 'auth/invalid-credential') {
            errorMsg.textContent = 'Credenciales incorrectas.';
        } else if (error.code === 'auth/weak-password') {
            errorMsg.textContent = 'La contraseña debe tener al menos 6 caracteres.';
        } else {
            errorMsg.textContent = 'Ocurrió un error. Verificá tus datos.';
        }
    }
});

// Profile interactions
profilePicContainer.addEventListener('click', () => {
    profileUpload.click();
});

profileUpload.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    
    const user = auth.currentUser;
    if (!user) return;

    try {
        profilePlaceholder.style.display = 'none';
        profileImg.style.display = 'none';
        
        // Upload to Firebase Storage
        const storageRef = ref(storage, `profiles/${user.uid}`);
        await uploadBytes(storageRef, file);
        const photoURL = await getDownloadURL(storageRef);
        
        // Update Firestore
        await setDoc(doc(db, 'users', user.uid), { photoURL: photoURL }, { merge: true });
        
        // Update UI
        profileImg.src = photoURL;
        profileImg.style.display = 'block';
        profileMsg.textContent = 'Foto actualizada correctamente.';
        profileMsg.style.color = 'var(--success)';
        profileMsg.style.display = 'block';
        setTimeout(() => { profileMsg.style.display = 'none'; }, 3000);
    } catch (err) {
        console.error(err);
        profileMsg.textContent = 'Error al subir la foto.';
        profileMsg.style.color = '#ff6b6b';
        profileMsg.style.display = 'block';
    }
});

saveProfileBtn.addEventListener('click', async () => {
    const user = auth.currentUser;
    if (!user) return;
    
    saveProfileBtn.disabled = true;
    saveProfileBtn.textContent = 'Guardando...';
    
    try {
        await setDoc(doc(db, 'users', user.uid), {
            name: dashName.value,
            phone: dashPhone.value
        }, { merge: true });
        
        profileMsg.textContent = 'Perfil actualizado.';
        profileMsg.style.color = 'var(--success)';
        profileMsg.style.display = 'block';
        setTimeout(() => { profileMsg.style.display = 'none'; }, 3000);
    } catch (err) {
        profileMsg.textContent = 'Error al guardar el perfil.';
        profileMsg.style.color = '#ff6b6b';
        profileMsg.style.display = 'block';
    }
    
    saveProfileBtn.disabled = false;
    saveProfileBtn.textContent = 'Guardar Perfil';
});

changePasswordBtn.addEventListener('click', async () => {
    const user = auth.currentUser;
    const newPassword = dashNewPassword.value;
    if (!user || newPassword.length < 6) {
        passwordMsg.textContent = 'La contraseña debe tener al menos 6 caracteres.';
        passwordMsg.style.color = '#ff6b6b';
        passwordMsg.style.display = 'block';
        return;
    }
    
    changePasswordBtn.disabled = true;
    changePasswordBtn.textContent = 'Cambiando...';
    
    try {
        await updatePassword(user, newPassword);
        passwordMsg.textContent = 'Contraseña actualizada.';
        passwordMsg.style.color = 'var(--success)';
        passwordMsg.style.display = 'block';
        dashNewPassword.value = '';
        setTimeout(() => { passwordMsg.style.display = 'none'; }, 3000);
    } catch (err) {
        console.error(err);
        if (err.code === 'auth/requires-recent-login') {
            passwordMsg.textContent = 'Por seguridad, debés cerrar sesión y volver a ingresar para cambiar la contraseña.';
        } else {
            passwordMsg.textContent = 'Error al cambiar contraseña.';
        }
        passwordMsg.style.color = '#ff6b6b';
        passwordMsg.style.display = 'block';
    }
    
    changePasswordBtn.disabled = false;
    changePasswordBtn.textContent = 'Cambiar Contraseña';
});

// Logout functionality
logoutBtn.addEventListener('click', async () => {
    await signOut(auth);
    window.location.reload();
});

// Listen for Auth State
onAuthStateChanged(auth, (user) => {
    if (user) {
        // Automatically link device if code exists and user was already logged in
        if (deviceCode) {
            handleAuthSuccess(user);
        } else {
            showDashboard(user);
        }
    }
});
