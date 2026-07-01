import admin from 'firebase-admin';
import { getAdminServices } from '../lib/firebase-admin.js';

// Registra metricas de la web del lado del servidor (Admin SDK). Reemplaza las
// escrituras del cliente a metrics/{hoy}, que dejaron de funcionar cuando se
// endurecieron las reglas de Firestore (page_views y time_spent quedaban sin
// registrar). Las descargas se cuentan en /api/download (redireccion).

function todayStr() {
    const d = new Date();
    return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
}

export default async function handler(req, res) {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
    if (req.method === 'OPTIONS') return res.status(204).end();
    if (req.method !== 'POST') return res.status(405).json({ error: 'Method Not Allowed' });

    const body = req.body || {};
    const type = String(body.type || '');
    const seconds = Math.max(0, Math.min(86400, parseInt(body.seconds, 10) || 0));

    const inc = { date: todayStr() };
    if (type === 'page_view') {
        inc.page_views = admin.firestore.FieldValue.increment(1);
    } else if (type === 'download') {
        inc.downloads = admin.firestore.FieldValue.increment(1);
    } else if (type === 'time_spent') {
        if (seconds <= 0) return res.status(200).json({ ok: true, skipped: true });
        inc.time_spent = admin.firestore.FieldValue.increment(seconds);
    } else {
        return res.status(400).json({ error: 'type invalido' });
    }

    try {
        const { db } = getAdminServices();
        await db.collection('metrics').doc(todayStr()).set(inc, { merge: true });
        return res.status(200).json({ ok: true });
    } catch (e) {
        return res.status(e.statusCode || 500).json({ error: e.message || 'Error interno.' });
    }
}
