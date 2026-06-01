/**
 * Foglesting DXF Editor
 * ES Module - uses esm.sh CDN for dxf-parser
 */

import DxfParser from 'https://esm.sh/dxf-parser@1.1.1';

// ---- State ----
let entities = [];
let view = { x: 0, y: 0, scale: 1 };
let canvas, ctx;
let activeTool = 'select';
let selectedEntities = new Set();
let loadedFiles = [];     // [{name, entities}]
let activeFileIdx = 0;

// Mouse State
let mouse = { worldX: 0, worldY: 0, isDown: false, startX: 0, startY: 0 };
let toolState = { step: 0, data: null };

const COLORS = {
    bg: '#111111',
    grid: '#2a2a2a',
    gridMajor: '#383838',
    entity: '#ffdfb8',
    selected: '#e85b2e',
    preview: '#3d8760',
    origin: { x: '#e85b2e', y: '#3d8760' }
};

// ---- Init ----
function init() {
    canvas = document.getElementById('dxf-canvas');
    ctx = canvas.getContext('2d');

    resize();
    window.addEventListener('resize', resize);
    initBgCanvas();

    // Canvas events
    canvas.addEventListener('mousedown', onMouseDown);
    canvas.addEventListener('mousemove', onMouseMove);
    window.addEventListener('mouseup', onMouseUp);
    canvas.addEventListener('wheel', onWheel, { passive: false });

    // Drag & Drop on canvas
    canvas.addEventListener('dragover', e => { e.preventDefault(); canvas.style.outline = '2px dashed var(--fire)'; });
    canvas.addEventListener('dragleave', () => { canvas.style.outline = ''; });
    canvas.addEventListener('drop', e => {
        e.preventDefault();
        canvas.style.outline = '';
        const files = [...e.dataTransfer.files].filter(f => f.name.toLowerCase().endsWith('.dxf'));
        if (files.length) loadFiles(files);
    });

    document.addEventListener('keydown', onKeyDown);

    // Toolbar tool buttons
    document.querySelectorAll('.tool-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            if (btn.dataset.tool === 'delete') { deleteSelected(); return; }
            document.querySelectorAll('.tool-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            activeTool = btn.dataset.tool;
            toolState = { step: 0, data: null };
        });
    });

    document.getElementById('file-input').addEventListener('change', e => {
        const files = [...e.target.files];
        if (files.length) loadFiles(files);
        e.target.value = ''; // allow re-selecting same files
    });
    document.getElementById('export-btn').addEventListener('click', handleExport);

    requestAnimationFrame(render);
}

function resize() {
    const rect = canvas.parentElement.getBoundingClientRect();
    canvas.width = Math.max(rect.width, 100);
    canvas.height = Math.max(rect.height, 100);
}

// ---- Coordinates ----
function toScreen(wx, wy) {
    return {
        x: (wx - view.x) * view.scale + canvas.width / 2,
        y: canvas.height / 2 - (wy - view.y) * view.scale
    };
}
function toWorld(sx, sy) {
    return {
        x: (sx - canvas.width / 2) / view.scale + view.x,
        y: view.y - (sy - canvas.height / 2) / view.scale
    };
}

// ---- Render ----
function render() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = COLORS.bg;
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    drawGrid();

    ctx.lineWidth = 1.5 / view.scale;
    ctx.lineWidth = Math.max(0.5, Math.min(2, 1.5 / view.scale)) ;

    entities.forEach(ent => {
        ctx.strokeStyle = selectedEntities.has(ent) ? COLORS.selected : COLORS.entity;
        ctx.lineWidth = 1.5;
        drawEntity(ent);
    });

    drawPreview();
    requestAnimationFrame(render);
}

