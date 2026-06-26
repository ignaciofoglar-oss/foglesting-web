import admin from 'firebase-admin';

function cleanJsonEnv(value) {
    let raw = String(value || '').trim();
    raw = raw.replace(/^\uFEFF/, '').trim();

    // Handles values pasted as:
    // ```json
    // { ... }
    // ```
    raw = raw.replace(/^```(?:json|JSON)?\s*/i, '').replace(/\s*```$/i, '').trim();

    // Handles values pasted as:
    // JSON
    // { ... }
    if (/^json\s*[\r\n{]/i.test(raw)) {
        raw = raw.replace(/^json\s*/i, '').trim();
    }

    return raw;
}

function parseJsonOrBase64(value) {
    const raw = cleanJsonEnv(value);
    if (!raw) return null;

    const candidates = [raw];

    if (!raw.startsWith('{')) {
        try {
            candidates.push(Buffer.from(raw, 'base64').toString('utf8').trim());
        } catch {
            // Ignore base64 fallback errors.
        }
    }

    for (const candidate of candidates) {
        if (!candidate || !candidate.startsWith('{')) continue;
        try {
            return JSON.parse(candidate);
        } catch {
            // Keep trying candidates so the final error is user-facing.
        }
    }

    const error = new Error('FIREBASE_SERVICE_ACCOUNT_JSON no es un JSON valido. Pegá el JSON completo de la service account, sin la palabra JSON arriba.');
    error.statusCode = 500;
    throw error;
}

function parseServiceAccount() {
    if (process.env.FIREBASE_SERVICE_ACCOUNT_JSON) {
        const serviceAccount = parseJsonOrBase64(process.env.FIREBASE_SERVICE_ACCOUNT_JSON);
        if (serviceAccount.private_key) {
            serviceAccount.private_key = serviceAccount.private_key.replace(/\\n/g, '\n');
        }
        return serviceAccount;
    }

    const projectId = process.env.FIREBASE_PROJECT_ID;
    const clientEmail = process.env.FIREBASE_CLIENT_EMAIL;
    const privateKey = process.env.FIREBASE_PRIVATE_KEY?.replace(/\\n/g, '\n');

    if (projectId && clientEmail && privateKey) {
        return {
            project_id: projectId,
            client_email: clientEmail,
            private_key: privateKey
        };
    }

    return null;
}

function getAdminApp() {
    if (admin.apps.length) return admin.apps[0];

    const serviceAccount = parseServiceAccount();
    if (!serviceAccount) {
        throw new Error('Falta configurar Firebase Admin en Vercel.');
    }

    return admin.initializeApp({
        credential: admin.credential.cert(serviceAccount),
        projectId: serviceAccount.project_id || process.env.FIREBASE_PROJECT_ID
    });
}

function getAdminServices() {
    getAdminApp();
    return {
        auth: admin.auth(),
        db: admin.firestore()
    };
}

function configuredAdminEmails() {
    const defaults = [
        'ignacio.foglar@gmail.com',
        'ignacio_ggirard@hotmail.com',
        'francisco.foglar@gmail.com'
    ];
    const extra = String(process.env.ADMIN_EMAILS || '')
        .split(',')
        .map((x) => x.trim().toLowerCase())
        .filter(Boolean);
    return new Set([...defaults, ...extra]);
}

// Verifica que el request venga de un usuario logueado (cualquiera, no admin).
// Devuelve el token decodificado (incluye uid, email).
async function verifyUser(req) {
    const header = req.headers.authorization || '';
    const match = header.match(/^Bearer\s+(.+)$/i);
    if (!match) {
        const error = new Error('No autorizado.');
        error.statusCode = 401;
        throw error;
    }
    const { auth } = getAdminServices();
    return auth.verifyIdToken(match[1]);
}

async function requireAdminUser(req) {
    const header = req.headers.authorization || '';
    const match = header.match(/^Bearer\s+(.+)$/i);
    if (!match) {
        const error = new Error('No autorizado.');
        error.statusCode = 401;
        throw error;
    }

    const { auth, db } = getAdminServices();
    const decoded = await auth.verifyIdToken(match[1]);
    const email = String(decoded.email || '').trim().toLowerCase();
    const userSnap = await db.collection('users').doc(decoded.uid).get();
    const user = userSnap.exists ? userSnap.data() : {};

    if (user.role !== 'admin' && user.isAdmin !== true && !configuredAdminEmails().has(email)) {
        const error = new Error('Tu cuenta no tiene rol de administrador.');
        error.statusCode = 403;
        throw error;
    }

    return decoded;
}

export {
    getAdminServices,
    verifyUser,
    requireAdminUser
};
