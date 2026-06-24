import re
import codecs

# 1. Modify index.html
with codecs.open('index.html', 'r', encoding='utf-8') as f:
    html = f.read()

# Remove glass-panel classes from sections to make it blend and look like one box with the background
html = html.replace('class="glass-panel upload-section"', 'class="upload-section"')
html = html.replace('class="glass-panel preview-panel"', 'class="preview-panel"')
html = html.replace('class="controls glass-panel"', 'class="controls"')

# Wrap main content in a single glass-panel?
# The user said: "que sea todo una sola caja no que esten las 3 separadas y que sea uno con el fondo no que este separado"
# Removing the glass-panel class effectively removes the borders and background, making it blend with the page background.

# Remove the WASM status
wasm_pattern = re.compile(r'<!-- WASM Status -->.*?</div>', re.DOTALL)
html = re.sub(wasm_pattern, '', html)

# Remove the footer
footer_pattern = re.compile(r'<footer>.*?</footer>', re.DOTALL)
html = re.sub(footer_pattern, '', html)

with codecs.open('index.html', 'w', encoding='utf-8') as f:
    f.write(html)

# 2. Modify style.css
with codecs.open('style.css', 'r', encoding='utf-8') as f:
    css = f.read()

# Remove glow/gradients from buttons
btn_primary_pattern = re.compile(r'\.btn-primary\s*{[^}]*}', re.DOTALL)
css = re.sub(btn_primary_pattern, '.btn-primary { background: var(--fire); color: white; }', css)

btn_primary_hover_pattern = re.compile(r'\.btn-primary:hover\s*{[^}]*}', re.DOTALL)
css = re.sub(btn_primary_hover_pattern, '.btn-primary:hover { background: var(--fire-dark); transform: translateY(-2px); }', css)

run_btn_pattern = re.compile(r'\.run-btn\s*{[^}]*}', re.DOTALL)
css = re.sub(run_btn_pattern, '.run-btn { width: 100%; padding: 1rem; font-size: 1.1rem; background: var(--fire); color: white; border: none; border-radius: var(--radius); font-family: var(--font-body); font-weight: 700; letter-spacing: 0.5px; }', css)

run_btn_hover_pattern = re.compile(r'\.run-btn:hover:not\(:disabled\)\s*{[^}]*}', re.DOTALL)
css = re.sub(run_btn_hover_pattern, '.run-btn:hover:not(:disabled) { transform: translateY(-2px); background: var(--fire-dark); }', css)

# Fix upload section styling since we removed glass-panel
upload_hover_pattern = re.compile(r'\.upload-section:hover\s*{[^}]*}', re.DOTALL)
css = re.sub(upload_hover_pattern, '.upload-section:hover { border-color: rgba(232,91,46,0.55); background: rgba(232,91,46,0.04); transform: translateY(-2px); }', css)

with codecs.open('style.css', 'w', encoding='utf-8') as f:
    f.write(css)
