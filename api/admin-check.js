import { requireAdminUser, getAdminServices } from '../lib/firebase-admin.js';

function json(res, status, payload) {
    res.status(status).json(payload);
}

export default async function handler(req, res) {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');
    res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
    if (req.method === 'OPTIONS') return res.status(204).end();

    if (req.method !== 'GET') {
        res.setHeader('Allow', 'GET');
        return json(res, 405, { error: 'Metodo no permitido.' });
    }

    try {
        const decoded = await requireAdminUser(req);
        const { db } = getAdminServices();
        const snap = await db.collection('users').doc(decoded.uid).get();
        const data = snap.exists ? snap.data() : {};

        return json(res, 200, {
            ok: true,
            uid: decoded.uid,
            email: decoded.email || data.email || '',
            role: data.role || '',
            isAdmin: data.isAdmin === true
        });
    } catch (error) {
        return json(res, error.statusCode || 500, {
            ok: false,
            error: error.message || 'No se pudo verificar el rol administrador.'
        });
    }
}
