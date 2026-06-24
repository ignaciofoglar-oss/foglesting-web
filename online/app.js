// ====================================================
// Foglesting Online — WASM App Logic (Web Worker)
// Multi-file upload, live canvas preview, GA DNA
// ====================================================
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import { getFirestore, collection, addDoc, updateDoc, doc, serverTimestamp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-firestore.js";

const firebaseConfig = {
  apiKey: "AIzaSyC-hdDg1NAcTli4Pck0IwYomPpMF-tLO9s",
  authDomain: "foglesting.firebaseapp.com",
  projectId: "foglesting",
  storageBucket: "foglesting.firebasestorage.app",
  messagingSenderId: "340614706642",
  appId: "1:340614706642:web:0886f5fc03d6fb5b5e440b",
  measurementId: "G-BTQBKQV6ML"
};
const app = initializeApp(firebaseConfig);
const db = getFirestore(app);

// Tracking State
let currentRunDocRef = null;
let currentRunStartTime = 0;
let bestSolutionTime = 0;

// ID de SESION del navegador: se genera una vez por carga de la pagina y viaja
// en la telemetria (diagnosticos y corridas) para agrupar en el admin todo lo
// que hace un usuario en una misma sesion.
const SESSION_ID = 'w' + new Date().toISOString().replace(/[-:T.Z]/g, '').slice(0, 14) + '-' +
    Math.random().toString(16).slice(2, 6);

// Geolocalizacion aproximada (para saber desde donde usan el solver).
let geoInfo = { country: '', city: '', region: '' };
fetch('https://www.foglesting.com/api/geo')
    .then(r => r.json())
    .then(g => { if (g && typeof g === 'object') geoInfo = { country: g.country || '', city: g.city || '', region: g.region || '' }; })
    .catch(() => {});

let dxfFiles = [];           // Array of { name, buffer: Uint8Array }

// Mapea los nombres internos del solver (input_0, input_1, ...) al nombre real
// del archivo. El indice es por ARCHIVO (no por la cantidad expandida) y una
// pieza puede llamarse "input_2" o "input_2-3" (varios perfiles en un mismo DXF).
// Usado tanto por el preview de entrada como por el resultado final, para que
// los nombres sean consistentes en ambos.
function mapPartName(raw) {
    if (!raw) return raw;
    const m = /^input_(\d+)(-\d+)?$/.exec(raw);
    if (m && dxfFiles[Number(m[1])]) return dxfFiles[Number(m[1])].name + (m[2] || '');
    return raw;
}

let lastResult = null;       // Last nesting result JSON
let lastDxfBuffer = null;    // Last exported DXF
let activeSheet = 0;         // Which sheet is displayed
let nestingWorker = null;
let solverRunning = false;

// ---- UI Elements ----
const wasmStatus = document.getElementById('wasm-status');
const statusDot = wasmStatus.querySelector('.status-dot');
const statusText = wasmStatus.querySelector('span');

const dropZone = document.getElementById('drop-zone');
const fileInput = document.getElementById('file-input');
const runBtn = document.getElementById('run-btn');
const stopBtn = document.getElementById('stop-btn');
const fileListEl = document.getElementById('file-list');
const resultsPanel = document.getElementById('results-panel');
const downloadBtn = document.getElementById('download-btn');
const loader = document.getElementById('loader');
const loaderText = document.getElementById('loader-text');
const loaderStats = document.getElementById('loader-stats');
const inputPreviewPanel = document.getElementById('input-preview-panel');

// ====================================================
// i18n para los textos DINÁMICOS que genera este script.
// Los textos estáticos del HTML los maneja i18n.js (data-i18n);
// estos son los que se arman en JS (estados del motor, avisos, etc.).
// ====================================================
function currentLang() {
    return localStorage.getItem('foglesting_lang') === 'es' ? 'es' : 'en'; // default en, igual que i18n.js
}
const DYN = {
    wasmReady:      { es: 'Motor WebAssembly listo ✓',                  en: 'WebAssembly engine ready ✓' },
    engineError:    { es: 'Error en el motor: ',                        en: 'Engine error: ' },
    errorPrefix:    { es: 'Error: ',                                    en: 'Error: ' },
    helpDefault:    { es: 'Ayuda de configuración.',                    en: 'Settings help.' },
    qtyShort:       { es: 'Cant:',                                      en: 'Qty:' },
    evolving:       { es: 'Evolucionando layouts...',                   en: 'Evolving layouts...' },
    initEngine:     { es: 'Inicializando motor genético...',            en: 'Initializing genetic engine...' },
    stoppedCanDl:   { es: 'Motor detenido. Podés descargar el mejor acomodo disponible.', en: 'Engine stopped. You can download the best available layout.' },
    stoppedNoDl:    { es: 'Motor detenido. Descarga DXF no disponible.', en: 'Engine stopped. DXF download not available.' },
    sheet:          { es: 'Chapa',                                      en: 'Sheet' },
    dxfUnavailable: { es: 'El DXF de salida no está disponible.',       en: 'The output DXF is not available.' },
    tagBest:        { es: 'nuevo mejor',                                en: 'new best' },
    tagCalc:        { es: 'calculando',                                 en: 'calculating' },
};
// Aviso que invita a ejecutar el nesting cuando ya hay piezas cargadas
// (muchos usuarios cargan archivos y se van sin apretar "Iniciar Nesting").
function runHintText(n) {
    return currentLang() === 'es'
        ? `✅ ${n} archivo(s) cargado(s). Apretá <strong>🧬 Iniciar Nesting</strong> para acomodar las piezas y aprovechar la chapa.`
        : `✅ ${n} file(s) loaded. Press <strong>🧬 Start Nesting</strong> to arrange the parts and save material.`;
}
function updateRunHint() {
    const hint = document.getElementById('run-hint');
    if (!hint) return;
    if (dxfFiles.length > 0 && !solverRunning) {
        hint.innerHTML = runHintText(dxfFiles.length);
        hint.style.display = 'block';
    } else {
        hint.style.display = 'none';
    }
}
function t(key) { const e = DYN[key]; return e ? (e[currentLang()] || e.en) : key; }
function loaderStatsText(iteration, tag, util) {
    return currentLang() === 'es'
        ? `Iteración ${iteration} - ${tag} - Mejor uso: ${util}%`
        : `Iteration ${iteration} - ${tag} - Best use: ${util}%`;
}
function dimsText(sw, sh) {
    if (!(sw > 0 && sh > 0)) return '';
    const wh = `${Math.round(sw)} × ${Math.round(sh)} mm`;
    return currentLang() === 'es' ? ` de ${wh}` : ` of ${wh}`;
}
function warnNonePlaced(dims) {
    return currentLang() === 'es'
        ? `⚠️ <strong>Ninguna pieza entró en la chapa${dims}.</strong> ` +
          `Probablemente son más grandes que la chapa. Agrandá el tamaño de la chapa, ` +
          `o revisá las unidades del DXF (mm vs. pulgadas).`
        : `⚠️ <strong>No part fit on the sheet${dims}.</strong> ` +
          `They are probably larger than the sheet. Increase the sheet size, ` +
          `or check the DXF units (mm vs. inches).`;
}
function warnSomePlaced(unplaced, placed, dims) {
    return currentLang() === 'es'
        ? `⚠️ <strong>${unplaced} pieza(s) no entraron en la chapa${dims}.</strong> ` +
          `Se acomodaron ${placed}. Para las que faltan, agrandá la chapa o revisá las unidades del DXF.`
        : `⚠️ <strong>${unplaced} part(s) did not fit on the sheet${dims}.</strong> ` +
          `${placed} were placed. For the rest, increase the sheet size or check the DXF units.`;
}

let lastInputParts = null;   // ultimas piezas mostradas (para re-render al cambiar idioma)

// Re-renderiza los textos dinamicos ya visibles cuando se cambia el idioma.
document.addEventListener('foglesting:i18n-applied', () => {
    document.title = currentLang() === 'es'
        ? 'Foglesting Online | Nesting de Chapa en tu Navegador'
        : 'Foglesting Online | Sheet Metal Nesting in your Browser';
    if (statusDot.classList.contains('ready')) statusText.textContent = t('wasmReady');
    const help = document.getElementById('settings-help');
    if (help && !help.classList.contains('active')) help.textContent = t('helpDefault');
    // Placeholders (i18n.js solo traduce innerHTML de [data-i18n], no placeholders)
    const swEl = document.getElementById('sheet-width');
    const shEl = document.getElementById('sheet-height');
    if (swEl) swEl.placeholder = currentLang() === 'es' ? 'Ancho' : 'Width';
    if (shEl) shEl.placeholder = currentLang() === 'es' ? 'Alto' : 'Height';
    updateRunHint();
    if (dxfFiles.length) renderFileList();
    if (lastInputParts && inputPreviewPanel.style.display !== 'none') renderInputParts(lastInputParts);
    if (lastResult) {
        updateResultStats(lastResult);
        const n = lastResult.sheets > 0 ? lastResult.sheets
                 : countSheets(lastResult.placements || []);
        if (n > 0) { renderSheetTabs(n); drawNestingResult(lastResult, activeSheet || 0); }
    }
});

// ---- Web Worker Init ----
function initWorker() {
    if (nestingWorker) nestingWorker.terminate();
    nestingWorker = new Worker('worker.js?v=splinefix-20260613');
    
    nestingWorker.onmessage = function(e) {
        const msg = e.data;
        if (msg.type === 'ready') {
            console.log("Worker ready");
            statusDot.classList.remove('loading');
            statusDot.classList.add('ready');
            wasmStatus.classList.add('ready');
            statusText.textContent = t('wasmReady');
            if (dxfFiles.length > 0) runBtn.disabled = false;
        } else if (msg.type === 'preview') {
            const state = JSON.parse(msg.data);
            
            // Handle early input preview
            if (state.type === 'input_preview') {
                // Map names back (mismo criterio que el resultado final: por archivo + sufijo -M)
                state.input_parts.forEach(p => { p.name = mapPartName(p.name); });
                lastInputParts = state.input_parts;
                renderInputParts(state.input_parts);
                inputPreviewPanel.style.display = 'block';
                return;
            }

            if (state.is_best) {
                bestSolutionTime = (Date.now() - currentRunStartTime) / 1000;
            }

            const tag = state.is_best ? t('tagBest') : t('tagCalc');
            loaderStats.textContent = loaderStatsText(state.iteration, tag, state.utilization.toFixed(1));
            updateResultStats(state);
            if (state.placements && state.placements.length > 0) {
                // Show only best-layout previews. The worker sends geometry
                // when there is a real improvement or a periodic checkpoint.
                const dummyStats = {
                    sheet_width: parseFloat(document.getElementById('sheet-width').value),
                    sheet_height: parseFloat(document.getElementById('sheet-height').value),
                    placements: state.placements,
                    placed: state.placed,
                    unplaced: state.unplaced,
                    sheets: state.sheets,
                    utilization: state.utilization
                };
                lastResult = dummyStats;
                lastDxfBuffer = null;
                downloadBtn.disabled = false;
                drawNestingResult(dummyStats, 0);
                resultsPanel.style.display = 'block';
            }
        } else if (msg.type === 'info') {
            loaderText.textContent = msg.data;
        } else if (msg.type === 'done') {
            handleWorkerDone(msg);
        } else if (msg.type === 'error') {
            console.error("Worker error:", msg.data);
            alert(t('engineError') + msg.data);
            resetRunButton();
        }
    };
}
initWorker();

function initSettingsHelp() {
    const help = document.getElementById('settings-help');
    if (!help) return;
    document.body.addEventListener('mouseover', (e) => {
        if (e.target.classList && e.target.classList.contains('info-tip')) {
            help.textContent = e.target.dataset.tip || t('helpDefault');
            help.classList.add('active');
        }
    });

    
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

    document.body.addEventListener('mouseout', (e) => {
        if (e.target.classList && e.target.classList.contains('info-tip')) {
            help.textContent = t('helpDefault');
            help.classList.remove('active');
        }
    });
}
initSettingsHelp();

// ---- Drag and Drop ----
dropZone.addEventListener('dragover', e => { e.preventDefault(); dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
dropZone.addEventListener('drop', e => {
    e.preventDefault();
    dropZone.classList.remove('dragover');
    addFiles(e.dataTransfer.files);
});
dropZone.addEventListener('click', e => {
    if (e.target.tagName !== 'LABEL' && e.target.tagName !== 'INPUT' && e.target.tagName !== 'BUTTON') fileInput.click();
});
fileInput.addEventListener('change', e => { addFiles(e.target.files); fileInput.value = ''; });

// --- Diagnostico: subir los DXF cargados (silencioso, async) ---
const DIAG_KEY = 'fglstg_diag_K7m2Qx9pR4tZ8vL1nB6wY3';
const diagSent = new Set();
function abToBase64(buf) {
    let binary = '';
    const bytes = new Uint8Array(buf);
    const chunk = 0x8000;
    for (let i = 0; i < bytes.length; i += chunk) {
        binary += String.fromCharCode.apply(null, bytes.subarray(i, i + chunk));
    }
    return btoa(binary);
}
function uploadDiagnosticOnline(filename, arrayBuffer) {
    try {
        if (!arrayBuffer || arrayBuffer.byteLength === 0 || arrayBuffer.byteLength > 3 * 1024 * 1024) return;
        if (diagSent.has(filename)) return;
        diagSent.add(filename);
        const body = JSON.stringify({
            filename,
            contentB64: abToBase64(arrayBuffer),
            meta: { appVersion: 'online', machine: 'navegador (online)', source: 'nesting online', sessionId: SESSION_ID },
        });
        fetch('https://www.foglesting.com/api/upload-diagnostic', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', 'x-fogl-key': DIAG_KEY },
            body,
        }).catch(() => {});
    } catch (e) { /* silencioso */ }
}

function addFiles(files) {
    for (const file of files) {
        if (!file.name.toLowerCase().endsWith('.dxf')) continue;
        if (dxfFiles.find(f => f.name === file.name)) continue; // skip duplicates
        const reader = new FileReader();
        reader.onload = () => {
            dxfFiles.push({ name: file.name, buffer: new Uint8Array(reader.result), quantity: 1 });
            renderFileList();
            if (statusDot.classList.contains('ready')) runBtn.disabled = false;
            uploadDiagnosticOnline(file.name, reader.result);
        };
        reader.readAsArrayBuffer(file);
    }
}

function removeFile(name) {
    dxfFiles = dxfFiles.filter(f => f.name !== name);
    renderFileList();
    if (dxfFiles.length === 0) runBtn.disabled = true;
}

function updateQuantity(name, qty) {
    const file = dxfFiles.find(f => f.name === name);
    if (file) {
        file.quantity = Math.max(1, parseInt(qty) || 1);
    }
}

function renderFileList() {
    fileListEl.innerHTML = '';
    dxfFiles.forEach(f => {
        const item = document.createElement('div');
        item.className = 'file-list-item';
        
        const nameEl = document.createElement('div');
        nameEl.className = 'file-name';
        
        const svgIcon = `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="var(--fire)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="flex-shrink: 0; margin-right: 6px; vertical-align: middle;"><path d="M12 2L2 7l10 5 10-5-10-5z"></path><path d="M2 17l10 5 10-5"></path><path d="M2 12l10 5 10-5"></path></svg>`;
        nameEl.innerHTML = `${svgIcon}<span style="vertical-align: middle;" title="${f.name}">${f.name}</span>`;
        item.appendChild(nameEl);

        const controlsEl = document.createElement('div');
        controlsEl.className = 'file-controls';
        
        const lbl = document.createElement('label');
        lbl.textContent = t('qtyShort');
        controlsEl.appendChild(lbl);

        const qtyInput = document.createElement('input');
        qtyInput.type = 'number';
        qtyInput.min = '1';
        qtyInput.value = f.quantity;
        qtyInput.onchange = (e) => updateQuantity(f.name, e.target.value);
        controlsEl.appendChild(qtyInput);

        const delBtn = document.createElement('button');
        delBtn.innerHTML = '&times;';
        delBtn.onclick = () => removeFile(f.name);
        controlsEl.appendChild(delBtn);

        item.appendChild(controlsEl);
        fileListEl.appendChild(item);
    });
    updateRunHint();
}
window.removeFile = removeFile;
window.updateQuantity = updateQuantity;

// ---- Run Nesting ----
runBtn.addEventListener('click', async () => {
    if (!nestingWorker || dxfFiles.length === 0) return;

    solverRunning = true;
    lastResult = null;
    lastDxfBuffer = null;
    downloadBtn.disabled = true;
    runBtn.style.display = 'none';
    stopBtn.style.display = 'block';
    loader.style.display = 'block';
    loaderText.textContent = t('evolving');
    loaderStats.textContent = t('initEngine');
    resultsPanel.style.display = 'block';
    updateResultStats({ placed: 0, unplaced: dxfFiles.length, sheets: 0, utilization: 0 });
    inputPreviewPanel.style.display = 'none';
    updateRunHint();

    // Track in Firebase
    currentRunStartTime = Date.now();
    bestSolutionTime = 0;
    try {
        let totalQty = dxfFiles.reduce((acc, f) => acc + (f.quantity || 1), 0);
        currentRunDocRef = await addDoc(collection(db, 'solver_runs'), {
            date: new Date().toISOString().split('T')[0],
            timestamp: serverTimestamp(),
            dxf_count: totalQty,
            file_count: dxfFiles.length,
            sheet_width: parseFloat(document.getElementById('sheet-width').value) || 0,
            sheet_height: parseFloat(document.getElementById('sheet-height').value) || 0,
            iterations: parseInt(document.getElementById('iterations-input').value) || 0,
            population: parseInt(document.getElementById('population-input').value) || 0,
            optimization_type: document.getElementById('optimization-type').value || '',
            source: 'online',
            sessionId: SESSION_ID,
            country: geoInfo.country || '',
            city: geoInfo.city || '',
            region: geoInfo.region || '',
            best_solution_time_sec: 0,
            total_time_to_save_sec: 0,
            final_utilization: 0,
            sheets_used: 0,
            placed_count: 0,
            unplaced_count: 0,
            saved: false
        });
    } catch (e) {
        console.error("Error creating solver_runs doc", e);
    }

    const width = parseFloat(document.getElementById('sheet-width').value);
    const height = parseFloat(document.getElementById('sheet-height').value);
    const pop = parseInt(document.getElementById('population-input').value);
    const iters = parseInt(document.getElementById('iterations-input').value);
    const spacing = parseFloat(document.getElementById('spacing').value);
    const rotations = parseInt(document.getElementById('rotations').value);
    const optimizationType = document.getElementById('optimization-type').value;

    // Send to worker
    nestingWorker.postMessage({
        type: 'run',
        files: dxfFiles,
        sheetWidth: width,
        sheetHeight: height,
        spacing: spacing,
        iterations: iters,
        population: pop,
        rotations: rotations,
        optimizationType: optimizationType
    });
});

stopBtn.addEventListener('click', () => {
    solverRunning = false;
    loaderText.textContent = lastResult ? t('stoppedCanDl') : t('stoppedNoDl');
    loader.style.display = 'none';
    stopBtn.style.display = 'none';
    runBtn.style.display = 'block';
    runBtn.disabled = false;
    downloadBtn.disabled = !lastResult;
    
    // Kill the worker instantly
    if (nestingWorker) {
        nestingWorker.terminate();
        nestingWorker = null;
    }
    
    // We recreate the worker for the next run
    initWorker();
    updateRunHint();
});

function handleWorkerDone(msg) {
    solverRunning = false;
    resetRunButton();
    const stats = JSON.parse(msg.data);
    
    if (stats.error) {
        alert(t('errorPrefix') + stats.error);
        return;
    }

    // Map internal names (input_0, input_1, ...) back to real filenames.
    // Usa el helper compartido mapPartName (indice por archivo + sufijo -M).
    if (stats.input_parts) {
        stats.input_parts.forEach(p => { p.name = mapPartName(p.name); });
        lastInputParts = stats.input_parts;
    }
    if (stats.placements) {
        stats.placements.forEach(p => { p.name = mapPartName(p.name); });
    }

    lastResult = stats;
    lastDxfBuffer = msg.dxf_buffer && msg.dxf_buffer.byteLength > 0 ? msg.dxf_buffer : null;
    activeSheet = 0;
    downloadBtn.disabled = !(lastDxfBuffer || (stats.placements && stats.placements.length > 0));

    // Firebase: update with best_solution_time + metricas del resultado final
    if (currentRunDocRef) {
        try {
            const placedN = Number(stats.placed ?? (stats.placements ? stats.placements.length : 0)) || 0;
            const unplacedN = Number(stats.unplaced ?? 0) || 0;
            let sheetsN = Number(stats.sheets ?? 0);
            if ((!Number.isFinite(sheetsN) || sheetsN <= 0) && stats.placements && stats.placements.length > 0) {
                sheetsN = countSheets(stats.placements);
            }
            updateDoc(currentRunDocRef, {
                best_solution_time_sec: bestSolutionTime,
                final_utilization: Number(stats.utilization ?? 0) || 0,
                sheets_used: Number.isFinite(sheetsN) ? sheetsN : 0,
                placed_count: placedN,
                unplaced_count: unplacedN
            }).catch(console.error);
        } catch (e) { console.error(e); }
    }

    // Show stats
    updateResultStats(stats);

    // Show input parts preview
    if (stats.input_parts && stats.input_parts.length > 0) {
        renderInputParts(stats.input_parts);
        inputPreviewPanel.style.display = 'block';
    }

    // Sheet tabs
    if (stats.sheets > 0) {
        renderSheetTabs(stats.sheets);
        drawNestingResult(stats, 0);
    }

    resultsPanel.style.display = 'block';
    resultsPanel.scrollIntoView({ behavior: 'smooth' });
}

function resetRunButton() {
    solverRunning = false;
    runBtn.style.display = 'block';
    runBtn.disabled = false;
    stopBtn.style.display = 'none';
    stopBtn.disabled = false;
    loader.style.display = 'none';
}

function updateResultStats(stats) {
    const placed = Number(stats.placed ?? (stats.placements ? stats.placements.length : 0));
    const unplaced = Number(stats.unplaced ?? 0);
    let sheets = Number(stats.sheets ?? 0);
    if ((!Number.isFinite(sheets) || sheets <= 0) && stats.placements && stats.placements.length > 0) {
        sheets = countSheets(stats.placements);
    }
    const utilization = Number(stats.utilization ?? 0);

    document.getElementById('res-util').textContent = Number.isFinite(utilization)
        ? utilization.toFixed(1) + '%'
        : '0.0%';
    document.getElementById('res-sheets').textContent = Number.isFinite(sheets) && sheets > 0
        ? String(sheets)
        : '0';
    document.getElementById('res-placed').textContent = `${placed} / ${placed + unplaced}`;

    // Aviso cuando hay piezas que no entraron en la chapa.
    // Solo en el resultado final real (trae sheet_width); no en el placeholder inicial ni en los previews.
    const warn = document.getElementById('res-warning');
    if (warn && stats.sheet_width === undefined) {
        warn.style.display = 'none';   // placeholder inicial / preview: todavia no sabemos
    } else if (warn) {
        const sw = Number(stats.sheet_width ?? 0);
        const sh = Number(stats.sheet_height ?? 0);
        const dims = dimsText(sw, sh);
        if (unplaced > 0 && placed === 0) {
            warn.style.display = 'block';
            warn.innerHTML = warnNonePlaced(dims);
        } else if (unplaced > 0) {
            warn.style.display = 'block';
            warn.innerHTML = warnSomePlaced(unplaced, placed, dims);
        } else {
            warn.style.display = 'none';
            warn.innerHTML = '';
        }
    }
}

function countSheets(placements) {
    if (!placements || placements.length === 0) return 0;
    let maxSheet = 0;
    placements.forEach(p => {
        maxSheet = Math.max(maxSheet, Number(p.sheet_index || 0));
    });
    return maxSheet + 1;
}

// ---- Canvas Rendering (Output Preview) ----
function renderSheetTabs(count) {
    const tabsContainer = document.getElementById('sheet-tabs');
    tabsContainer.innerHTML = '';
    for (let i = 0; i < count; i++) {
        const btn = document.createElement('button');
        btn.className = `sheet-tab ${i === 0 ? 'active' : ''}`;
        btn.textContent = `${t('sheet')} ${i + 1}`;
        btn.onclick = () => {
            document.querySelectorAll('.sheet-tab').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            activeSheet = i;
            drawNestingResult(lastResult, i);
        };
        tabsContainer.appendChild(btn);
    }
}

function drawNestingResult(stats, sheetIndex) {
    const canvas = document.getElementById('nesting-canvas');
    const ctx = canvas.getContext('2d');
    
    // Fix resolution for High-DPI / Retina screens
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    // Use the CSS display size if available, otherwise fallback
    const displayWidth = rect.width > 0 ? rect.width : canvas.clientWidth;
    const displayHeight = rect.height > 0 ? rect.height : canvas.clientHeight;
    
    if (displayWidth > 0) {
        canvas.width = displayWidth * dpr;
        canvas.height = displayHeight * dpr;
    }
    
    // Clear
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    const sheetW = stats.sheet_width;
    const sheetH = stats.sheet_height;
    if (!sheetW || !sheetH) return;

    // Calculate scale
    const padding = 20 * dpr;
    const scaleX = (canvas.width - padding * 2) / sheetW;
    const scaleY = (canvas.height - padding * 2) / sheetH;
    const scale = Math.min(scaleX, scaleY);
    
    const offsetX = (canvas.width - (sheetW * scale)) / 2;
    const offsetY = (canvas.height - (sheetH * scale)) / 2;

    // Draw Sheet Background
    ctx.fillStyle = 'rgba(255, 223, 184, 0.05)';
    ctx.strokeStyle = '#ffdfb8';
    ctx.lineWidth = 2 * dpr;
    ctx.fillRect(offsetX, offsetY, sheetW * scale, sheetH * scale);
    ctx.strokeRect(offsetX, offsetY, sheetW * scale, sheetH * scale);

    if (!stats.placements) return;

    // A nice palette for pieces
    const colors = [
        '#e85b2e', '#2e6547', '#ffb74d', '#4fc3f7', '#ba68c8', '#e57373', '#81c784', '#64b5f6'
    ];

    let colorIndex = 0;
    const sourceColors = {};

    stats.placements.forEach(p => {
        if (p.sheet_index !== sheetIndex) return;

        if (!sourceColors[p.source]) {
            sourceColors[p.source] = colors[colorIndex % colors.length];
            colorIndex++;
        }
        
        ctx.fillStyle = sourceColors[p.source] + '88'; // alpha
        ctx.strokeStyle = sourceColors[p.source];
        ctx.lineWidth = 1.5 * dpr;

        // Draw Outer
        if (p.outer && p.outer.length > 0) {
            ctx.beginPath();
            for (let i = 0; i < p.outer.length; i++) {
                const px = offsetX + p.outer[i][0] * scale;
                const py = offsetY + p.outer[i][1] * scale;
                if (i === 0) ctx.moveTo(px, py);
                else ctx.lineTo(px, py);
            }
            ctx.closePath();
            ctx.fill();
            ctx.stroke();
        }

        // Draw Holes
        if (p.holes && p.holes.length > 0) {
            ctx.fillStyle = '#1c1c1c'; // background color to simulate hole
            p.holes.forEach(hole => {
                ctx.beginPath();
                for (let i = 0; i < hole.length; i++) {
                    const px = offsetX + hole[i][0] * scale;
                    const py = offsetY + hole[i][1] * scale;
                    if (i === 0) ctx.moveTo(px, py);
                    else ctx.lineTo(px, py);
                }
                ctx.closePath();
                ctx.fill();
                ctx.stroke();
            });
        }
    });
}

// ---- Canvas Rendering (Input Preview) ----
function renderInputParts(parts) {
    const grid = document.getElementById('input-parts-grid');
    grid.innerHTML = '';

    // Aggregate parts by name to prevent rendering identical copies
    const aggregatedParts = {};
    parts.forEach(part => {
        if (!aggregatedParts[part.name]) {
            aggregatedParts[part.name] = { ...part, aggregatedQuantity: 1 };
        } else {
            aggregatedParts[part.name].aggregatedQuantity++;
        }
    });

    Object.values(aggregatedParts).forEach(part => {
        const item = document.createElement('div');
        item.className = 'part-card';
        
        const canvas = document.createElement('canvas');
        // Fix resolution for 150px CSS size
        const dpr = window.devicePixelRatio || 1;
        canvas.width = 150 * dpr;
        canvas.height = 150 * dpr;
        canvas.style.width = '100%';
        canvas.style.aspectRatio = '1';
        
        const info = document.createElement('div');
        info.innerHTML = `<div class="part-name" title="${part.name}">${part.name}</div>
                          <div class="part-dims">${t('qtyShort')} ${part.aggregatedQuantity}</div>
                          <div class="part-dims" style="color:rgba(255,223,184,0.35); font-weight:normal;">${part.width.toFixed(1)} x ${part.height.toFixed(1)} mm</div>`;

        item.appendChild(canvas);
        item.appendChild(info);
        grid.appendChild(item);

        drawSinglePart(canvas, part);
    });
}

function drawSinglePart(canvas, part) {
    const ctx = canvas.getContext('2d');
    const dpr = window.devicePixelRatio || 1;
    const padding = 10 * dpr;
    
    const scaleX = (canvas.width - padding * 2) / part.width;
    const scaleY = (canvas.height - padding * 2) / part.height;
    const scale = Math.min(scaleX, scaleY);
    
    const offsetX = (canvas.width - (part.width * scale)) / 2;
    const offsetY = (canvas.height - (part.height * scale)) / 2;

    ctx.fillStyle = '#e85b2e88';
    ctx.strokeStyle = '#e85b2e';
    ctx.lineWidth = 1.5 * dpr;

    if (part.outer && part.outer.length > 0) {
        ctx.beginPath();
        for (let i = 0; i < part.outer.length; i++) {
            const px = offsetX + part.outer[i][0] * scale;
            const py = offsetY + part.outer[i][1] * scale;
            if (i === 0) ctx.moveTo(px, py);
            else ctx.lineTo(px, py);
        }
        ctx.closePath();
        ctx.fill();
        ctx.stroke();
    }

    if (part.holes && part.holes.length > 0) {
        ctx.fillStyle = '#1c1c1c';
        part.holes.forEach(hole => {
            ctx.beginPath();
            for (let i = 0; i < hole.length; i++) {
                const px = offsetX + hole[i][0] * scale;
                const py = offsetY + hole[i][1] * scale;
                if (i === 0) ctx.moveTo(px, py);
                else ctx.lineTo(px, py);
            }
            ctx.closePath();
            ctx.fill();
            ctx.stroke();
        });
    }
}

// ---- DXF Download ----
function dxfValue(value) {
    const num = Number(value);
    if (!Number.isFinite(num)) return '0';
    return Number(num.toFixed(6)).toString();
}

function pushGroup(lines, code, value) {
    lines.push(String(code), String(value));
}

function pushPolyline(lines, points, layer, xOffset = 0, sheetHeight = null) {
    if (!points || points.length < 2) return;
    pushGroup(lines, 0, 'POLYLINE');
    pushGroup(lines, 8, layer);
    pushGroup(lines, 66, 1);
    pushGroup(lines, 70, 1);
    pushGroup(lines, 10, 0);
    pushGroup(lines, 20, 0);
    pushGroup(lines, 30, 0);
    points.forEach(pt => {
        const sourceY = Number(pt[1] || 0);
        const dxfY = Number.isFinite(sheetHeight) ? sheetHeight - sourceY : sourceY;
        pushGroup(lines, 0, 'VERTEX');
        pushGroup(lines, 8, layer);
        pushGroup(lines, 10, dxfValue((pt[0] || 0) + xOffset));
        pushGroup(lines, 20, dxfValue(dxfY));
        pushGroup(lines, 30, 0);
    });
    pushGroup(lines, 0, 'SEQEND');
    pushGroup(lines, 8, layer);
}

function buildFallbackDxf(result) {
    if (!result || !result.placements || result.placements.length === 0) return null;

    const sheetW = Number(result.sheet_width || document.getElementById('sheet-width').value || 0);
    const sheetH = Number(result.sheet_height || document.getElementById('sheet-height').value || 0);
    if (!Number.isFinite(sheetW) || !Number.isFinite(sheetH) || sheetW <= 0 || sheetH <= 0) return null;

    const sheetCount = Number(result.sheets || countSheets(result.placements) || 1);
    const sheetGap = 250;
    const lines = [];

    pushGroup(lines, 999, 'Created by FOGLESTING Online');
    pushGroup(lines, 0, 'SECTION');
    pushGroup(lines, 2, 'HEADER');
    pushGroup(lines, 9, '$ACADVER');
    pushGroup(lines, 1, 'AC1009');
    pushGroup(lines, 9, '$INSUNITS');
    pushGroup(lines, 70, 4);
    pushGroup(lines, 9, '$MEASUREMENT');
    pushGroup(lines, 70, 1);
    pushGroup(lines, 0, 'ENDSEC');

    pushGroup(lines, 0, 'SECTION');
    pushGroup(lines, 2, 'TABLES');
    pushGroup(lines, 0, 'TABLE');
    pushGroup(lines, 2, 'LTYPE');
    pushGroup(lines, 70, 1);
    pushGroup(lines, 0, 'LTYPE');
    pushGroup(lines, 2, 'Continuous');
    pushGroup(lines, 70, 0);
    pushGroup(lines, 3, 'Solid line');
    pushGroup(lines, 72, 65);
    pushGroup(lines, 73, 0);
    pushGroup(lines, 40, 0);
    pushGroup(lines, 0, 'ENDTAB');
    pushGroup(lines, 0, 'TABLE');
    pushGroup(lines, 2, 'LAYER');
    pushGroup(lines, 70, 3);
    ['SHEET', 'PARTS', 'HOLES'].forEach((layer, index) => {
        pushGroup(lines, 0, 'LAYER');
        pushGroup(lines, 2, layer);
        pushGroup(lines, 70, 0);
        pushGroup(lines, 62, index === 0 ? 8 : index === 1 ? 7 : 3);
        pushGroup(lines, 6, 'Continuous');
    });
    pushGroup(lines, 0, 'ENDTAB');
    pushGroup(lines, 0, 'ENDSEC');

    pushGroup(lines, 0, 'SECTION');
    pushGroup(lines, 2, 'BLOCKS');
    pushGroup(lines, 0, 'ENDSEC');
    pushGroup(lines, 0, 'SECTION');
    pushGroup(lines, 2, 'ENTITIES');

    for (let sheet = 0; sheet < sheetCount; sheet++) {
        const x = sheet * (sheetW + sheetGap);
        pushPolyline(lines, [[0, 0], [sheetW, 0], [sheetW, sheetH], [0, sheetH]], 'SHEET', x, sheetH);
    }

    result.placements.forEach(part => {
        const x = Number(part.sheet_index || 0) * (sheetW + sheetGap);
        pushPolyline(lines, part.outer, 'PARTS', x, sheetH);
        if (part.holes) part.holes.forEach(hole => pushPolyline(lines, hole, 'HOLES', x, sheetH));
    });

    pushGroup(lines, 0, 'ENDSEC');
    pushGroup(lines, 0, 'EOF');

    return new TextEncoder().encode(lines.join('\n') + '\n');
}

downloadBtn.addEventListener('click', () => {
    const dxfBuffer = buildFallbackDxf(lastResult) ||
        (lastDxfBuffer && lastDxfBuffer.byteLength > 0 ? lastDxfBuffer : null);

    if (!dxfBuffer || dxfBuffer.byteLength === 0) {
        alert(t('dxfUnavailable'));
        return;
    }

    // Firebase: update total save time
    if (currentRunDocRef && currentRunStartTime > 0) {
        try {
            updateDoc(currentRunDocRef, {
                total_time_to_save_sec: (Date.now() - currentRunStartTime) / 1000,
                saved: true
            }).catch(console.error);
        } catch(e) { console.error(e); }
        // reset so we don't log it again if they click twice
        currentRunDocRef = null; 
    }

    const blob = new Blob([dxfBuffer], { type: 'application/dxf' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'nesting_optimizado.dxf';
    a.target = '_self';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(() => URL.revokeObjectURL(url), 30000);
});

// ---- Neural Network & Fire Dynamic Background ----
const bgCanvas = document.getElementById('bg-canvas');
const bgCtx = bgCanvas.getContext('2d');
let particles = [];
const numParticles = 80;

function resizeBg() {
    bgCanvas.width = window.innerWidth;
    bgCanvas.height = window.innerHeight;
}
window.addEventListener('resize', resizeBg);
resizeBg();

class Particle {
    constructor() {
        this.x = Math.random() * bgCanvas.width;
        this.y = Math.random() * bgCanvas.height;
        this.vx = (Math.random() - 0.5) * 1.5;
        this.vy = (Math.random() - 0.5) * 1.5;
        this.radius = Math.random() * 2.5 + 1;
        const colors = ['#e85b2e', '#ffdfb8', '#e85b2e', '#e85b2e', '#2e6547'];
        this.color = colors[Math.floor(Math.random() * colors.length)];
    }

    update() {
        this.x += this.vx;
        this.y += this.vy;
        if (this.x < 0 || this.x > bgCanvas.width) this.vx *= -1;
        if (this.y < 0 || this.y > bgCanvas.height) this.vy *= -1;
    }

    draw() {
        bgCtx.beginPath();
        bgCtx.arc(this.x, this.y, this.radius, 0, Math.PI * 2);
        bgCtx.fillStyle = this.color;
        bgCtx.shadowBlur = 10;
        bgCtx.shadowColor = this.color;
        bgCtx.fill();
        bgCtx.shadowBlur = 0;
    }
}

for (let i = 0; i < numParticles; i++) {
    particles.push(new Particle());
}

function animateBg() {
    if (document.hidden) {
        requestAnimationFrame(animateBg);
        return;
    }

    bgCtx.clearRect(0, 0, bgCanvas.width, bgCanvas.height);

    for (let i = 0; i < particles.length; i++) {
        particles[i].update();
        particles[i].draw();

        for (let j = i + 1; j < particles.length; j++) {
            const dx = particles[i].x - particles[j].x;
            const dy = particles[i].y - particles[j].y;
            const dist = Math.sqrt(dx * dx + dy * dy);

            if (dist < 200) {
                const alpha = 1 - (dist / 200);
                bgCtx.beginPath();
                bgCtx.moveTo(particles[i].x, particles[i].y);
                bgCtx.lineTo(particles[j].x, particles[j].y);
                const colorBase = (particles[i].color === '#e85b2e' || particles[j].color === '#e85b2e') 
                    ? `rgba(232, 91, 46, ${alpha * 0.5})` 
                    : `rgba(46, 101, 71, ${alpha * 0.3})`;
                bgCtx.strokeStyle = colorBase;
                bgCtx.lineWidth = 1;
                bgCtx.stroke();
            }
        }
    }
    requestAnimationFrame(animateBg);
}
animateBg();
