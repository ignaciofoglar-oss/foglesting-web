import admin from 'firebase-admin';
import { getAdminServices } from '../lib/firebase-admin.js';

// Cuenta la descarga del lado del servidor (Admin SDK, ignora las reglas de
// Firestore que bloquean las escrituras del cliente) y redirige al .exe real.
// Lo usan el boton de la web, la lista de versiones y el cartel de actualizacion
// del programa (version.json). Asi TODA descarga queda contada, no solo el click
// en el boton.

function todayStr() {
    const d = new Date();
    return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
}

// Solo permitimos redirigir a un .exe dentro de /downloads (evita open-redirect).
function safeFile(f) {
    const name = String(f || '').trim();
    return /^[A-Za-z0-9._-]+\.exe$/.test(name) ? name : '';
}

export default async function handler(req, res) {
    const file = safeFile(req.query.f);
    const target = file ? `/downloads/${file}` : '/#descargar';

    // Contar la descarga; nunca romper la redireccion por un error de metrica.
    try {
        const { db } = getAdminServices();
        const today = todayStr();
        await db.collection('metrics').doc(today).set({
            downloads: admin.firestore.FieldValue.increment(1),
            date: today,
        }, { merge: true });
    } catch (e) {
        // Ignorado a proposito: la descarga tiene que funcionar igual.
    }

    res.setHeader('Cache-Control', 'no-store');
    res.redirect(302, target);
}
