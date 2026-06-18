import { getAdminServices, verifyUser } from './firebase-admin.js';

// Producto que se cobra (debe coincidir con lo que valida el programa de escritorio).
const PRODUCT = 'Sheet Metal Nesting';

function json(res, status, payload) { res.status(status).json(payload); }

// Activa (o renueva) la licencia del usuario por N dias.
// Reutilizado tanto por el modo de prueba (simular pago) como, mas adelante,
// por el webhook real de Mercado Pago / Stripe.
async function activateForUid(db, uid, email, { plan, days, gateway, paymentId, amount, currency, source }) {
    const now = new Date();
    const expires = new Date(now.getTime() + days * 24 * 60 * 60 * 1000);

    const license = {
        status: 'active',
        plan: plan || 'mensual',
        gateway: gateway || 'test',
        activatedAt: now.toISOString(),
        expiresAt: expires.toISOString(),
        lastPaymentId: paymentId || null,
    };

    // El programa de escritorio valida users/{uid}.licenses["Sheet Metal Nesting"].status === 'active'
    await db.collection('users').doc(uid).set({
        licenses: { [PRODUCT]: license },
    }, { merge: true });

    // Registro del pago (para el panel de facturacion).
    await db.collection('payments').add({
        uid,
        email: email || null,
        product: PRODUCT,
        plan: license.plan,
        amount: Number(amount) || 0,
        currency: currency || 'ARS',
        gateway: license.gateway,
        status: 'approved',
        source: source || 'desconocido',
        paymentId: paymentId || null,
        createdAt: now.toISOString(),
        timestamp: now,
        expiresAt: license.expiresAt,
    });

    return license;
}

export default async function handler(req, res) {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');
    res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
    if (req.method === 'OPTIONS') return res.status(204).end();
    if (req.method !== 'POST') return json(res, 405, { error: 'Method Not Allowed' });

    try {
        const decoded = await verifyUser(req);
        const { db } = getAdminServices();

        // Config de facturacion (precio, modo de prueba). El admin la controla.
        const cfgSnap = await db.collection('settings').doc('billing').get();
        const cfg = cfgSnap.exists ? cfgSnap.data() : {};
        const testMode = cfg.testMode === true;

        const body = req.body || {};
        const wantSimulate = body.simulate === true;

        // SEGURIDAD: la activacion directa (simular pago) SOLO funciona con el
        // modo de prueba encendido desde el admin. Asi nadie puede auto-activarse
        // gratis cuando esto este en produccion; ahi la activacion vendra del
        // webhook real de la pasarela (con validacion del pago).
        if (!wantSimulate || !testMode) {
            return json(res, 403, {
                error: testMode
                    ? 'Falta indicar simulate:true.'
                    : 'El modo de prueba esta apagado. La activacion debe venir de un pago real.',
            });
        }

        const days = Number(cfg.periodDays) > 0 ? Number(cfg.periodDays) : 30;
        const license = await activateForUid(db, decoded.uid, decoded.email, {
            plan: cfg.plan || 'mensual',
            days,
            gateway: 'test',
            paymentId: 'sim_' + Date.now(),
            amount: cfg.priceArs || 0,
            currency: 'ARS',
            source: 'simulado',
        });

        return json(res, 200, { ok: true, simulated: true, license });
    } catch (e) {
        return json(res, e.statusCode || 500, { error: e.message || 'Error interno.' });
    }
}

export { activateForUid, PRODUCT };