function drawGrid() {
    const worldW = canvas.width / view.scale;
    const worldH = canvas.height / view.scale;

    // Choose a nice grid interval
    const rawInterval = worldW / 10;
    const magnitude = Math.pow(10, Math.floor(Math.log10(rawInterval)));
    const candidates = [1, 2, 5, 10];
    let gridSize = magnitude * candidates.find(c => magnitude * c >= rawInterval) || magnitude * 10;

    const minorGridSize = gridSize;
    const majorGridSize = gridSize * 5;

    const leftWorld = view.x - worldW / 2;
    const rightWorld = view.x + worldW / 2;
    const bottomWorld = view.y - worldH / 2;
    const topWorld = view.y + worldH / 2;

    const startX = Math.floor(leftWorld / minorGridSize) * minorGridSize;
    const startY = Math.floor(bottomWorld / minorGridSize) * minorGridSize;

    ctx.beginPath();
    ctx.strokeStyle = COLORS.grid;
    ctx.lineWidth = 1;
    for (let x = startX; x <= rightWorld; x += minorGridSize) {
        const sx = toScreen(x, 0).x;
        ctx.moveTo(sx, 0); ctx.lineTo(sx, canvas.height);
    }
    for (let y = startY; y <= topWorld; y += minorGridSize) {
        const sy = toScreen(0, y).y;
        ctx.moveTo(0, sy); ctx.lineTo(canvas.width, sy);
    }
    ctx.stroke();

    ctx.beginPath();
    ctx.strokeStyle = COLORS.gridMajor;
    ctx.lineWidth = 1;
    const startXM = Math.floor(leftWorld / majorGridSize) * majorGridSize;
    const startYM = Math.floor(bottomWorld / majorGridSize) * majorGridSize;
    for (let x = startXM; x <= rightWorld; x += majorGridSize) {
        const sx = toScreen(x, 0).x;
        ctx.moveTo(sx, 0); ctx.lineTo(sx, canvas.height);
    }
    for (let y = startYM; y <= topWorld; y += majorGridSize) {
        const sy = toScreen(0, y).y;
        ctx.moveTo(0, sy); ctx.lineTo(canvas.width, sy);
    }
    ctx.stroke();

    // Origin axes
    const o = toScreen(0, 0);
    ctx.lineWidth = 2;
    ctx.strokeStyle = COLORS.origin.x;
    ctx.beginPath(); ctx.moveTo(o.x, o.y); ctx.lineTo(o.x + 40, o.y); ctx.stroke();
    ctx.strokeStyle = COLORS.origin.y;
    ctx.beginPath(); ctx.moveTo(o.x, o.y); ctx.lineTo(o.x, o.y - 40); ctx.stroke();
}

