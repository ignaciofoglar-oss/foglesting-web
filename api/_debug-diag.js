// TEMPORAL: diagnostico. Lista los ultimos documentos de dxf_diagnostics
// (sin contenido) para verificar si las subidas estan llegando. BORRAR luego.
import { getAdminServices } from './firebase-admin.js';

export default async function handler(req, res) {
    const key = String(req.headers['x-fogl-key'] || '').trim();
    const expected = String(process.env.DIAGNOSTIC_UPLOAD_KEY || '').trim();
    if (!expected || key !== expected) return res.status(401).json({ error: 'No autorizado.' });
    try {
        const { db } = getAdminServices();
        const snap = await db
            .collection('dxf_diagnostics')
            .orderBy('createdAt', 'desc')
            .limit(15)
            .get();
        const recent = snap.docs.map((d) => {
            const x = d.data() || {};
            return {
                filename: x.filename,
                machine: (x.meta || {}).machine,
                bboxW: (x.meta || {}).bboxW,
                bboxH: (x.meta || {}).bboxH,
                createdAt: x.createdAt,
            };
        });
        res.status(200).json({ count_shown: recent.length, recent });
    } catch (e) {
        res.status(500).json({ error: e.message || 'error' });
    }
}
