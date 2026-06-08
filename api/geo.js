// Devuelve la geolocalizacion aproximada del visitante (pais/ciudad/region)
// a partir de los headers que agrega Vercel en el edge. Sin datos sensibles.
function decode(v) {
    try { return decodeURIComponent(String(v || '')); } catch { return String(v || ''); }
}

export default async function handler(req, res) {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
    res.setHeader('Cache-Control', 'no-store');
    if (req.method === 'OPTIONS') return res.status(204).end();

    const country = String(req.headers['x-vercel-ip-country'] || '').slice(0, 8);
    const city = decode(req.headers['x-vercel-ip-city']).slice(0, 80);
    const region = String(req.headers['x-vercel-ip-country-region'] || '').slice(0, 16);

    return res.status(200).json({ country, city, region });
}
