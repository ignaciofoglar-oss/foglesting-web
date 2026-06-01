import { getGithubFile, putGithubFile, requireReleaseAuth } from './github-content.js';

function json(res, status, payload) {
    res.status(status).json(payload);
}

export default async function handler(req, res) {
    if (req.method !== 'POST') {
        return json(res, 405, { error: 'Method Not Allowed' });
    }

    if (!requireReleaseAuth(req, res)) return;

    const { version, publicListed } = req.body || {};
    if (!version || typeof publicListed !== 'boolean') {
        return json(res, 400, { error: 'Datos de visibilidad incompletos.' });
    }

    try {
        const releasesFile = await getGithubFile('admin-releases/releases.json');
        const releases = JSON.parse(releasesFile.content);
        let found = false;

        releases.candidates = (releases.candidates || []).map((candidate) => {
            if (candidate.version !== version) return candidate;
            found = true;
            return { ...candidate, public_listed: publicListed };
        });

        if (!found) {
            return json(res, 404, { error: 'Version no encontrada.' });
        }

        await putGithubFile(
            'admin-releases/releases.json',
            Buffer.from(`${JSON.stringify(releases, null, 2)}\n`, 'utf8').toString('base64'),
            `${publicListed ? 'Show' : 'Hide'} ${version} in public release history`
        );

        return json(res, 200, { success: true, version, publicListed });
    } catch (error) {
        console.error('update-release-visibility error:', error);
        return json(res, 500, { error: error.message || 'Error actualizando visibilidad.' });
    }
}
