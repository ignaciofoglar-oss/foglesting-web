import { getAdminServices } from '../lib/firebase-admin.js';

// Telemetría web: recibe eventos del tracker del sitio (track.js) y los guarda
// en Firestore (coleccion web_events) agregandoles el pais por IP (headers de
// Vercel). Auth: x-fogl-key (header) o body.k -> DIAGNOSTIC_UPLOAD_KEY.
// Acepta un lote: { events: [...] }.

function json(res, status, payload) { res.status(status).json(payload); }
function decode(v) { try { return decodeURIComponent(String(v || '')); } catch { return String(v || ''); } }

function geoFrom(req) {
    return {
        country: String(req.headers['x-vercel-ip-country'] || '').slice(0, 8),
        city: decode(req.headers['x-vercel-ip-city']).slice(0, 80),
        region: String(req.headers['x-vercel-ip-country-region'] || '').slice(0, 16),
    };
}

function clean(ev, geo) {
    return {
        visitorId: String(ev.visitorId || '').slice(0, 40),
        sessionId: String(ev.sessionId || '').slice(0, 48),
        event: String(ev.event || '').slice(0, 40),
        path: String(ev.path || '').slice(0, 200),
        title: String(ev.title || '').slice(0, 160),
        referrer: String(ev.referrer || '').slice(0, 300),
        detail: String(ev.detail || '').slice(0, 300),
        lang: String(ev.lang || '').slice(0, 8),
        device: String(ev.device || '').slice(0, 16),
        browser: String(ev.browser || '').slice(0, 24),
        os: String(ev.os || '').slice(0, 24),
        screen: String(ev.screen || '').slice(0, 16),
        utm: String(ev.utm || '').slice(0, 200),
        isNew: !!ev.isNew,
        ms: Number(ev.ms) || 0,
        ua: String(ev.ua || '').slice(0, 300),
        country: geo.country,
        city: geo.city,
        region: geo.region,
        tsClientISO: String(ev.tsClientISO || '').slice(0, 32),
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

    const body = req.body || {};
    const key = String(req.headers['x-fogl-key'] || body.k || '').trim();
    const expected = String(process.env.DIAGNOSTIC_UPLOAD_KEY || '').trim();
    if (!expected || key !== expected) return json(res, 401, { error: 'No autorizado.' });

    try {
        const { db } = getAdminServices();
        const geo = geoFrom(req);
        const list = Array.isArray(body.events) ? body.events : [body];
        if (list.length === 0 || list.length > 200) return json(res, 400, { error: 'Lote invalido.' });

        const col = db.collection('web_events');
        const batch = db.batch();
        let n = 0;
        for (const ev of list) {
            if (!ev || !ev.event) continue;
            batch.set(col.doc(), clean(ev, geo));
            n++;
        }
        if (n > 0) await batch.commit();
        return json(res, 200, { ok: true, saved: n });
    } catch (e) {
        return json(res, e.statusCode || 500, { error: e.message || 'Error interno.' });
    }
}
