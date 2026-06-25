import { getAdminServices, requireAdminUser } from '../lib/firebase-admin.js';

// Diagnósticos DXF (un solo endpoint para no pasar el limite de funciones de Vercel):
//   GET            -> lista los ultimos diagnosticos (metadata) para el panel admin.
//   GET ?id=<id>   -> descarga el contenido DXF de ese diagnostico (binario).
// Solo admin.
export default async function handler(req, res) {
    if (req.method !== 'GET') return res.status(405).json({ error: 'Method Not Allowed' });
    try {
        await requireAdminUser(req);
        const { db } = getAdminServices();

        // --- Descarga de un DXF puntual ---
        const id = String(req.query.id || '');
        if (id) {
            const doc = await db.collection('dxf_diagnostics').doc(id).get();
            if (!doc.exists) return res.status(404).json({ error: 'No existe.' });
            const x = doc.data() || {};
            if (x.truncated || !x.content) {
                return res.status(409).json({ error: 'El contenido no se guardo (archivo muy grande).' });
            }
            const buf = Buffer.from(x.content, 'base64');
            const safe = (x.filename || 'pieza.dxf').replace(/[^a-zA-Z0-9._-]/g, '_');
            res.setHeader('Content-Type', 'application/dxf');
            res.setHeader('Content-Disposition', `attachment; filename="${safe}"`);
            return res.status(200).send(buf);
        }

        // --- Listado de diagnosticos (metadata) ---
        const snap = await db
            .collection('dxf_diagnostics')
            .orderBy('createdAt', 'desc')
            .limit(300)
            .get();
        const items = snap.docs.map((d) => {
            const x = d.data() || {};
            return {
                id: d.id,
                filename: x.filename || 'pieza.dxf',
                sizeBytes: x.sizeBytes || 0,
                truncated: !!x.truncated,
                meta: x.meta || {},
                sessionId: x.sessionId || (x.meta && x.meta.sessionId) || '',
                country: x.country || '',
                city: x.city || '',
                createdAt: x.createdAt || '',
            };
        });
        res.status(200).json({ items });
    } catch (e) {
        res.status(e.statusCode || 500).json({ error: e.message || 'Error interno.' });
    }
}
