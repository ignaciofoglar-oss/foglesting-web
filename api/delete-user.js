import { getAdminServices, requireAdminUser } from './firebase-admin.js';

function json(res, status, payload) {
    res.status(status).json(payload);
}

export default async function handler(req, res) {
    if (req.method !== 'POST') {
        res.setHeader('Allow', 'POST');
        return json(res, 405, { error: 'Metodo no permitido.' });
    }

    try {
        const adminUser = await requireAdminUser(req);
        const targetUid = String(req.body?.uid || '').trim();

        if (!targetUid) {
            return json(res, 400, { error: 'Falta el uid del usuario.' });
        }
        if (targetUid === adminUser.uid) {
            return json(res, 400, { error: 'No podes borrar tu propia cuenta desde esta sesion.' });
        }

        const { auth, db } = getAdminServices();

        try {
            await auth.deleteUser(targetUid);
        } catch (error) {
            if (error.code !== 'auth/user-not-found') throw error;
        }

        const batch = db.batch();
        batch.delete(db.collection('users').doc(targetUid));

        const deviceLogins = await db.collection('device_logins').where('uid', '==', targetUid).get();
        deviceLogins.forEach((docSnap) => batch.delete(docSnap.ref));

        await batch.commit();

        return json(res, 200, {
            ok: true,
            deletedUid: targetUid,
            removedDeviceLogins: deviceLogins.size
        });
    } catch (error) {
        console.error('delete-user failed:', error);
        return json(res, error.statusCode || 500, {
            error: error.message || 'No se pudo borrar el usuario.'
        });
    }
}
