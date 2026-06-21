import { getAdminServices } from '../lib/firebase-admin.js';

export default async function handler(req, res) {
    if (req.method !== 'POST') {
        return res.status(405).json({ error: 'Method Not Allowed' });
    }

    // Body defensivo: sin body valido no crashear, y limitar largos para
    // que nadie pueda mandar mensajes gigantes por WhatsApp.
    const b = (req.body && typeof req.body === 'object') ? req.body : {};
    const name = String(b.name || '').slice(0, 80);
    const email = String(b.email || '').slice(0, 120);
    const type = String(b.type || '').slice(0, 40);
    const message = String(b.message || '').slice(0, 1000);
    if (!name || !message) {
        return res.status(400).json({ error: 'Faltan nombre o mensaje.' });
    }

    // Rate limit: 1 envio por minuto por IP, guardado en Firestore.
    // Evita que alguien spamee el WhatsApp con el formulario de feedback.
    const ip = String(req.headers['x-forwarded-for'] || req.headers['x-real-ip'] || '')
        .split(',')[0].trim() || 'unknown';
    try {
        const { db } = getAdminServices();
        const key = ip.replace(/[^a-zA-Z0-9.:_-]/g, '_').slice(0, 100) || 'unknown';
        const ref = db.collection('rate_limits').doc(`feedback_${key}`);
        const snap = await ref.get();
        const now = Date.now();
        const last = snap.exists ? Number(snap.data().lastMs || 0) : 0;
        if (now - last < 60 * 1000) {
            return res.status(429).json({ error: 'Esperá un minuto antes de enviar otro mensaje.' });
        }
        await ref.set({ lastMs: now, updatedAt: new Date().toISOString() });
    } catch (rateErr) {
        // Si Firestore falla, no bloqueamos el feedback legitimo.
        console.error('rate-limit feedback:', rateErr);
    }

    // Obtenemos las credenciales desde las variables de entorno de Vercel
    const phone = process.env.CALLMEBOT_PHONE;
    const apikey = process.env.CALLMEBOT_API_KEY;

    if (!phone || !apikey) {
        console.error('Faltan variables de entorno en Vercel (CALLMEBOT_PHONE o CALLMEBOT_API_KEY).');
        return res.status(500).json({ error: 'Falta configuración en el servidor' });
    }

    // Formateamos el mensaje para WhatsApp (con negritas)
    const text = `*NUEVO MENSAJE DE FOGLESTING*\n\n*Nombre:* ${name}\n*Email:* ${email}\n*Asunto:* ${type}\n*Mensaje:* ${message}`;
    const encodedText = encodeURIComponent(text);
    
    const url = `https://api.callmebot.com/whatsapp.php?phone=${phone}&text=${encodedText}&apikey=${apikey}`;

    try {
        const response = await fetch(url);
        if (response.ok) {
            return res.status(200).json({ success: true, message: 'WhatsApp enviado correctamente.' });
        } else {
            const errorText = await response.text();
            console.error('Error de CallMeBot:', errorText);
            return res.status(500).json({ error: 'Error al comunicarse con CallMeBot' });
        }
    } catch (error) {
        console.error('Error enviando el fetch a CallMeBot:', error);
        return res.status(500).json({ error: error.message });
    }
}
