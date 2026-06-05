import { getAdminServices, requireAdminUser } from './firebase-admin.js';

export default async function handler(req, res) {
    if (req.method !== 'POST' && req.method !== 'DELETE') {
        return res.status(405).json({ error: 'Method Not Allowed' });
    }
    try {
        await requireAdminUser(req);
        const body = req.body || {};
        const id = String(body.id || req.query.id || '');
        const all = body.all === true || req.query.all === 'true';
        const { db } = getAdminServices();

        if (all) {
            // Borrar todos los diagnosticos (en lotes).
            let deleted = 0;
            while (true) {
                const snap = await db.collection('dxf_diagnostics').limit(300).get();
                if (snap.empty) break;
                const batch = db.batch();
                snap.docs.forEach((d) => batch.delete(d.ref));
                await batch.commit();
                deleted += snap.size;
                if (snap.size < 300) break;
            }
            return res.status(200).json({ ok: true, deleted });
        }

        if (!id) return res.status(400).json({ error: 'Falta id.' });
        await db.collection('dxf_diagnostics').doc(id).delete();
        return res.status(200).json({ ok: true, deleted: 1 });
    } catch (e) {
        return res.status(e.statusCode || 500).json({ error: e.message || 'Error interno.' });
    }
}
