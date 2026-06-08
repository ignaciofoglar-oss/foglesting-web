import { getAdminServices } from './firebase-admin.js';

function json(res, status, payload) { res.status(status).json(payload); }

function decode(v) {
    try { return decodeURIComponent(String(v || '')); } catch { return String(v || ''); }
}

function geoFrom(req) {
    return {
        country: String(req.headers['x-vercel-ip-country'] || '').slice(0, 8),
        city: decode(req.headers['x-vercel-ip-city']).slice(0, 80),
        region: String(req.headers['x-vercel-ip-country-region'] || '').slice(0, 16),
    };
}

const num = (v) => {
    const n = Number(v);
    return Number.isFinite(n) ? n : 0;
};

// Registra una corrida del solver (online o descargable) en la coleccion solver_runs,
// agregando el pais aproximado (geo del edge de Vercel). Auth: x-fogl-key.
export default async function handler(req, res) {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, x-fogl-key');
    res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
    if (req.method === 'OPTIONS') return res.status(204).end();
    if (req.method !== 'POST') return json(res, 405, { error: 'Method Not Allowed' });

    const key = String(req.headers['x-fogl-key'] || '').trim();
    const expected = String(process.env.DIAGNOSTIC_UPLOAD_KEY || '').trim();
    if (!expected || key !== expected) return json(res, 401, { error: 'No autorizado.' });

    const b = req.body || {};
    const runId = String(b.runId || '').slice(0, 64);

    try {
        const { db } = getAdminServices();

        // Segundo POST: solo marcar como guardado (cuando el usuario exporta el DXF).
        if (b.savedUpdate === true && runId) {
            await db.collection('solver_runs').doc(runId).set({
                saved: true,
                total_time_to_save_sec: num(b.total_time_to_save_sec),
                savedAtISO: new Date().toISOString(),
            }, { merge: true });
            return json(res, 200, { ok: true, updated: true });
        }

        const geo = geoFrom(req);
        const data = {
            source: String(b.source || 'desktop').slice(0, 16),
            date: new Date().toISOString().split('T')[0],
            timestamp: new Date(),
            timestampISO: new Date().toISOString(),
            country: geo.country,
            city: geo.city,
            region: geo.region,
            app_version: String(b.appVersion || '').slice(0, 32),
            machine: String(b.machine || '').slice(0, 120),
            dxf_count: num(b.dxf_count),
            file_count: num(b.file_count),
            sheet_width: num(b.sheet_width),
            sheet_height: num(b.sheet_height),
            iterations: num(b.iterations),
            optimization_type: String(b.optimization_type || '').slice(0, 32),
            final_utilization: num(b.final_utilization),
            sheets_used: num(b.sheets_used),
            placed_count: num(b.placed_count),
            unplaced_count: num(b.unplaced_count),
            best_solution_time_sec: num(b.best_solution_time_sec),
            total_time_sec: num(b.total_time_sec),
            saved: b.saved === true,
        };

        if (runId) {
            await db.collection('solver_runs').doc(runId).set(data, { merge: true });
            return json(res, 200, { ok: true, id: runId });
        }
        const ref = await db.collection('solver_runs').add(data);
        return json(res, 200, { ok: true, id: ref.id });
    } catch (e) {
        return json(res, e.statusCode || 500, { error: e.message || 'Error interno.' });
    }
}
