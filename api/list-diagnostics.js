import { getAdminServices, requireAdminUser } from './firebase-admin.js';

export default async function handler(req, res) {
    if (req.method !== 'GET') return res.status(405).json({ error: 'Method Not Allowed' });
    try {
        await requireAdminUser(req);
        const { db } = getAdminServices();
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
                createdAt: x.createdAt || '',
            };
        });
        res.status(200).json({ items });
    } catch (e) {
        res.status(e.statusCode || 500).json({ error: e.message || 'Error interno.' });
    }
}
