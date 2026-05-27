// =============================================
// Foglar Landing Page — Interactions & Animations
// =============================================

document.addEventListener('DOMContentLoaded', () => {

    // --- Navbar scroll effect ---
    const navbar = document.getElementById('navbar');
    let lastScroll = 0;

    function handleNavScroll() {
        const currentScroll = window.scrollY;
        if (currentScroll > 50) {
            navbar.classList.add('scrolled');
        } else {
            navbar.classList.remove('scrolled');
        }
        lastScroll = currentScroll;
    }

    window.addEventListener('scroll', handleNavScroll, { passive: true });
    handleNavScroll(); // initial check

    // --- Mobile menu toggle ---
    const navToggle = document.getElementById('nav-toggle');
    const navLinks = document.getElementById('nav-links');

    navToggle.addEventListener('click', () => {
        navToggle.classList.toggle('active');
        navLinks.classList.toggle('active');
        document.body.style.overflow = navLinks.classList.contains('active') ? 'hidden' : '';
    });

    // Close mobile menu when clicking a link
    navLinks.querySelectorAll('a').forEach(link => {
        link.addEventListener('click', () => {
            navToggle.classList.remove('active');
            navLinks.classList.remove('active');
            document.body.style.overflow = '';
        });
    });

    // --- Scroll reveal animations ---
    const revealElements = document.querySelectorAll('.reveal, .reveal-left, .reveal-right');

    const revealObserver = new IntersectionObserver((entries) => {
        entries.forEach((entry, index) => {
            if (entry.isIntersecting) {
                // Stagger animation for sibling elements
                const delay = entry.target.dataset.delay || 0;
                setTimeout(() => {
                    entry.target.classList.add('visible');
                }, delay);
                revealObserver.unobserve(entry.target);
            }
        });
    }, {
        threshold: 0.15,
        rootMargin: '0px 0px -50px 0px'
    });

    revealElements.forEach((el, i) => {
        // Add stagger delays for feature cards and steps
        const parent = el.parentElement;
        if (parent && (parent.classList.contains('features-grid') || parent.classList.contains('steps-container'))) {
            const siblings = Array.from(parent.querySelectorAll('.reveal, .reveal-left, .reveal-right'));
            const siblingIndex = siblings.indexOf(el);
            el.dataset.delay = siblingIndex * 150;
        }
        revealObserver.observe(el);
    });

    // --- Active nav link highlighting ---
    const sections = document.querySelectorAll('section[id]');
    const navLinksAll = document.querySelectorAll('.nav-links a:not(.nav-cta)');

    function highlightNav() {
        const scrollPos = window.scrollY + 100;

        sections.forEach(section => {
            const top = section.offsetTop;
            const height = section.offsetHeight;
            const id = section.getAttribute('id');

            if (scrollPos >= top && scrollPos < top + height) {
                navLinksAll.forEach(link => {
                    link.style.color = '';
                    if (link.getAttribute('href') === '#' + id) {
                        link.style.color = '#e85b2e';
                    }
                });
            }
        });
    }

    window.addEventListener('scroll', highlightNav, { passive: true });

    // --- Smooth scroll for anchor links (fallback) ---
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', function (e) {
            const targetId = this.getAttribute('href');
            if (targetId === '#') return;
            
            const target = document.querySelector(targetId);
            if (target) {
                e.preventDefault();
                target.scrollIntoView({
                    behavior: 'smooth',
                    block: 'start'
                });
            }
        });
    });

    // --- Parallax effect on hero background ---
    const heroBg = document.querySelector('.hero-bg img');
    if (heroBg) {
        window.addEventListener('scroll', () => {
            const scrolled = window.scrollY;
            if (scrolled < window.innerHeight) {
                heroBg.style.transform = `scale(1.1) translateY(${scrolled * 0.3}px)`;
            }
        }, { passive: true });
    }

    // --- Feature cards hover tilt effect ---
    const featureCards = document.querySelectorAll('.feature-card');
    featureCards.forEach(card => {
        card.addEventListener('mousemove', (e) => {
            const rect = card.getBoundingClientRect();
            const x = e.clientX - rect.left;
            const y = e.clientY - rect.top;
            const centerX = rect.width / 2;
            const centerY = rect.height / 2;
            const rotateX = (y - centerY) / 20;
            const rotateY = (centerX - x) / 20;
            card.style.transform = `translateY(-8px) perspective(800px) rotateX(${rotateX}deg) rotateY(${rotateY}deg)`;
        });

        card.addEventListener('mouseleave', () => {
            card.style.transform = '';
        });
    });

    // --- Typing effect for hero subtitle (optional enhancement) ---
    // Disabled for simplicity, but available if needed

    // --- Counter animation for stats (if added) ---
    function animateCounter(element, target, duration = 1500) {
        let start = 0;
        const step = target / (duration / 16);
        const counter = setInterval(() => {
            start += step;
            if (start >= target) {
                element.textContent = target;
                clearInterval(counter);
            } else {
                element.textContent = Math.floor(start);
            }
        }, 16);
    }

    // --- Download button click tracking ---
    const downloadBtn = document.getElementById('download-btn');
    if (downloadBtn) {
        downloadBtn.addEventListener('click', () => {
            console.log('Download button clicked — Foglesting v1.0');
            // Can add analytics here later
        });
    }

    console.log('🔥 Foglar Landing Page loaded successfully');

    // =============================================
    // Neural-Fire Canvas Animation
    // =============================================
    const canvas = document.getElementById('neural-canvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    const FIRE_COLORS = [
        'rgba(232, 91, 46, ',   // #e85b2e
        'rgba(255, 160, 60, ',  // warm orange
        'rgba(255, 220, 100, ', // ember yellow
        'rgba(200, 50, 20, ',   // deep red-fire
    ];

    let W, H, nodes, edges, particles;

    function resize() {
        W = canvas.width  = canvas.offsetWidth;
        H = canvas.height = canvas.offsetHeight;
        init();
    }

    function randomFire(alpha) {
        const c = FIRE_COLORS[Math.floor(Math.random() * FIRE_COLORS.length)];
        return c + alpha + ')';
    }

    function init() {
        const count = Math.min(Math.floor((W * H) / 18000), 55);
        nodes = Array.from({ length: count }, () => ({
            x: Math.random() * W,
            y: Math.random() * H,
            r: 2 + Math.random() * 3,
            vx: (Math.random() - 0.5) * 0.35,
            vy: (Math.random() - 0.5) * 0.35,
            pulse: Math.random() * Math.PI * 2,
            pulseSpeed: 0.02 + Math.random() * 0.025,
            colorIdx: Math.floor(Math.random() * FIRE_COLORS.length),
        }));

        edges = [];
        const maxDist = Math.min(W, H) * 0.28;
        for (let i = 0; i < nodes.length; i++) {
            for (let j = i + 1; j < nodes.length; j++) {
                const dx = nodes[i].x - nodes[j].x;
                const dy = nodes[i].y - nodes[j].y;
                if (Math.sqrt(dx * dx + dy * dy) < maxDist) {
                    edges.push([i, j]);
                }
            }
        }

        particles = [];
    }

    function spawnParticle(x, y) {
        particles.push({
            x, y,
            vx: (Math.random() - 0.5) * 1.2,
            vy: -0.5 - Math.random() * 1.5,
            life: 1,
            decay: 0.012 + Math.random() * 0.018,
            r: 1 + Math.random() * 2,
            colorIdx: Math.floor(Math.random() * FIRE_COLORS.length),
        });
    }

    let frame = 0;

    function draw() {
        ctx.clearRect(0, 0, W, H);
        frame++;

        // Rebuild edges dynamically
        const maxDist = Math.min(W, H) * 0.28;
        edges = [];
        for (let i = 0; i < nodes.length; i++) {
            for (let j = i + 1; j < nodes.length; j++) {
                const dx = nodes[i].x - nodes[j].x;
                const dy = nodes[i].y - nodes[j].y;
                const dist = Math.sqrt(dx * dx + dy * dy);
                if (dist < maxDist) edges.push([i, j, dist, maxDist]);
            }
        }

        // Draw edges
        for (const [i, j, dist, maxD] of edges) {
            const a = nodes[i], b = nodes[j];
            const alpha = (1 - dist / maxD) * 0.22;
            const grad = ctx.createLinearGradient(a.x, a.y, b.x, b.y);
            grad.addColorStop(0, FIRE_COLORS[a.colorIdx] + alpha + ')');
            grad.addColorStop(1, FIRE_COLORS[b.colorIdx] + alpha + ')');
            ctx.beginPath();
            ctx.moveTo(a.x, a.y);
            ctx.lineTo(b.x, b.y);
            ctx.strokeStyle = grad;
            ctx.lineWidth = 0.7;
            ctx.stroke();

            // Occasionally spawn an ember along the edge
            if (frame % 6 === 0 && Math.random() < 0.04) {
                const t = Math.random();
                spawnParticle(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
            }
        }

        // Draw nodes
        for (const n of nodes) {
            n.pulse += n.pulseSpeed;
            const glow = 0.5 + 0.5 * Math.sin(n.pulse);
            const r = n.r + glow * 2;

            // Outer glow
            const g = ctx.createRadialGradient(n.x, n.y, 0, n.x, n.y, r * 4);
            g.addColorStop(0, FIRE_COLORS[n.colorIdx] + (0.35 * glow) + ')');
            g.addColorStop(1, FIRE_COLORS[n.colorIdx] + '0)');
            ctx.beginPath();
            ctx.arc(n.x, n.y, r * 4, 0, Math.PI * 2);
            ctx.fillStyle = g;
            ctx.fill();

            // Core dot
            ctx.beginPath();
            ctx.arc(n.x, n.y, r, 0, Math.PI * 2);
            ctx.fillStyle = FIRE_COLORS[n.colorIdx] + (0.7 + 0.3 * glow) + ')';
            ctx.fill();

            // Move
            n.x += n.vx;
            n.y += n.vy;
            if (n.x < 0 || n.x > W) n.vx *= -1;
            if (n.y < 0 || n.y > H) n.vy *= -1;

            // Spawn ember from hot nodes occasionally
            if (Math.random() < 0.003) spawnParticle(n.x, n.y);
        }

        // Draw & update particles (embers)
        for (let p = particles.length - 1; p >= 0; p--) {
            const pt = particles[p];
            ctx.beginPath();
            ctx.arc(pt.x, pt.y, pt.r * pt.life, 0, Math.PI * 2);
            ctx.fillStyle = FIRE_COLORS[pt.colorIdx] + pt.life + ')';
            ctx.fill();
            pt.x += pt.vx;
            pt.y += pt.vy;
            pt.vy -= 0.03; // float up
            pt.life -= pt.decay;
            if (pt.life <= 0) particles.splice(p, 1);
        }

        requestAnimationFrame(draw);
    }

    window.addEventListener('resize', resize);
    resize();
    draw();

});

