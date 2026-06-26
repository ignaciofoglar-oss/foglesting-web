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

function isoFrom(value) {
    if (!value) return '';
    if (typeof value === 'string') return value;
    if (value.toDate) return value.toDate().toISOString();
    if (value instanceof Date) return value.toISOString();
    return '';
}

function dayFromRun(x) {
    if (x.date) return String(x.date);
    const iso = isoFrom(x.timestamp) || x.timestampISO || x.serverISO || '';
    return iso ? iso.slice(0, 10) : '';
}

function num(v) {
    const n = Number(v);
    return Number.isFinite(n) ? n : 0;
}

function cleanMessageDoc(doc) {
    const x = doc.data() || {};
    return {
        id: doc.id,
        name: x.name || '',
        email: x.email || '',
        type: x.type || '',
        message: x.message || '',
        timestampISO: isoFrom(x.timestamp) || x.dateISO || ''
    };
}

function cleanAuditDoc(doc) {
    const x = doc.data() || {};
    return {
        id: doc.id,
        action: x.action || '',
        description: x.description || '',
        admin: x.admin || '',
        dateISO: isoFrom(x.ts) || x.dateISO || ''
    };
}

function buildMetricsResponse(metricsDocs, runDocs) {
    const metricRows = metricsDocs
        .filter((d) => d.id !== 'general')
        .map((d) => ({ id: d.id, ...(d.data() || {}) }))
        .sort((a, b) => String(a.id).localeCompare(String(b.id)));

    let totalViews = 0;
    let totalDownloads = 0;
    let totalTimeSpent = 0;
    const labels = [];
    const viewsData = [];
    const downloadsData = [];

    for (const row of metricRows) {
        const views = num(row.page_views);
        const downloads = num(row.downloads);
        const timeSpent = num(row.time_spent);
        totalViews += views;
        totalDownloads += downloads;
        totalTimeSpent += timeSpent;
        labels.push(row.id);
        viewsData.push(views);
        downloadsData.push(downloads);
    }

    const recentRuns = runDocs.map((d) => d.data() || {});
    const allRuns = recentRuns;
    let totalDxfs = 0, saved = 0;
    let bestSum = 0, bestCount = 0, saveSum = 0, saveCount = 0;
    let utilSum = 0, utilCount = 0, sheetsSum = 0, sheetsCount = 0;
    let withResult = 0, unplacedSum = 0, withUnplaced = 0;
    let online = 0, desktop = 0;
    const optCounts = {};
    const sheetSizeCounts = {};
    const runsByDay = {};
    const savesByDay = {};
    const utilByDay = {};
    const byCountry = {};

    for (const x of allRuns) {
        totalDxfs += num(x.dxf_count);
        const source = x.source === 'desktop' ? 'desktop' : (x.source === 'online' ? 'online' : '');
        if (source === 'desktop') desktop++;
        if (source === 'online') online++;
        const country = String(x.country || '').toUpperCase();
        if (country) byCountry[country] = (byCountry[country] || 0) + 1;
        const day = dayFromRun(x);
        if (day) runsByDay[day] = (runsByDay[day] || 0) + 1;

        if (num(x.best_solution_time_sec) > 0) { bestSum += num(x.best_solution_time_sec); bestCount++; }
        if (x.saved === true) {
            saved++;
            if (day) savesByDay[day] = (savesByDay[day] || 0) + 1;
            if (num(x.total_time_to_save_sec) > 0) { saveSum += num(x.total_time_to_save_sec); saveCount++; }
        }
        if (num(x.final_utilization) > 0) {
            const u = num(x.final_utilization);
            utilSum += u; utilCount++;
            if (day) {
                if (!utilByDay[day]) utilByDay[day] = { sum: 0, count: 0 };
                utilByDay[day].sum += u;
                utilByDay[day].count++;
            }
        }
        if (num(x.sheets_used) > 0) { sheetsSum += num(x.sheets_used); sheetsCount++; }
        if (x.placed_count !== undefined || x.unplaced_count !== undefined) {
            withResult++;
            const up = num(x.unplaced_count);
            unplacedSum += up;
            if (up > 0) withUnplaced++;
        }
        if (x.optimization_type) optCounts[x.optimization_type] = (optCounts[x.optimization_type] || 0) + 1;
        if (x.sheet_width && x.sheet_height) {
            const key = `${Math.round(num(x.sheet_width))}×${Math.round(num(x.sheet_height))}`;
            sheetSizeCounts[key] = (sheetSizeCounts[key] || 0) + 1;
        }
    }

    const solverDays = Array.from(new Set([...Object.keys(runsByDay), ...Object.keys(utilByDay)])).sort();
    const countryEntries = Object.entries(byCountry).sort((a, b) => b[1] - a[1]).slice(0, 10);
    const optEntries = Object.entries(optCounts).sort((a, b) => b[1] - a[1]);
    const sheetEntries = Object.entries(sheetSizeCounts).sort((a, b) => b[1] - a[1]);

    return {
        metrics: { totalViews, totalDownloads, totalTimeSpent, labels, viewsData, downloadsData },
        solver: {
            total: allRuns.length,
            dxfs: totalDxfs,
            saved,
            avgBest: bestCount ? bestSum / bestCount : 0,
            avgSave: saveCount ? saveSum / saveCount : 0,
            avgUtil: utilCount ? utilSum / utilCount : 0,
            avgSheets: sheetsCount ? sheetsSum / sheetsCount : 0,
            withResult,
            unplacedSum,
            withUnplaced,
            online,
            desktop,
            legacy: Math.max(0, allRuns.length - online - desktop),
            solverDays,
            runsSeries: solverDays.map((d) => runsByDay[d] || 0),
            savesSeries: solverDays.map((d) => savesByDay[d] || 0),
            utilSeries: solverDays.map((d) => utilByDay[d] ? +(utilByDay[d].sum / utilByDay[d].count).toFixed(1) : null),
            optEntries,
            topSheet: sheetEntries.length ? sheetEntries[0][0] : '',
            countryLabels: countryEntries.map(([k]) => k),
            countryCounts: countryEntries.map(([, v]) => v),
            countryCount: Object.keys(byCountry).length
        }
    };
}

