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

        // --- Corridas del solver (server-side: Admin SDK saltea las reglas de
        // Firestore, que ahora bloquean la lectura desde el cliente). Se cruzan
        // con las sesiones en el panel para mostrar "N corridas" por sesion. ---
        let runs = [];
        try {
            const rsnap = await db
                .collection('solver_runs')
                .orderBy('timestamp', 'desc')
                .limit(1500)
                .get();
            runs = rsnap.docs.map((d) => {
                const x = d.data() || {};
                let ts = '';
                if (x.timestamp && typeof x.timestamp.toDate === 'function') {
                    ts = x.timestamp.toDate().toISOString();
                } else if (x.timestampISO) {
                    ts = x.timestampISO;
                }
                return {
                    sessionId: x.sessionId || '',
                    machine: x.machine || '',
                    source: x.source || '',
                    country: x.country || '',
                    date: x.date || '',
                    timestampISO: ts,
                    placed_count: x.placed_count,
                    unplaced_count: x.unplaced_count,
                    sheets_used: x.sheets_used,
                    final_utilization: x.final_utilization,
                    optimization_type: x.optimization_type || '',
                    fill_sheet: !!x.fill_sheet,
                    saved: !!x.saved,
                    best_solution_time_sec: x.best_solution_time_sec,
                    total_time_to_save_sec: x.total_time_to_save_sec,
                    placed_pieces: x.placed_pieces,
                    app_version: x.app_version || '',
                    dxf_count: x.dxf_count,
                };
            });
        } catch (e) {
            console.warn('No se pudieron leer corridas del solver:', e && e.message);
        }

        res.status(200).json({ items, runs });
    } catch (e) {
        res.status(e.statusCode || 500).json({ error: e.message || 'Error interno.' });
    }
}
