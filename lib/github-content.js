const DEFAULT_REPO = 'ignaciofoglar-oss/foglesting-web';
const DEFAULT_BRANCH = 'main';

function githubConfig() {
    return {
        token: process.env.GITHUB_TOKEN,
        repo: process.env.GITHUB_REPO || DEFAULT_REPO,
        branch: process.env.GITHUB_BRANCH || DEFAULT_BRANCH
    };
}

function githubPath(path) {
    return encodeURIComponent(path).replace(/%2F/g, '/');
}

async function githubRequest(path, options = {}) {
    const { token, repo } = githubConfig();
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

async function getGithubFile(path) {
    const { branch } = githubConfig();
    const body = await githubRequest(`/contents/${githubPath(path)}?ref=${encodeURIComponent(branch)}`);
    const content = body.content ? Buffer.from(body.content, 'base64').toString('utf8') : '';
    return { sha: body.sha || null, content };
}

async function putGithubFile(path, contentBase64, message) {
    const { branch } = githubConfig();
    let sha = null;
    try {
        const existing = await getGithubFile(path);
        sha = existing.sha;
    } catch {
        sha = null;
    }

    const body = {
        message,
        content: contentBase64,
        branch
    };
    if (sha) body.sha = sha;

    return githubRequest(`/contents/${githubPath(path)}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });
}

export {
    githubRequest,
    getGithubFile,
    putGithubFile
};