async function handlePost(req, res) {
    const body = req.body || {};
    const authHeader = req.headers.authorization || '';
    if (authHeader) {
        const adminUser = await requireAdminUser(req);
        const { db } = getAdminServices();
        const op = String(body.adminOp || '').trim();

        if (op === 'logAudit') {
            await db.collection('audit_log').add({
                ts: new Date(),
                dateISO: new Date().toISOString(),
                action: String(body.action || '').slice(0, 80),
                description: String(body.description || '').slice(0, 500),
                admin: String(body.admin || adminUser.email || '').slice(0, 160),
                details: body.details || null,
            });
            return json(res, 200, { ok: true });
        }

        if (op === 'deleteMessage') {
            const id = String(body.id || '').trim();
            if (!id) return json(res, 400, { error: 'Falta id.' });
            await db.collection('messages').doc(id).delete();
            return json(res, 200, { ok: true, deleted: 1 });
        }

        if (op === 'deleteOldMessages') {
            const days = Math.max(1, Math.min(3650, parseInt(body.days, 10) || 30));
            const cutoff = Date.now() - days * 24 * 60 * 60 * 1000;
            const snap = await db.collection('messages').get();
            const refs = [];
            snap.forEach((doc) => {
                const x = doc.data() || {};
                const t = x.timestamp && x.timestamp.toDate ? x.timestamp.toDate().getTime() : 0;
                if (t && t < cutoff) refs.push(doc.ref);
            });
            for (let i = 0; i < refs.length; i += 450) {
                const batch = db.batch();
                refs.slice(i, i + 450).forEach((ref) => batch.delete(ref));
                await batch.commit();
            }
            return json(res, 200, { ok: true, deleted: refs.length });
        }

        return json(res, 400, { error: 'Operacion admin invalida.' });
    }

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
    const adminUser = await requireAdminUser(req);
    const { db } = getAdminServices();

    if (String(req.query.adminCheck || '') === '1') {
        const snap = await db.collection('users').doc(adminUser.uid).get();
        const data = snap.exists ? snap.data() : {};
        return json(res, 200, {
            ok: true,
            uid: adminUser.uid,
            email: adminUser.email || data.email || '',
            role: data.role || '',
            isAdmin: data.isAdmin === true
        });
    }

    if (String(req.query.adminMessages || '') === '1') {
        const snap = await db.collection('messages').orderBy('timestamp', 'desc').limit(1000).get();
        return json(res, 200, { ok: true, items: snap.docs.map(cleanMessageDoc) });
    }

    if (String(req.query.adminAudit || '') === '1') {
        const pageSize = Math.min(Math.max(parseInt(req.query.limit, 10) || 300, 1), 500);
        const offset = Math.max(parseInt(req.query.offset, 10) || 0, 0);
        const snap = await db.collection('audit_log').orderBy('ts', 'desc').offset(offset).limit(pageSize + 1).get();
        const docs = snap.docs.map(cleanAuditDoc);
        return json(res, 200, {
            ok: true,
            items: docs.slice(0, pageSize),
            nextOffset: docs.length > pageSize ? offset + pageSize : null
        });
    }

    if (String(req.query.adminMetrics || '') === '1') {
        const [metricsSnap, runsSnap] = await Promise.all([
            db.collection('metrics').get(),
            db.collection('solver_runs').orderBy('timestamp', 'desc').limit(5000).get()
        ]);
        return json(res, 200, {
            ok: true,
            ...buildMetricsResponse(metricsSnap.docs, runsSnap.docs)
        });
    }

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
            ua: x.ua || '',
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
