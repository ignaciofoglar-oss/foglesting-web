// i18n logic for Foglesting
document.addEventListener('DOMContentLoaded', () => {
    const langToggle = document.getElementById('lang-toggle');
    if (!langToggle) return;

    // Check saved language or default to 'en'
    let currentLang = localStorage.getItem('foglesting_lang') || 'en';
    
    function applyTranslations(lang) {
        if (!translations || !translations[lang]) return;
        
        const elements = document.querySelectorAll('[data-i18n]');
        elements.forEach(el => {
            const key = el.getAttribute('data-i18n');
            if (translations[lang][key]) {
                el.innerHTML = translations[lang][key];
            }
        });
        
        // Update document lang attribute
        document.documentElement.lang = lang;
        
        // Update toggle button text (shows the OTHER language to switch to)
        langToggle.textContent = lang === 'es' ? 'EN' : 'ES';
        
        // Save preference
        localStorage.setItem('foglesting_lang', lang);
    }

    // Initial apply
    applyTranslations(currentLang);

    // Toggle event
    langToggle.addEventListener('click', (e) => {
        e.preventDefault();
        currentLang = currentLang === 'es' ? 'en' : 'es';
        applyTranslations(currentLang);
    });
});
