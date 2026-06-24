import codecs
import re

# 1. Restore txt_13 in translations.js and index.html
with codecs.open('translations.js', 'r', encoding='utf-8') as f:
    t_content = f.read()

t_content = re.sub(
    r'"txt_13": "Cargá tus DXF y obtené el acomodo óptimo que minimiza el desperdicio de chapa."',
    r'"txt_13": "Usamos un <strong>Algoritmo Genético</strong> para calcular el mejor ordenamiento de tus piezas. Cargá tus DXF y obtené el acomodo óptimo que minimiza el desperdicio de chapa."',
    t_content
)
t_content = re.sub(
    r'"txt_13": "Load your DXF files and get the optimal nesting that minimizes material waste."',
    r'"txt_13": "We use a <strong>Genetic Algorithm</strong> to calculate the best layout for your parts. Load your DXF files and get the optimal nesting that minimizes material waste."',
    t_content
)
with codecs.open('translations.js', 'w', encoding='utf-8') as f:
    f.write(t_content)

with codecs.open('index.html', 'r', encoding='utf-8') as f:
    i_content = f.read()
i_content = i_content.replace(
    '<p class="hero-tagline" data-i18n="txt_13">Cargá tus DXF y obtené el acomodo óptimo que minimiza el desperdicio de chapa.</p>',
    '<p class="hero-tagline" data-i18n="txt_13">Usamos un <strong>Algoritmo Genético</strong> para calcular el mejor ordenamiento de tus piezas. Cargá tus DXF y obtené el acomodo óptimo que minimiza el desperdicio de chapa.</p>'
)
with codecs.open('index.html', 'w', encoding='utf-8') as f:
    f.write(i_content)


# 2. Modify online/index.html
with codecs.open('online/index.html', 'r', encoding='utf-8') as f:
    html = f.read()

# Make it one box
html = html.replace('class="glass-panel upload-section"', 'class="upload-section"')
html = html.replace('class="glass-panel preview-panel"', 'class="preview-panel"')
html = html.replace('class="controls glass-panel"', 'class="controls"')

# Remove WASM status & footer
wasm_pattern = re.compile(r'<!-- WASM Status -->.*?</div>', re.DOTALL)
html = re.sub(wasm_pattern, '', html)
footer_pattern = re.compile(r'<footer>.*?</footer>', re.DOTALL)
html = re.sub(footer_pattern, '', html)

with codecs.open('online/index.html', 'w', encoding='utf-8') as f:
    f.write(html)


# 3. Modify online/style.css
with codecs.open('online/style.css', 'r', encoding='utf-8') as f:
    css = f.read()

btn_primary_pattern = re.compile(r'\.btn-primary\s*{[^}]*}', re.DOTALL)
css = re.sub(btn_primary_pattern, '.btn-primary { background: var(--fire); color: white; }', css)

btn_primary_hover_pattern = re.compile(r'\.btn-primary:hover\s*{[^}]*}', re.DOTALL)
css = re.sub(btn_primary_hover_pattern, '.btn-primary:hover { background: var(--fire-dark); transform: translateY(-2px); }', css)

run_btn_pattern = re.compile(r'\.run-btn\s*{[^}]*}', re.DOTALL)
css = re.sub(run_btn_pattern, '.run-btn { width: 100%; padding: 1rem; font-size: 1.1rem; background: var(--fire); color: white; border: none; border-radius: var(--radius); font-family: var(--font-body); font-weight: 700; letter-spacing: 0.5px; }', css)

run_btn_hover_pattern = re.compile(r'\.run-btn:hover:not\(:disabled\)\s*{[^}]*}', re.DOTALL)
css = re.sub(run_btn_hover_pattern, '.run-btn:hover:not(:disabled) { transform: translateY(-2px); background: var(--fire-dark); }', css)

upload_hover_pattern = re.compile(r'\.upload-section:hover\s*{[^}]*}', re.DOTALL)
css = re.sub(upload_hover_pattern, '.upload-section:hover { background: rgba(232,91,46,0.04); transform: translateY(-2px); }', css)

dashed_pattern = re.compile(r'border:\s*1\.5px\s*dashed\s*rgba[^;]+;')
css = re.sub(dashed_pattern, 'border: none;', css)

css += '''
/* Remove border radius globally */
button:not(.powered-badge), 
[class*="btn"]:not(.powered-badge), 
.nav-login:not(.powered-badge), 
.nav-cta:not(.powered-badge), 
.lang-btn:not(.powered-badge) {
    border-radius: 0 !important;
}

.powered-badge {
    border-radius: 999px !important;
}
'''

with codecs.open('online/style.css', 'w', encoding='utf-8') as f:
    f.write(css)


# 4. Fix app.js click handler
with codecs.open('online/app.js', 'r', encoding='utf-8') as f:
    app_js = f.read()

click_handler = """
    document.body.addEventListener('click', (e) => {
        if (e.target.classList && e.target.classList.contains('info-tip')) {
            if (help.classList.contains('active') && help.textContent === (e.target.dataset.tip || t('helpDefault'))) {
                help.textContent = t('helpDefault');
                help.classList.remove('active');
            } else {
                help.textContent = e.target.dataset.tip || t('helpDefault');
                help.classList.add('active');
            }
        }
    });
"""

if "document.body.addEventListener('click'" not in app_js:
    app_js = app_js.replace(
        "document.body.addEventListener('mouseout'",
        click_handler + "\n    document.body.addEventListener('mouseout'"
    )

with codecs.open('online/app.js', 'w', encoding='utf-8') as f:
    f.write(app_js)
