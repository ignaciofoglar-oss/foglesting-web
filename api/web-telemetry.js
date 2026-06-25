import { getAdminServices, requireAdminUser } from '../lib/firebase-admin.js';

// Telemetría web (un solo endpoint para no pasar el limite de funciones de Vercel):
//   POST  -> recibe eventos del tracker del sitio (track.js), agrega geo por IP
//            y los guarda en Firestore (coleccion web_events). Auth: x-fogl-key
//            (header) o body.k = DIAGNOSTIC_UPLOAD_KEY.
//   GET   -> lista los ultimos eventos para el panel admin (solo admin).

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

async function handlePost(req, res) {
    const body = req.body || {};
    const key = String(req.headers['x-fogl-key'] || body.k || '').trim();
    const expected = String(process.env.DIAGNOSTIC_UPLOAD_KEY || '').trim();
    if (!expected || key !== expected) return json(res, 401, { error: 'No autorizado.' });

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
}

async function handleGet(req, res) {
    await requireAdminUser(req);
    const { db } = getAdminServices();
    const lim = Math.min(Math.max(parseInt(req.query.limit, 10) || 3000, 1), 8000);
    const snap = await db.collection('web_events').orderBy('ts', 'desc').limit(lim).get();
    const items = snap.docs.map((d) => {
        const x = d.data() || {};
        const tsISO = x.serverISO || (x.ts && x.ts.toDate ? x.ts.toDate().toISOString() : '');
        return {
            id: d.id,
            visitorId: x.visitorId || '', sessionId: x.sessionId || '',
            event: x.event || '', path: x.path || '', title: x.title || '',
            referrer: x.referrer || '', detail: x.detail || '', lang: x.lang || '',
            device: x.device || '', browser: x.browser || '', os: x.os || '',
            screen: x.screen || '', utm: x.utm || '', isNew: !!x.isNew, ms: x.ms || 0,
            country: x.country || '', city: x.city || '', region: x.region || '', ts: tsISO,
        };
    });
    return json(res, 200, { items });
}

export default async function handler(req, res) {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, x-fogl-key, Authorization');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    if (req.method === 'OPTIONS') return res.status(204).end();
    try {
        if (req.method === 'POST') return await handlePost(req, res);
        if (req.method === 'GET') return await handleGet(req, res);
        return json(res, 405, { error: 'Method Not Allowed' });
    } catch (e) {
        return json(res, e.statusCode || 500, { error: e.message || 'Error interno.' });
    }
}
