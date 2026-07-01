import admin from 'firebase-admin';
import { getAdminServices, requireAdminUser } from '../lib/firebase-admin.js';

// Telemetría web (un solo endpoint para no pasar el limite de funciones de Vercel):
//   POST  -> recibe eventos del tracker del sitio (track.js), agrega geo por IP
//            y los guarda en Firestore (coleccion web_events). Auth: x-fogl-key
//            (header) o body.k = DIAGNOSTIC_UPLOAD_KEY.
//         -> tambien: POST {metric:'page_view'|'time_spent'} para contar visitas
//            del lado del servidor (las reglas de Firestore bloquean el cliente).
//   GET   -> lista los ultimos eventos para el panel admin (solo admin).
//         -> tambien: GET ?download=<archivo.exe> cuenta la descarga y redirige
//            al .exe (lo usan el boton de la web y el cartel de actualizacion).

function json(res, status, payload) { res.status(status).json(payload); }
function decode(v) { try { return decodeURIComponent(String(v || '')); } catch { return String(v || ''); } }

function metricDayStr() {
    const d = new Date();
    return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
}
// Solo permitimos redirigir a un .exe dentro de /downloads (evita open-redirect).
function safeExe(f) {
    const name = String(f || '').trim();
    return /^[A-Za-z0-9._-]+\.exe$/.test(name) ? name : '';
}
async function incMetric(fields) {
    const { db } = getAdminServices();
    await db.collection('metrics').doc(metricDayStr()).set(
        { ...fields, date: metricDayStr() },
        { merge: true }
    );
}

// GET ?download=<archivo.exe>: cuenta la descarga (Admin SDK, ignora las reglas
// que bloquean la escritura del cliente) y redirige al .exe real. Publico.
async function handleDownloadRedirect(req, res) {
    const file = safeExe(req.query.download);
    const target = file ? `/downloads/${file}` : '/#descargar';
    try {
        await incMetric({ downloads: admin.firestore.FieldValue.increment(1) });
    } catch (e) {
        // Ignorado a proposito: la descarga tiene que funcionar igual.
    }
    res.setHeader('Cache-Control', 'no-store');
    res.redirect(302, target);
}

// POST {metric:'page_view'|'time_spent'|'download', seconds?}: metrica publica de
// la web contada del lado del servidor. Reemplaza las escrituras del cliente.
async function handleMetricPost(res, body) {
    const type = String(body.metric || '');
    const seconds = Math.max(0, Math.min(86400, parseInt(body.seconds, 10) || 0));
    const inc = {};
    if (type === 'page_view') inc.page_views = admin.firestore.FieldValue.increment(1);
    else if (type === 'download') inc.downloads = admin.firestore.FieldValue.increment(1);
    else if (type === 'time_spent') {
        if (seconds <= 0) return json(res, 200, { ok: true, skipped: true });
        inc.time_spent = admin.firestore.FieldValue.increment(seconds);
    } else return json(res, 400, { error: 'metric invalido' });
    try {
        await incMetric(inc);
        return json(res, 200, { ok: true });
    } catch (e) {
        return json(res, e.statusCode || 500, { error: e.message || 'Error interno.' });
    }
}

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

function cleanUserDoc(doc) {
    const x = doc.data() || {};
    const license = x.licenses && x.licenses['Sheet Metal Nesting'] ? x.licenses['Sheet Metal Nesting'] : {};
    return {
        id: doc.id,
        uid: doc.id,
        name: x.name || '',
        email: x.email || '',
        role: x.role === 'admin' || x.isAdmin === true ? 'admin' : 'user',
        isAdmin: x.role === 'admin' || x.isAdmin === true,
        hasActiveLicense: license.status === 'active' || x.hasActiveLicense === true,
        licenseStatus: license.status || (x.hasActiveLicense ? 'active' : 'inactive'),
        createdAtISO: isoFrom(x.createdAt),
        lastLoginISO: isoFrom(x.lastLogin)
    };
}

