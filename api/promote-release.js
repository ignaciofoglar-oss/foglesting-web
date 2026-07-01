import { getGithubFile, putGithubFile } from '../lib/github-content.js';
import { requireAdminUser } from '../lib/firebase-admin.js';

// Dominio publico de las descargas (el mismo que usamos al publicar a mano).
const DEFAULT_PUBLIC_BASE_URL = 'https://foglesting.com';

function json(res, status, payload) {
    res.status(status).json(payload);
}

function cleanFileName(value) {
    const name = String(value || '').trim();
    if (!/^[a-zA-Z0-9._-]+\.(exe|zip)$/i.test(name)) return '';
    return name;
}

function cleanSourcePath(value, fallbackFile) {
    const path = String(value || '').trim();
    const fallback = `/admin-releases/${fallbackFile}`;
    const match = path.match(/^\/(admin-releases|downloads)\/([a-zA-Z0-9._-]+\.(exe|zip))$/i);
    return match ? path : fallback;
}

function encodeBase64(buffer) {
    return Buffer.from(buffer).toString('base64');
}

export default async function handler(req, res) {
    if (req.method !== 'POST') {
        return json(res, 405, { error: 'Method Not Allowed' });
    }

    try {
        await requireAdminUser(req);
    } catch (error) {
        return json(res, error.statusCode || 401, { error: error.message || 'No autorizado.' });
    }
    if (!process.env.GITHUB_TOKEN) {
        return json(res, 500, { error: 'Falta GITHUB_TOKEN en Vercel.' });
    }

    const publicBaseUrl = (process.env.PUBLIC_BASE_URL || DEFAULT_PUBLIC_BASE_URL).replace(/\/$/, '');
    const { version, name, sourceFile, sourcePath, publicFile, notes } = req.body || {};

    const sourceName = cleanFileName(sourceFile);
    const publicName = cleanFileName(publicFile || sourceFile);
    const safeSourcePath = cleanSourcePath(sourcePath, sourceName);
    if (!version || !sourceName || !publicName) {
        return json(res, 400, { error: 'Datos de release incompletos.' });
    }

    try {
        const origin = `https://${req.headers.host}`;
        const sourceUrl = `${origin}${safeSourcePath}`;
        const exeResponse = await fetch(sourceUrl);
        if (!exeResponse.ok) {
            return json(res, 404, { error: `No se pudo leer ${sourceUrl}` });
        }

        const exeBuffer = await exeResponse.arrayBuffer();
        await putGithubFile(
            `downloads/${publicName}`,
            encodeBase64(exeBuffer),
            `Publish ${name || `FOGLESTING ${version}`} executable`
        );

        const manifest = {
            version,
            name: name || `FOGLESTING V${version}`,
            download_url: `${publicBaseUrl}/downloads/${publicName}`,
            notes: notes || `${name || `FOGLESTING V${version}`} disponible. Descarga directa del ejecutable.`
        };

        await putGithubFile(
            'version.json',
            Buffer.from(`${JSON.stringify(manifest, null, 2)}\n`, 'utf8').toString('base64'),
            `Release ${manifest.name}`
        );

        try {
            const releasesFile = await getGithubFile('admin-releases/releases.json');
            const releases = JSON.parse(releasesFile.content);
            releases.active_public_version = version;
            releases.candidates = (releases.candidates || []).map((candidate) => {
                if (candidate.version !== version) return candidate;
                return {
                    ...candidate,
                    public_listed: true,
                    download_url: `/downloads/${publicName}`,
                    source_path: `/downloads/${publicName}`
                };
            });
            await putGithubFile(
                'admin-releases/releases.json',
                Buffer.from(`${JSON.stringify(releases, null, 2)}\n`, 'utf8').toString('base64'),
                `Update release manifest for ${manifest.name}`
            );
        } catch (manifestError) {
            console.error('Could not update releases manifest:', manifestError);
        }

        return json(res, 200, {
            success: true,
            version,
            publicFile: publicName,
            downloadUrl: manifest.download_url
        });
    } catch (error) {
        console.error('promote-release error:', error);
        return json(res, 500, { error: error.message || 'Error publicando release.' });
    }
}