function drawEntity(ent, isPreview = false) {
    if (isPreview) {
        ctx.strokeStyle = COLORS.preview;
        ctx.setLineDash([6, 4]);
    } else {
        ctx.setLineDash([]);
    }

    ctx.beginPath();
    const type = ent.type;

    if (type === 'LINE') {
        const v = ent.vertices || [{ x: ent.start?.x ?? 0, y: ent.start?.y ?? 0 }, { x: ent.end?.x ?? 0, y: ent.end?.y ?? 0 }];
        if (v.length < 2) { ctx.setLineDash([]); return; }
        const p1 = toScreen(v[0].x, v[0].y);
        const p2 = toScreen(v[1].x, v[1].y);
        ctx.moveTo(p1.x, p1.y);
        ctx.lineTo(p2.x, p2.y);
    } else if (type === 'CIRCLE') {
        const c = toScreen(ent.center.x, ent.center.y);
        ctx.arc(c.x, c.y, ent.radius * view.scale, 0, Math.PI * 2);
    } else if (type === 'ARC') {
        const c = toScreen(ent.center.x, ent.center.y);
        // DXF arcs are CCW, canvas Y-axis is flipped
        const sa = -ent.endAngle * Math.PI / 180;
        const ea = -ent.startAngle * Math.PI / 180;
        ctx.arc(c.x, c.y, ent.radius * view.scale, sa, ea, false);
    } else if (type === 'LWPOLYLINE' || type === 'POLYLINE') {
        const verts = ent.vertices || [];
        if (verts.length === 0) { ctx.setLineDash([]); return; }
        const first = toScreen(verts[0].x, verts[0].y);
        ctx.moveTo(first.x, first.y);
        for (let i = 1; i < verts.length; i++) {
            const p = toScreen(verts[i].x, verts[i].y);
            ctx.lineTo(p.x, p.y);
        }
        if (ent.shape || ent.closed) ctx.closePath();
    } else if (type === 'SPLINE') {
        const pts = ent.controlPoints || ent.fitPoints || [];
        if (pts.length < 2) { ctx.setLineDash([]); return; }
        const p0 = toScreen(pts[0].x, pts[0].y);
        ctx.moveTo(p0.x, p0.y);
        for (let i = 1; i < pts.length; i++) {
            const p = toScreen(pts[i].x, pts[i].y);
            ctx.lineTo(p.x, p.y);
        }
        if (ent.closed) ctx.closePath();
    } else if (type === 'ELLIPSE') {
        const c = toScreen(ent.center.x, ent.center.y);
        const rx = ent.majorAxisEndPoint
            ? Math.hypot(ent.majorAxisEndPoint.x, ent.majorAxisEndPoint.y) * view.scale
            : ent.majorRadius * view.scale;
        const ratio = ent.axisRatio || ent.minorRadius / ent.majorRadius || 0.5;
        const ry = rx * ratio;
        const rot = ent.majorAxisEndPoint ? Math.atan2(ent.majorAxisEndPoint.y, ent.majorAxisEndPoint.x) : 0;
        ctx.ellipse(c.x, c.y, rx, ry, -rot, -(ent.endAngle || Math.PI * 2), -(ent.startAngle || 0), false);
    }
    ctx.stroke();
    ctx.setLineDash([]);
}

function drawPreview() {
    if (activeTool === 'line' && toolState.step === 1) {
        ctx.lineWidth = 1.5;
        drawEntity({ type: 'LINE', vertices: [toolState.data.p1, { x: mouse.worldX, y: mouse.worldY }] }, true);
    } else if (activeTool === 'circle' && toolState.step === 1) {
        const r = Math.hypot(mouse.worldX - toolState.data.p1.x, mouse.worldY - toolState.data.p1.y);
        ctx.lineWidth = 1.5;
        drawEntity({ type: 'CIRCLE', center: toolState.data.p1, radius: r }, true);
    } else if (activeTool === 'move' && selectedEntities.size > 0 && toolState.step === 1) {
        const dx = mouse.worldX - toolState.data.start.x;
        const dy = mouse.worldY - toolState.data.start.y;
        selectedEntities.forEach(ent => {
            const cloned = cloneEntity(ent);
            translateEntity(cloned, dx, dy);
            ctx.lineWidth = 1.5;
            drawEntity(cloned, true);
        });
    }
}

// ---- Mouse ----
function onMouseDown(e) {
    mouse.isDown = true;
    mouse.startX = e.clientX;
    mouse.startY = e.clientY;

    if (e.button === 1 || activeTool === 'pan') return;

    if (activeTool === 'select') {
        const hit = findHitEntity(mouse.worldX, mouse.worldY);
        if (!e.shiftKey) selectedEntities.clear();
        if (hit) {
            if (selectedEntities.has(hit)) selectedEntities.delete(hit);
            else selectedEntities.add(hit);
        }
    } else if (activeTool === 'line') {
        if (toolState.step === 0) {
            toolState.step = 1;
            toolState.data = { p1: { x: mouse.worldX, y: mouse.worldY } };
        } else {
            entities.push({ type: 'LINE', vertices: [toolState.data.p1, { x: mouse.worldX, y: mouse.worldY }] });
            toolState = { step: 0, data: null };
            enableExport();
        }
    } else if (activeTool === 'circle') {
        if (toolState.step === 0) {
            toolState.step = 1;
            toolState.data = { p1: { x: mouse.worldX, y: mouse.worldY } };
        } else {
            const r = Math.hypot(mouse.worldX - toolState.data.p1.x, mouse.worldY - toolState.data.p1.y);
            entities.push({ type: 'CIRCLE', center: toolState.data.p1, radius: r });
            toolState = { step: 0, data: null };
            enableExport();
        }
    } else if (activeTool === 'move') {
        if (selectedEntities.size === 0) return;
        if (toolState.step === 0) {
            toolState.step = 1;
            toolState.data = { start: { x: mouse.worldX, y: mouse.worldY } };
        } else {
            const dx = mouse.worldX - toolState.data.start.x;
            const dy = mouse.worldY - toolState.data.start.y;
            selectedEntities.forEach(ent => translateEntity(ent, dx, dy));
            toolState = { step: 0, data: null };
            enableExport();
        }
    }
}

