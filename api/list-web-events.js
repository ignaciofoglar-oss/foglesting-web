import { getAdminServices, requireAdminUser } from '../lib/firebase-admin.js';

// Lista los ultimos eventos de telemetria web (coleccion web_events) para el
// panel admin -> pestaña "Tráfico web". Solo admin. Devuelve los mas recientes;
// el agrupado por visitante/sesion lo arma el cliente.
export default async function handler(req, res) {
    if (req.method !== 'GET') return res.status(405).json({ error: 'Method Not Allowed' });
    try {
        await requireAdminUser(req);
        const { db } = getAdminServices();
        const lim = Math.min(Math.max(parseInt(req.query.limit, 10) || 3000, 1), 8000);
        const snap = await db
            .collection('web_events')
            .orderBy('ts', 'desc')
            .limit(lim)
            .get();
        const items = snap.docs.map((d) => {
            const x = d.data() || {};
            const tsISO = x.serverISO || (x.ts && x.ts.toDate ? x.ts.toDate().toISOString() : '');
            return {
                id: d.id,
                visitorId: x.visitorId || '',
                sessionId: x.sessionId || '',
                event: x.event || '',
                path: x.path || '',
                title: x.title || '',
                referrer: x.referrer || '',
                detail: x.detail || '',
                lang: x.lang || '',
                device: x.device || '',
                browser: x.browser || '',
                os: x.os || '',
                screen: x.screen || '',
                utm: x.utm || '',
                isNew: !!x.isNew,
                ms: x.ms || 0,
                country: x.country || '',
                city: x.city || '',
                region: x.region || '',
                ts: tsISO,
            };
        });
        res.status(200).json({ items });
    } catch (e) {
        res.status(e.statusCode || 500).json({ error: e.message || 'Error interno.' });
    }
}
