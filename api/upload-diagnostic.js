import { getAdminServices } from './firebase-admin.js';

// Guarda contenido del DXF hasta este tamano; mas grande -> solo metadata.
const MAX_CONTENT_BYTES = 600 * 1024;

function json(res, status, payload) {
    res.status(status).json(payload);
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

    const body = req.body || {};
    const filename = String(body.filename || 'pieza.dxf').slice(0, 200);
    const contentB64 = typeof body.contentB64 === 'string' ? body.contentB64 : '';
    const meta = body.meta && typeof body.meta === 'object' ? body.meta : {};

    if (!contentB64) return json(res, 400, { error: 'Falta contentB64.' });

    const approxBytes = Math.floor((contentB64.length * 3) / 4);
    const tooBig = approxBytes > MAX_CONTENT_BYTES;

    try {
        const { db } = getAdminServices();
        const doc = {
            filename,
            sizeBytes: approxBytes,
            content: tooBig ? '' : contentB64,
            truncated: tooBig,
            meta: {
                bboxW: Number(meta.bboxW) || null,
                bboxH: Number(meta.bboxH) || null,
                appVersion: String(meta.appVersion || '').slice(0, 32),
                machine: String(meta.machine || '').slice(0, 120),
                source: String(meta.source || '').slice(0, 200),
            },
            createdAt: new Date().toISOString(),
        };
        const ref = await db.collection('dxf_diagnostics').add(doc);
        return json(res, 200, { ok: true, id: ref.id, truncated: tooBig });
    } catch (e) {
        return json(res, e.statusCode || 500, { error: e.message || 'Error interno.' });
    }
}