function onMouseMove(e) {
    const rect = canvas.getBoundingClientRect();
    const w = toWorld(e.clientX - rect.left, e.clientY - rect.top);
    mouse.worldX = w.x;
    mouse.worldY = w.y;

    if (mouse.isDown && (e.buttons & 4 || activeTool === 'pan')) {
        const dx = e.clientX - mouse.startX;
        const dy = e.clientY - mouse.startY;
        view.x -= dx / view.scale;
        view.y += dy / view.scale;
        mouse.startX = e.clientX;
        mouse.startY = e.clientY;
    }
}

function onMouseUp() { mouse.isDown = false; }

function onWheel(e) {
    e.preventDefault();
    const factor = e.deltaY < 0 ? 1.12 : 1 / 1.12;
    const old = view.scale;
    view.scale = Math.max(0.001, Math.min(10000, view.scale * factor));
    view.x = mouse.worldX - (mouse.worldX - view.x) * (old / view.scale);
    view.y = mouse.worldY - (mouse.worldY - view.y) * (old / view.scale);
}

function onKeyDown(e) {
    if (e.key === 'Delete' || e.key === 'Backspace') deleteSelected();
    if (e.key === 'Escape') { toolState = { step: 0, data: null }; selectedEntities.clear(); }
    if (e.key === 'f' || e.key === 'F') fitToScreen();
}

function deleteSelected() {
    if (!selectedEntities.size) return;
    entities = entities.filter(e => !selectedEntities.has(e));
    selectedEntities.clear();
    enableExport();
}

// ---- Hit Test ----
function findHitEntity(x, y) {
    const thr = 8 / view.scale;
    for (let i = entities.length - 1; i >= 0; i--) {
        const ent = entities[i];
        if (ent.type === 'LINE') {
            const v = ent.vertices || [];
            if (v.length >= 2 && distToSeg(x, y, v[0], v[1]) < thr) return ent;
        } else if (ent.type === 'CIRCLE' || ent.type === 'ARC') {
            if (Math.abs(Math.hypot(x - ent.center.x, y - ent.center.y) - ent.radius) < thr) return ent;
        } else if (ent.type === 'LWPOLYLINE' || ent.type === 'POLYLINE') {
            const v = ent.vertices || [];
            for (let j = 0; j < v.length - 1; j++)
                if (distToSeg(x, y, v[j], v[j+1]) < thr) return ent;
            if ((ent.shape || ent.closed) && v.length > 1)
                if (distToSeg(x, y, v[v.length-1], v[0]) < thr) return ent;
        }
    }
    return null;
}

function distToSeg(px, py, a, b) {
    const dx = b.x - a.x, dy = b.y - a.y;
    const l2 = dx*dx + dy*dy;
    if (l2 === 0) return Math.hypot(px-a.x, py-a.y);
    const t = Math.max(0, Math.min(1, ((px-a.x)*dx + (py-a.y)*dy) / l2));
    return Math.hypot(px - (a.x + t*dx), py - (a.y + t*dy));
}

function cloneEntity(e) { return JSON.parse(JSON.stringify(e)); }

