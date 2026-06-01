const DEFAULT_REPO = 'ignaciofoglar-oss/foglesting-web';
const DEFAULT_BRANCH = 'main';
const DEFAULT_PUBLIC_BASE_URL = 'https://foglesting-web.vercel.app';

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

async function githubRequest(path, options = {}) {
    const token = process.env.GITHUB_TOKEN;
    const repo = process.env.GITHUB_REPO || DEFAULT_REPO;
    const url = `https://api.github.com/repos/${repo}${path}`;
    const response = await fetch(url, {
        ...options,
        headers: {
            'Accept': 'application/vnd.github+json',
            'Authorization': `Bearer ${token}`,
            'X-GitHub-Api-Version': '2022-11-28',
            ...(options.headers || {})
        }
    });

    const text = await response.text();
    let body = {};
    try {
        body = text ? JSON.parse(text) : {};
    } catch {
        body = { raw: text };
    }

    if (!response.ok) {
        throw new Error(body.message || `GitHub HTTP ${response.status}`);
    }
    return body;
}

async function getExistingSha(repoPath, branch) {
    try {
        const body = await githubRequest(`/contents/${encodeURIComponent(repoPath).replace(/%2F/g, '/')}?ref=${encodeURIComponent(branch)}`);
        return body.sha || null;
    } catch (error) {
        if (String(error.message).includes('Not Found')) return null;
        return null;
    }
}

async function putGithubFile(repoPath, contentBase64, message, branch) {
    const sha = await getExistingSha(repoPath, branch);
    const body = {
        message,
        content: contentBase64,
        branch
    };
    if (sha) body.sha = sha;

    return githubRequest(`/contents/${encodeURIComponent(repoPath).replace(/%2F/g, '/')}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });
}

export default async function handler(req, res) {
    if (req.method !== 'POST') {
        return json(res, 405, { error: 'Method Not Allowed' });
    }

    const configuredSecret = process.env.RELEASE_ADMIN_SECRET;
    const providedSecret = req.headers['x-release-secret'];
    if (!configuredSecret || !providedSecret || providedSecret !== configuredSecret) {
        return json(res, 401, { error: 'Release no autorizado o RELEASE_ADMIN_SECRET no configurado.' });
    }

    if (!process.env.GITHUB_TOKEN) {
        return json(res, 500, { error: 'Falta GITHUB_TOKEN en Vercel.' });
    }

    const branch = process.env.GITHUB_BRANCH || DEFAULT_BRANCH;
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
            `Publish ${name || `FOGLESTING ${version}`} executable`,
            branch
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
            `Release ${manifest.name}`,
            branch
        );

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