// =============================================
// Nesting Algorithm Mini-Animation (standalone)
// =============================================
(function() {
    const cv = document.getElementById('nesting-anim');
    if (!cv) return;
    const cx = cv.getContext('2d');
    const W = 560, H = 180;
    cv.width = W; cv.height = H;

    const FIRE  = '#e85b2e';
    const CREAM = 'rgba(255,223,184,';
    const DIM   = 'rgba(255,223,184,0.06)';

    // --- DXF shapes definition (left side) ---
    const SHAPES = [
        // L-shape
        (x,y,s,a)=>{ cx.beginPath(); cx.moveTo(x,y); cx.lineTo(x+s,y); cx.lineTo(x+s,y+s*0.4); cx.lineTo(x+s*0.4,y+s*0.4); cx.lineTo(x+s*0.4,y+s); cx.lineTo(x,y+s); cx.closePath(); },
        // Rectangle
        (x,y,s,a)=>{ cx.beginPath(); cx.rect(x,y,s,s*0.55); },
        // Triangle
        (x,y,s,a)=>{ cx.beginPath(); cx.moveTo(x+s/2,y); cx.lineTo(x+s,y+s); cx.lineTo(x,y+s); cx.closePath(); },
        // Diamond
        (x,y,s,a)=>{ cx.beginPath(); cx.moveTo(x+s/2,y); cx.lineTo(x+s,y+s/2); cx.lineTo(x+s/2,y+s); cx.lineTo(x,y+s/2); cx.closePath(); },
        // Pentagon
        (x,y,s,a)=>{ cx.beginPath(); for(let i=0;i<5;i++){const ang=-Math.PI/2+i*2*Math.PI/5; cx.lineTo(x+s/2+s/2*Math.cos(ang),y+s/2+s/2*Math.sin(ang));} cx.closePath(); },
    ];

    // --- Layout for output (right side): pre-packed grid ---
    const outputLayout = [
        {shape:0,x:0,  y:0,  s:26},
        {shape:1,x:30, y:0,  s:26},
        {shape:2,x:60, y:0,  s:26},
        {shape:0,x:0,  y:30, s:26},
        {shape:3,x:30, y:30, s:26},
        {shape:4,x:60, y:30, s:26},
        {shape:1,x:0,  y:58, s:26},
        {shape:2,x:30, y:58, s:26},
        {shape:0,x:60, y:58, s:26},
    ];

    // Input pieces (scattered, left zone)
    const inputPieces = [
        {shape:0,x:28,y:20,s:26}, {shape:1,x:10,y:60,s:26},
        {shape:2,x:40,y:100,s:26},{shape:3,x:5, y:140,s:26},
        {shape:4,x:50,y:52,s:26}, {shape:1,x:20,y:130,s:26},
        {shape:0,x:55,y:88,s:22}, {shape:2,x:8, y:88,s:22},
        {shape:3,x:45,y:148,s:20},
    ];

    // State
    let t = 0; // 0..1 overall cycle (4 seconds)
    const CYCLE = 600; // frames per full cycle (~10 seconds at 60fps)
    let frame = 0;

    // Core glow (center)
    const CX = W / 2, CY = H / 2;

    function easeInOut(t) { return t < 0.5 ? 2*t*t : -1+(4-2*t)*t; }
    function lerp(a,b,t) { return a + (b-a)*t; }

    function drawShape(shapeFn, x, y, s, alpha, glow, filled) {
        cx.save();
        shapeFn(x - s/2, y - s/2, s);
        if (glow) {
            cx.shadowColor = FIRE;
            cx.shadowBlur = 12;
        }
        if (filled) {
            cx.fillStyle = `rgba(232,91,46,${alpha * 0.35})`;
            cx.fill();
        }
        cx.strokeStyle = `rgba(232,91,46,${alpha})`;
        cx.lineWidth = 1.5;
        cx.stroke();
        cx.restore();
    }

    function drawZoneLabel(text, x, y) {
        cx.fillStyle = CREAM + '0.22)';
        cx.font = '500 9px DM Sans, sans-serif';
        cx.letterSpacing = '0.1em';
        cx.textAlign = 'center';
        cx.fillText(text.toUpperCase(), x, y);
    }

    function render() {
        cx.clearRect(0, 0, W, H);

        frame++;
        t = (frame % CYCLE) / CYCLE;

        // --- Background zones ---
        // Left zone
        cx.fillStyle = 'rgba(255,223,184,0.02)';
        cx.beginPath(); cx.rect(0, 0, W*0.3, H); cx.fill();
        cx.strokeStyle = DIM;
        cx.lineWidth = 1;
        cx.beginPath(); cx.moveTo(W*0.3, 0); cx.lineTo(W*0.3, H); cx.stroke();

        // Right zone
        cx.fillStyle = 'rgba(232,91,46,0.03)';
        cx.beginPath(); cx.rect(W*0.7, 0, W*0.3, H); cx.fill();
        cx.strokeStyle = DIM;
        cx.beginPath(); cx.moveTo(W*0.7, 0); cx.lineTo(W*0.7, H); cx.stroke();

        drawZoneLabel('Piezas DXF', W*0.15, H-8);
        drawZoneLabel('Algoritmo Genético', W*0.5, H-8);
        drawZoneLabel('Acomodo óptimo', W*0.85, H-8);

        // --- Core glow (center brain) ---
        const coreGlow = 0.6 + 0.4 * Math.sin(frame * 0.08);
        const coreR = 28 + coreGlow * 6;

        // Outer halo
        const halo = cx.createRadialGradient(CX, CY, 0, CX, CY, coreR * 2.5);
        halo.addColorStop(0, `rgba(232,91,46,${0.18 * coreGlow})`);
        halo.addColorStop(1, 'rgba(232,91,46,0)');
        cx.beginPath(); cx.arc(CX, CY, coreR * 2.5, 0, Math.PI*2);
        cx.fillStyle = halo; cx.fill();

        // Core ring
        cx.beginPath(); cx.arc(CX, CY, coreR, 0, Math.PI*2);
        cx.strokeStyle = `rgba(232,91,46,${0.5 + 0.5*coreGlow})`;
        cx.lineWidth = 1.5;
        cx.shadowColor = FIRE; cx.shadowBlur = 16;
        cx.stroke(); cx.shadowBlur = 0;

        // Inner DNA icon (🧬 substitute: rotating dot pattern)
        for (let i = 0; i < 6; i++) {
            const ang = frame * 0.04 + i * Math.PI / 3;
            const r = 10;
            const nx = CX + r * Math.cos(ang);
            const ny = CY + r * Math.sin(ang);
            cx.beginPath(); cx.arc(nx, ny, 2, 0, Math.PI*2);
            cx.fillStyle = `rgba(232,91,46,${0.7 + 0.3*Math.sin(frame*0.1 + i)})`;
            cx.shadowColor = FIRE; cx.shadowBlur = 8;
            cx.fill(); cx.shadowBlur = 0;
        }

        // --- Phase breakdown ---
        // Phase 0..0.35: pieces fly from left to center
        // Phase 0.35..0.55: processing (pieces spin inside core)
        // Phase 0.55..0.9: packed result flies to right
        // Phase 0.9..1: fade & reset

        const phase = t;

        // --- INPUT PIECES (left zone → core) ---
        if (phase < 0.55) {
            const flyT = Math.min(phase / 0.35, 1);
            inputPieces.forEach((p, i) => {
                const delay = i / inputPieces.length * 0.6;
                const localT = Math.max(0, Math.min((phase - delay*0.35) / 0.35, 1));
                const ease = easeInOut(localT);

                const startX = W*0.07 + p.x;
                const startY = p.y + 10;
                const endX = CX;
                const endY = CY;

                const px = lerp(startX, endX, ease);
                const py = lerp(startY, endY, ease);
                const alpha = localT < 1 ? (0.7 - ease*0.4) : 0;
                const sc = lerp(p.s, p.s * 0.3, ease);

                if (ease < 1) drawShape(SHAPES[p.shape], px, py, sc, alpha, ease > 0.7, ease > 0.5);
            });
        }

        // --- PROCESSING SPARKS (center, during phase 0.3..0.65) ---
        if (phase > 0.28 && phase < 0.68) {
            const prog = (phase - 0.28) / 0.4;
            for (let i = 0; i < 8; i++) {
                const ang = frame * 0.12 + i * Math.PI / 4;
                const r = 18 + i * 2.5;
                const sx = CX + r * Math.cos(ang);
                const sy = CY + r * Math.sin(ang);
                const a = Math.sin(prog * Math.PI) * (0.5 + 0.5*Math.sin(frame*0.15+i));
                cx.beginPath(); cx.arc(sx, sy, 1.5, 0, Math.PI*2);
                cx.fillStyle = `rgba(255,160,60,${a})`;
                cx.shadowColor = '#ffa03c'; cx.shadowBlur = 6;
                cx.fill(); cx.shadowBlur = 0;
            }

            // Flying labels inside core
            cx.save();
            cx.globalAlpha = Math.sin(prog * Math.PI) * 0.6;
            cx.fillStyle = CREAM+'0.8)';
            cx.font = 'bold 8px DM Sans, sans-serif';
            cx.textAlign = 'center';
            cx.fillText('evaluando...', CX, CY - coreR - 8);
            cx.restore();
        }

        // --- OUTPUT: packed layout flies to right (phase 0.55..0.9) ---
        if (phase > 0.52) {
            const outT = Math.min((phase - 0.52) / 0.38, 1);
            const ease = easeInOut(outT);

            // Sheet border (bounding box of output)
            const sheetX = lerp(CX, W*0.715, ease);
            const sheetY = H/2 - 50;
            const sheetW = 92, sheetH = 90;

            cx.save();
            cx.globalAlpha = ease;
            cx.strokeStyle = `rgba(232,91,46,0.5)`;
            cx.lineWidth = 1;
            cx.setLineDash([3, 3]);
            cx.strokeRect(sheetX - 4, sheetY - 4, sheetW + 8, sheetH + 8);
            cx.setLineDash([]);

            // Pack pieces inside sheet
            outputLayout.forEach((p, i) => {
                const pDelay = i / outputLayout.length * 0.4;
                const localE = easeInOut(Math.max(0, Math.min((outT - pDelay) / 0.6, 1)));
                const px = sheetX + p.x + p.s/2;
                const py = sheetY + p.y + p.s/2;
                cx.globalAlpha = ease * localE;
                drawShape(SHAPES[p.shape], px, py, p.s, 0.85, false, true);
            });

            // "Ahorro 18%" badge
            if (outT > 0.7) {
                const badgeAlpha = (outT - 0.7) / 0.3;
                cx.globalAlpha = badgeAlpha;
                cx.fillStyle = FIRE;
                cx.beginPath();
                const bx = sheetX + sheetW - 2, by = sheetY - 12;
                cx.roundRect(bx - 38, by - 10, 42, 16, 8);
                cx.fill();
                cx.fillStyle = '#fff';
                cx.font = 'bold 8px DM Sans, sans-serif';
                cx.textAlign = 'center';
                cx.fillText('Ahorro 18%', bx - 17, by + 2);
            }

            cx.restore();
        }

        // --- Arrow indicators ---
        // Left arrow: input → core
        if (phase < 0.55) {
            const a = phase < 0.1 ? phase/0.1 : (phase > 0.45 ? (0.55-phase)/0.1 : 1);
            cx.save();
            cx.globalAlpha = a * 0.35;
            cx.strokeStyle = FIRE;
            cx.lineWidth = 1;
            cx.setLineDash([4, 4]);
            cx.beginPath();
            cx.moveTo(W*0.3, CY);
            cx.lineTo(CX - 35, CY);
            cx.stroke();
            cx.setLineDash([]);
            cx.restore();
        }
        // Right arrow: core → output
        if (phase > 0.5) {
            const a = Math.min((phase - 0.5)/0.1, 1) * (phase > 0.88 ? (1-(phase-0.88)/0.12) : 1);
            cx.save();
            cx.globalAlpha = a * 0.35;
            cx.strokeStyle = FIRE;
            cx.lineWidth = 1;
            cx.setLineDash([4, 4]);
            cx.beginPath();
            cx.moveTo(CX + 35, CY);
            cx.lineTo(W*0.7, CY);
            cx.stroke();
            cx.setLineDash([]);
            cx.restore();
        }

        requestAnimationFrame(render);
    }

    render();
})();

