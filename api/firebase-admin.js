import admin from 'firebase-admin';

function parseServiceAccount() {
    if (process.env.FIREBASE_SERVICE_ACCOUNT_JSON) {
        const serviceAccount = JSON.parse(process.env.FIREBASE_SERVICE_ACCOUNT_JSON);
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
    const userSnap = await db.collection('users').doc(decoded.uid).get();
    const user = userSnap.exists ? userSnap.data() : {};

    if (user.role !== 'admin' && user.isAdmin !== true) {
        const error = new Error('Tu cuenta no tiene rol de administrador.');
        error.statusCode = 403;
        throw error;
    }

    return decoded;
}

export {
    getAdminServices,
    requireAdminUser
};
