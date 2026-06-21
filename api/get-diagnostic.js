import { getAdminServices, requireAdminUser } from '../lib/firebase-admin.js';

export default async function handler(req, res) {
    if (req.method !== 'GET') return res.status(405).json({ error: 'Method Not Allowed' });
    try {
        await requireAdminUser(req);
        const id = String(req.query.id || '');
        if (!id) return res.status(400).json({ error: 'Falta id.' });
        const { db } = getAdminServices();
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
        res.status(200).send(buf);
    } catch (e) {
        res.status(e.statusCode || 500).json({ error: e.message || 'Error interno.' });
    }
}
