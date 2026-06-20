import { getAdminServices } from './firebase-admin.js';

// Registra eventos de uso del programa (caja negra de la sesion): que boton toca
// el usuario, cuando carga, cuando corre, etc. Auth: x-fogl-key (igual que el
// resto de la telemetria). Acepta un evento o un lote (array) en "events".

function json(res, status, payload) { res.status(status).json(payload); }
function decode(v) { try { return decodeURIComponent(String(v || '')); } catch { return String(v || ''); } }

function geoFrom(req) {
    return {
        country: String(req.headers['x-vercel-ip-country'] || '').slice(0, 8),
        city: decode(req.headers['x-vercel-ip-city']).slice(0, 80),
    };
}

function clean(ev, geo, base) {
    return {
        sessionId: String(ev.sessionId || base.sessionId || '').slice(0, 48),
        source: String(ev.source || base.source || 'desktop').slice(0, 16),
        machine: String(ev.machine || base.machine || '').slice(0, 120),
        appVersion: String(ev.appVersion || base.appVersion || '').slice(0, 32),
        action: String(ev.action || '').slice(0, 48),
        detail: String(ev.detail || '').slice(0, 200),
        tsClientISO: String(ev.tsClientISO || '').slice(0, 32),
        country: geo.country,
        city: geo.city,
        ts: new Date(),
        serverISO: new Date().toISOString(),
    };
}

export default async function handler(req, res) {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, x-fogl-key');
    res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
    if (req.method === 'OPTIONS') return res.status(204).end();
    if (req.method !== 'POST') return json(res, 405, { error: 'Method Not Allowed' });

    const key = String(req.headers['x-fogl-key'] || '').trim();
    const expected = String(process.env.DIAGNOSTIC_UPLOAD_KEY || '').trim();
    if (!expected || key !== expected) return json(res, 401, { error: 'No autorizado.' });

    try {
        const { db } = getAdminServices();
        const geo = geoFrom(req);
        const b = req.body || {};
        const list = Array.isArray(b.events) ? b.events : [b];
        if (list.length === 0 || list.length > 200) return json(res, 400, { error: 'Lote invalido.' });

        const col = db.collection('session_events');
        const batch = db.batch();
        let n = 0;
        for (const ev of list) {
            if (!ev || !ev.action) continue;
            batch.set(col.doc(), clean(ev, geo, b));
            n++;
        }
        if (n > 0) await batch.commit();
        return json(res, 200, { ok: true, saved: n });
    } catch (e) {
        return json(res, e.statusCode || 500, { error: e.message || 'Error interno.' });
    }
}
