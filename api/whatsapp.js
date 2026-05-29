export default async function handler(req, res) {
    if (req.method !== 'POST') {
        return res.status(405).json({ error: 'Method Not Allowed' });
    }

    const { name, email, type, message } = req.body;
    
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