function buildMetricsResponse(metricsDocs, runDocs, webEventDocs = []) {
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

    if (totalViews === 0 && webEventDocs.length > 0) {
        const byDay = {};
        for (const doc of webEventDocs) {
            const x = doc.data() || {};
            const iso = x.serverISO || isoFrom(x.ts) || x.tsClientISO || '';
            const day = iso ? iso.slice(0, 10) : '';
            if (!day) continue;
            if (!byDay[day]) byDay[day] = { page_views: 0, downloads: 0, time_spent: 0 };
            if (x.event === 'page_view') byDay[day].page_views++;
            if (x.event === 'download') byDay[day].downloads++;
            if (x.event === 'page_leave') byDay[day].time_spent += Math.round(num(x.ms) / 1000);
        }
        labels.length = 0;
        viewsData.length = 0;
        downloadsData.length = 0;
        totalViews = 0;
        totalDownloads = 0;
        totalTimeSpent = 0;
        for (const day of Object.keys(byDay).sort()) {
            const row = byDay[day];
            labels.push(day);
            viewsData.push(row.page_views);
            downloadsData.push(row.downloads);
            totalViews += row.page_views;
            totalDownloads += row.downloads;
            totalTimeSpent += row.time_spent;
        }
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
    // Metrica publica de la web (page_view / time_spent), sin auth.
    if (body.metric) {
        return handleMetricPost(res, body);
    }
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

        if (op === 'updateUserRole') {
            const uid = String(body.uid || '').trim();
            const role = String(body.role || '') === 'admin' ? 'admin' : 'user';
            if (!uid) return json(res, 400, { error: 'Falta uid.' });
            await db.collection('users').doc(uid).set({
                role,
                isAdmin: role === 'admin',
                updatedAt: new Date()
            }, { merge: true });
            return json(res, 200, { ok: true, uid, role });
        }

        if (op === 'setUserLicense') {
            const uid = String(body.uid || '').trim();
            const status = String(body.status || '') === 'active' ? 'active' : 'inactive';
            if (!uid) return json(res, 400, { error: 'Falta uid.' });
            await db.collection('users').doc(uid).set({
                licenses: {
                    'Sheet Metal Nesting': {
                        status,
                        updatedAt: new Date()
                    }
                },
                hasActiveLicense: status === 'active',
                updatedAt: new Date()
            }, { merge: true });
            return json(res, 200, { ok: true, uid, status });
        }

        if (op === 'setAppConfig') {
            await db.collection('settings').doc('app_config').set({
                requireLicense: body.requireLicense === true,
                updatedAt: new Date()
            }, { merge: true });
            return json(res, 200, { ok: true, requireLicense: body.requireLicense === true });
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
    // Descarga publica contada (antes de exigir admin).
    if (req.query.download) {
        return handleDownloadRedirect(req, res);
    }
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

    if (String(req.query.adminUsers || '') === '1') {
        const snap = await db.collection('users').orderBy('createdAt', 'desc').limit(2000).get();
        return json(res, 200, { ok: true, items: snap.docs.map(cleanUserDoc) });
    }

    if (String(req.query.adminSettings || '') === '1') {
        const snap = await db.collection('settings').doc('app_config').get();
        const data = snap.exists ? snap.data() : {};
        return json(res, 200, { ok: true, requireLicense: data.requireLicense === true });
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

    if (String(req.query.adminSessionEvents || '')) {
        const sid = String(req.query.adminSessionEvents);
        const snap = await db.collection('session_events').where('sessionId', '==', sid).limit(1000).get();
        const items = snap.docs.map((d) => {
            const x = d.data() || {};
            return {
                action: x.action || '',
                detail: x.detail || '',
                tsClientISO: x.tsClientISO || '',
                serverISO: x.serverISO || isoFrom(x.ts) || ''
            };
        });
        return json(res, 200, { ok: true, items });
    }

    if (String(req.query.adminMetrics || '') === '1') {
        const [metricsSnap, runsSnap, webSnap] = await Promise.all([
            db.collection('metrics').get(),
            db.collection('solver_runs').orderBy('timestamp', 'desc').limit(5000).get(),
            db.collection('web_events').orderBy('ts', 'desc').limit(5000).get()
        ]);
        return json(res, 200, {
            ok: true,
            ...buildMetricsResponse(metricsSnap.docs, runsSnap.docs, webSnap.docs)
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