function translateEntity(ent, dx, dy) {
    const move = v => { v.x += dx; v.y += dy; };
    if (ent.vertices) ent.vertices.forEach(move);
    if (ent.center) move(ent.center);
    if (ent.start) { move(ent.start); }
    if (ent.end) { move(ent.end); }
}

// ---- File Tabs UI ----
function renderTabs() {
    const container = document.getElementById('file-tabs');
    if (!container) return;
    container.innerHTML = '';
    loadedFiles.forEach((f, i) => {
        const tab = document.createElement('button');
        tab.className = 'file-tab' + (i === activeFileIdx ? ' active' : '');
        tab.textContent = f.name;
        tab.title = f.name;
        tab.addEventListener('click', () => {
            activeFileIdx = i;
            entities = loadedFiles[i].entities;
            selectedEntities.clear();
            renderTabs();
            fitToScreen();
        });
        // Close button
        const x = document.createElement('span');
        x.textContent = ' ×';
        x.style.marginLeft = '6px';
        x.style.opacity = '0.5';
        x.addEventListener('click', ev => {
            ev.stopPropagation();
            loadedFiles.splice(i, 1);
            if (activeFileIdx >= loadedFiles.length) activeFileIdx = Math.max(0, loadedFiles.length - 1);
            if (loadedFiles.length > 0) entities = loadedFiles[activeFileIdx].entities;
            else { entities = []; document.getElementById('export-btn').disabled = true; }
            selectedEntities.clear();
            renderTabs();
            fitToScreen();
        });
        tab.appendChild(x);
        container.appendChild(tab);
    });
}

// ---- DXF I/O ----
function loadFiles(files) {
    let pending = files.length;
    files.forEach(file => {
        const reader = new FileReader();
        reader.onload = evt => {
            try {
                const parser = new DxfParser();
                const dxf = parser.parseSync(evt.target.result);
                const parsed = flattenEntities(dxf);

                // Remove existing entry with same name
                const existing = loadedFiles.findIndex(f => f.name === file.name);
                if (existing >= 0) loadedFiles[existing] = { name: file.name, entities: parsed };
                else { loadedFiles.push({ name: file.name, entities: parsed }); activeFileIdx = loadedFiles.length - 1; }

                entities = loadedFiles[activeFileIdx].entities;
                selectedEntities.clear();
            } catch(err) {
                alert(`Error al parsear "${file.name}": ${err.message}`);
            }
            pending--;
            if (pending === 0) {
                renderTabs();
                fitToScreen();
                enableExport();
            }
        };
        reader.onerror = () => { alert(`No se pudo leer "${file.name}"`); pending--; };
        reader.readAsText(file);
    });
}

/**
 * dxf-parser returns entities directly, but also nested inside blocks.
 * This flattens everything (basic, no INSERT transforms).
 */
function flattenEntities(dxf) {
    let all = (dxf.entities || []).slice();
    // Also extract from blocks (optional: skip *Model_Space as it may duplicate)
    if (dxf.blocks) {
        Object.values(dxf.blocks).forEach(block => {
            if (block.name === '*Model_Space' || block.name === '*PAPER_SPACE') return;
            all = all.concat(block.entities || []);
        });
    }
    // Normalize LINE vertices (dxf-parser uses start/end, not vertices array)
    all.forEach(ent => {
        if (ent.type === 'LINE' && !ent.vertices && ent.start && ent.end) {
            ent.vertices = [
                { x: ent.start.x, y: ent.start.y },
                { x: ent.end.x, y: ent.end.y }
            ];
        }
    });
    return all;
}

