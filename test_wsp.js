async function test() {
    try {
        const response = await fetch('https://foglesting-web.vercel.app/api/whatsapp', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name: 'Bot', email: 'bot@test.com', type: 'Prueba', message: 'Testing API' })
        });
        const text = await response.text();
        console.log(`Status: ${response.status}`);
        console.log(`Body: ${text}`);
    } catch (e) {
        console.error(e);
    }
}
test();