function fitToScreen() {
    if (entities.length === 0) return;
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;

    const expand = (x, y) => {
        if (x < minX) minX = x; if (x > maxX) maxX = x;
        if (y < minY) minY = y; if (y > maxY) maxY = y;
    };

    entities.forEach(ent => {
        if (ent.vertices) ent.vertices.forEach(v => expand(v.x, v.y));
        if (ent.center) {
            const r = ent.radius || 0;
            expand(ent.center.x - r, ent.center.y - r);
            expand(ent.center.x + r, ent.center.y + r);
        }
        if (ent.start) expand(ent.start.x, ent.start.y);
        if (ent.end) expand(ent.end.x, ent.end.y);
    });

    if (!isFinite(minX)) return;

    const w = maxX - minX || 1;
    const h = maxY - minY || 1;
    const padding = 40;
    view.x = minX + w / 2;
    view.y = minY + h / 2;
    view.scale = Math.min(
        (canvas.width - padding * 2) / w,
        (canvas.height - padding * 2) / h
    );
    if (!isFinite(view.scale) || view.scale <= 0) view.scale = 1;
}

function enableExport() { document.getElementById('export-btn').disabled = false; }

// ---- DXF Export ----
function handleExport() {
    const lines = ['0\nSECTION', '2\nHEADER', '9\n$ACADVER', '1\nAC1015', '0\nENDSEC',
                   '0\nSECTION', '2\nENTITIES'];

    entities.forEach(e => {
        if (e.type === 'LINE') {
            const v = e.vertices || [];
            if (v.length < 2) return;
            lines.push(`0\nLINE\n8\n0\n10\n${v[0].x.toFixed(6)}\n20\n${v[0].y.toFixed(6)}\n30\n0.0\n11\n${v[1].x.toFixed(6)}\n21\n${v[1].y.toFixed(6)}\n31\n0.0`);
        } else if (e.type === 'CIRCLE') {
            lines.push(`0\nCIRCLE\n8\n0\n10\n${e.center.x.toFixed(6)}\n20\n${e.center.y.toFixed(6)}\n30\n0.0\n40\n${e.radius.toFixed(6)}`);
        } else if (e.type === 'ARC') {
            lines.push(`0\nARC\n8\n0\n10\n${e.center.x.toFixed(6)}\n20\n${e.center.y.toFixed(6)}\n30\n0.0\n40\n${e.radius.toFixed(6)}\n50\n${e.startAngle.toFixed(6)}\n51\n${e.endAngle.toFixed(6)}`);
        } else if (e.type === 'LWPOLYLINE' || e.type === 'POLYLINE') {
            const verts = e.vertices || [];
            let s = `0\nLWPOLYLINE\n8\n0\n90\n${verts.length}\n70\n${(e.shape || e.closed) ? 1 : 0}`;
            verts.forEach(v => {
                s += `\n10\n${v.x.toFixed(6)}\n20\n${v.y.toFixed(6)}`;
                if (v.bulge) s += `\n42\n${v.bulge.toFixed(6)}`;
            });
            lines.push(s);
        }
    });

    lines.push('0\nENDSEC', '0\nEOF');
    const blob = new Blob([lines.join('\n')], { type: 'application/dxf' });
    const url = URL.createObjectURL(blob);
    const a = Object.assign(document.createElement('a'), { href: url, download: 'foglesting_editado.dxf' });
    document.body.appendChild(a); a.click(); document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

// ---- Background Animation ----
function initBgCanvas() {
    const bg = document.getElementById('bg-canvas');
    if (!bg) return;
    const c = bg.getContext('2d');
    const resize = () => { bg.width = innerWidth; bg.height = innerHeight; };
    resize(); window.addEventListener('resize', resize);
    let t = 0;
    (function anim() {
        c.clearRect(0, 0, bg.width, bg.height);
        c.fillStyle = 'rgba(232,91,46,0.04)';
        for (let i = 0; i < 15; i++) {
            c.beginPath();
            c.arc((Math.sin(t + i * 0.7) * 0.5 + 0.5) * bg.width,
                  (Math.cos(t * 0.4 + i * 0.9) * 0.5 + 0.5) * bg.height, 60, 0, Math.PI * 2);
            c.fill();
        }
        t += 0.008;
        requestAnimationFrame(anim);
    })();
}

window.addEventListener('DOMContentLoaded', init);
