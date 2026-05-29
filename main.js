import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import { getFirestore, doc, setDoc, getDoc, updateDoc, increment, collection, addDoc, serverTimestamp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-firestore.js";

// Firebase Configuration from User
const firebaseConfig = {
  apiKey: "AIzaSyC-hdDg1NAcTli4Pck0IwYomPpMF-tLO9s",
  authDomain: "foglesting.firebaseapp.com",
  projectId: "foglesting",
  storageBucket: "foglesting.firebasestorage.app",
  messagingSenderId: "340614706642",
  appId: "1:340614706642:web:0886f5fc03d6fb5b5e440b",
  measurementId: "G-BTQBKQV6ML"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const db = getFirestore(app);

// =============================================
// Foglar Landing Page — Interactions & Animations
// =============================================

function init() {

    // --- Proximity Morphing Badge ---
    const badge = document.querySelector('.powered-badge');
    if (badge) {
        document.addEventListener('mousemove', (e) => {
            const rect = badge.getBoundingClientRect();
            // Center of the badge
            const centerX = rect.left + rect.width / 2;
            const centerY = rect.top + rect.height / 2;
            
            // Distance from mouse to center
            const dx = e.clientX - centerX;
            const dy = e.clientY - centerY;
            const distance = Math.sqrt(dx * dx + dy * dy);
            
            // Define distances
            const maxDist = 200; // Distance to start morphing
            const minDist = 40;  // Distance to be 100% morphed (approx radius of the final button)
            let progress = 0;
            
            if (distance <= minDist) {
                progress = 1;
            } else if (distance < maxDist) {
                progress = 1 - ((distance - minDist) / (maxDist - minDist));
                // Quadratic easing so the effect is stronger closer to the button
                progress = progress * progress; 
            }
            
            // Update CSS variable for the badge
            badge.style.setProperty('--morph-progress', progress);
        });
    }

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

    // Helper para obtener fecha en formato YYYY-MM-DD local
    function getTodayString() {
        const d = new Date();
        return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
    }

    // --- Download button click tracking ---
    const downloadBtn = document.getElementById('download-btn');
    if (downloadBtn) {
        downloadBtn.addEventListener('click', async () => {
            console.log('Download button clicked');
            try {
                const today = getTodayString();
                const metricsRef = doc(db, 'metrics', today);
                const docSnap = await getDoc(metricsRef);
                if (docSnap.exists()) {
                    await updateDoc(metricsRef, { downloads: increment(1) });
                } else {
                    await setDoc(metricsRef, { page_views: 0, downloads: 1, time_spent: 0, date: today });
                }
            } catch(e) { console.error(e); }
        });
    }

    // --- Page View Tracking ---
    async function trackPageView() {
        if (!sessionStorage.getItem('page_viewed')) {
            try {
                const today = getTodayString();
                const metricsRef = doc(db, 'metrics', today);
                const docSnap = await getDoc(metricsRef);
                if (docSnap.exists()) {
                    await updateDoc(metricsRef, { page_views: increment(1) });
                } else {
                    await setDoc(metricsRef, { page_views: 1, downloads: 0, time_spent: 0, date: today });
                }
                sessionStorage.setItem('page_viewed', 'true');
            } catch(e) { console.error(e); }
        }
    }
    trackPageView();

    // --- Time Spent Tracking ---
    const sessionStartTime = Date.now();
    let timeLogged = false;

    // Cuando el usuario sale de la pestaña o cierra
    document.addEventListener('visibilitychange', () => {
        if (document.visibilityState === 'hidden' && !timeLogged) {
            const timeSpentSeconds = Math.floor((Date.now() - sessionStartTime) / 1000);
            if (timeSpentSeconds > 5) { // solo registramos si estuvo más de 5 segundos
                const today = getTodayString();
                const metricsRef = doc(db, 'metrics', today);
                // Fire and forget, no await because page is closing
                getDoc(metricsRef).then(docSnap => {
                    if (docSnap.exists()) {
                        updateDoc(metricsRef, { time_spent: increment(timeSpentSeconds) });
                    } else {
                        setDoc(metricsRef, { page_views: 1, downloads: 0, time_spent: timeSpentSeconds, date: today });
                    }
                });
            }
            // timeLogged = true; // Si vuelve, queremos contar el nuevo tiempo? Mejor dejarlo acumular.
        }
    });

    // --- Feedback Form Handling ---
    const feedbackForm = document.getElementById('feedback-form');
    const feedbackStatus = document.getElementById('feedback-status');
    const btnSendFeedback = document.getElementById('btn-send-feedback');

    if (feedbackForm) {
        feedbackForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            btnSendFeedback.disabled = true;
            btnSendFeedback.innerHTML = 'Enviando...';
            feedbackStatus.style.color = 'var(--cream)';
            feedbackStatus.textContent = '';

            const name = document.getElementById('fb-name').value;
            const email = document.getElementById('fb-email').value;
            const type = document.getElementById('fb-type').value;
            const msg = document.getElementById('fb-msg').value;

            try {
                await addDoc(collection(db, 'messages'), {
                    name: name,
                    email: email || 'No especificado',
                    type: type,
                    message: msg,
                    timestamp: serverTimestamp()
                });

                // Enviar notificación a WhatsApp vía nuestra API en Vercel
                try {
                    await fetch('/api/whatsapp', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ name, email: email || 'No especificado', type, message: msg })
                    });
                } catch (wspError) {
                    console.error("No se pudo enviar la notificación de WhatsApp:", wspError);
                    // Si falla el WhatsApp no importa, igual le mostramos éxito al usuario porque se guardó en BD.
                }

                feedbackForm.reset();
                feedbackStatus.style.color = '#50c878';
                feedbackStatus.textContent = '¡Mensaje enviado con éxito! Gracias por tu feedback.';
            } catch(error) {
                console.error("Error submitting form", error);
                feedbackStatus.style.color = '#dc3545';
                feedbackStatus.textContent = 'Error al enviar el mensaje. Intenta nuevamente más tarde.';
            } finally {
                btnSendFeedback.disabled = false;
                btnSendFeedback.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="22" y1="2" x2="11" y2="13"/><polygon points="22 2 15 22 11 13 2 9 22 2"/></svg> Enviar Feedback';
            }
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

}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}

// =============================================
// Genetic Algorithm Population Animation
// =============================================
(function () {
    const cv = document.getElementById('nesting-anim');
    if (!cv) return;
    const cx = cv.getContext('2d');
    const W = 700, H = 220;
    cv.width = W; cv.height = H;

    const FIRE      = '#e85b2e';
    const FIRE_RGBA = (a) => `rgba(232,91,46,${a})`;
    const CREAM     = (a) => `rgba(255,223,184,${a})`;
    const GREEN     = (a) => `rgba(80,200,120,${a})`;
    const RED       = (a) => `rgba(220,60,60,${a})`;

    // --- Tiny shapes to draw inside each candidate node ---
    const SHAPES = [
        (x,y,s) => { cx.beginPath(); cx.rect(x,y,s,s*0.6); },
        (x,y,s) => { cx.beginPath(); cx.moveTo(x,y); cx.lineTo(x+s,y); cx.lineTo(x+s,y+s*0.45); cx.lineTo(x+s*0.45,y+s*0.45); cx.lineTo(x+s*0.45,y+s); cx.lineTo(x,y+s); cx.closePath(); },
        (x,y,s) => { cx.beginPath(); cx.moveTo(x+s/2,y); cx.lineTo(x+s,y+s); cx.lineTo(x,y+s); cx.closePath(); },
        (x,y,s) => { cx.beginPath(); cx.moveTo(x+s/2,y); cx.lineTo(x+s,y+s/2); cx.lineTo(x+s/2,y+s); cx.lineTo(x,y+s/2); cx.closePath(); },
        (x,y,s) => { cx.beginPath(); cx.arc(x+s/2,y+s/2,s/2,0,Math.PI*2); },
    ];

    // Candidate solutions: each has a fitness and a random arrangement of tiny pieces
    const candidates = [
        { cx: 90,  cy: 60,  fitness: 0.61, label: '61%', color: RED,   pieces: [{s:0,x:2,y:2,sz:14},{s:2,x:18,y:3,sz:10},{s:1,x:5,y:18,sz:12},{s:3,x:22,y:17,sz:10}] },
        { cx: 90,  cy: 155, fitness: 0.74, label: '74%', color: CREAM,  pieces: [{s:1,x:2,y:3,sz:13},{s:0,x:17,y:2,sz:12},{s:4,x:3,y:18,sz:10},{s:2,x:18,y:18,sz:12}] },
        { cx: 230, cy: 45,  fitness: 0.68, label: '68%', color: RED,    pieces: [{s:3,x:3,y:3,sz:11},{s:0,x:16,y:2,sz:14},{s:2,x:2,y:17,sz:11},{s:1,x:18,y:18,sz:11}] },
        { cx: 230, cy: 110, fitness: 0.89, label: '89%', color: GREEN,  pieces: [{s:0,x:2,y:2,sz:13},{s:1,x:16,y:2,sz:12},{s:1,x:2,y:17,sz:13},{s:0,x:16,y:17,sz:12}] },
        { cx: 230, cy: 175, fitness: 0.77, label: '77%', color: CREAM,  pieces: [{s:2,x:3,y:2,sz:12},{s:3,x:16,y:3,sz:11},{s:4,x:4,y:18,sz:10},{s:1,x:18,y:17,sz:12}] },
        { cx: 90,  cy: 110, fitness: 0.53, label: '53%', color: RED,    pieces: [{s:4,x:4,y:4,sz:12},{s:2,x:18,y:4,sz:10},{s:3,x:3,y:19,sz:13},{s:4,x:20,y:18,sz:10}] },
    ];

    // Best two (green) → crossover → winner
    const bestA = candidates[3]; // 89%
    const bestB = candidates[4]; // 77%

    // Winner node (right side, the output)
    const winner = {
        cx: W - 80, cy: H / 2, fitness: 0.94, label: '94%',
        pieces: [
            {s:0,x:2, y:2, sz:14}, {s:1,x:17,y:2, sz:13},
            {s:1,x:2, y:17,sz:13}, {s:0,x:17,y:17,sz:14},
        ]
    };

    // Crossover node (center-right)
    const crossNode = { cx: W * 0.63, cy: H / 2 };

    let frame = 0;
    const CYCLE = 600;

    function easeInOut(t) { return t<0.5 ? 2*t*t : -1+(4-2*t)*t; }
    function lerp(a,b,t)  { return a+(b-a)*t; }
    function clamp(v,a,b) { return Math.max(a,Math.min(b,v)); }

    function drawNodeBox(node, alpha, glowColor, labelAlpha) {
        const bw = 36, bh = 36;
        const bx = node.cx - bw/2, by = node.cy - bh/2;

        // Glow
        if (glowColor) {
            cx.shadowColor = glowColor;
            cx.shadowBlur = 18;
        }

        // Box border
        cx.strokeStyle = glowColor ? glowColor.replace(/[\d.]+\)$/, `${alpha})`) : CREAM(alpha * 0.4);
        cx.lineWidth = 1.2;
        cx.strokeRect(bx, by, bw, bh);
        cx.shadowBlur = 0;

        // Fill
        cx.fillStyle = glowColor
            ? glowColor.replace(/[\d.]+\)$/, `${alpha * 0.12})`)
            : `rgba(255,255,255,${alpha * 0.03})`;
        cx.fillRect(bx, by, bw, bh);

        // Tiny pieces inside
        if (node.pieces) {
            node.pieces.forEach(p => {
                cx.save();
                cx.globalAlpha = alpha * 0.85;
                SHAPES[p.s](bx + p.x, by + p.y, p.sz);
                cx.fillStyle = glowColor
                    ? glowColor.replace(/[\d.]+\)$/, '0.25)')
                    : CREAM(0.12);
                cx.fill();
                cx.strokeStyle = glowColor
                    ? glowColor.replace(/[\d.]+\)$/, '0.7)')
                    : CREAM(0.4);
                cx.lineWidth = 0.8;
                cx.stroke();
                cx.restore();
            });
        }

        // Label above box
        if (labelAlpha > 0) {
            cx.save();
            cx.globalAlpha = labelAlpha;
            cx.font = `700 9px "DM Sans", sans-serif`;
            cx.textAlign = 'center';
            cx.fillStyle = glowColor
                ? glowColor.replace(/[\d.]+\)$/, '0.9)')
                : CREAM(0.55);
            cx.fillText(node.label, node.cx, by - 5);
            cx.restore();
        }
    }

    function drawLine(x1,y1,x2,y2, alpha, dashed, color) {
        cx.save();
        cx.globalAlpha = alpha;
        cx.strokeStyle = color || CREAM(0.25);
        cx.lineWidth = 0.8;
        if (dashed) cx.setLineDash([3,4]);
        cx.beginPath(); cx.moveTo(x1,y1); cx.lineTo(x2,y2); cx.stroke();
        cx.setLineDash([]);
        cx.restore();
    }

    // Animated particle along a line
    function drawParticle(x1,y1,x2,y2, progress, color) {
        const px = lerp(x1,x2,progress);
        const py = lerp(y1,y2,progress);
        cx.beginPath(); cx.arc(px,py,2.5,0,Math.PI*2);
        cx.fillStyle = color || FIRE;
        cx.shadowColor = color || FIRE;
        cx.shadowBlur = 8;
        cx.fill();
        cx.shadowBlur = 0;
    }

    function drawLabel(text, x, y, alpha, size=8) {
        cx.save();
        cx.globalAlpha = alpha;
        cx.fillStyle = CREAM(0.3);
        cx.font = `500 ${size}px "DM Sans", sans-serif`;
        cx.textAlign = 'center';
        cx.letterSpacing = '0.08em';
        cx.fillText(text.toUpperCase(), x, y);
        cx.restore();
    }

    // Eliminated X mark
    function drawX(node, alpha) {
        const r = 8;
        cx.save();
        cx.globalAlpha = alpha;
        cx.strokeStyle = RED(0.9);
        cx.lineWidth = 1.8;
        cx.beginPath();
        cx.moveTo(node.cx - r, node.cy - r); cx.lineTo(node.cx + r, node.cy + r);
        cx.moveTo(node.cx + r, node.cy - r); cx.lineTo(node.cx - r, node.cy + r);
        cx.stroke();
        cx.restore();
    }

    function render() {
        cx.clearRect(0, 0, W, H);
        frame++;
        const t = (frame % CYCLE) / CYCLE;

        // Phase definitions
        // 0.00-0.20 → population appears
        // 0.20-0.42 → fitness lines + evaluation sparks
        // 0.42-0.58 → weaker ones eliminated, best two highlighted
        // 0.58-0.75 → crossover lines + particle flow to center node
        // 0.75-0.92 → winner emerges on right
        // 0.92-1.00 → fade out

        const fadeOut = t > 0.92 ? 1 - (t - 0.92) / 0.08 : 1;
        const masterAlpha = t < 0.06 ? t / 0.06 : fadeOut;

        // ── ZONE LABELS ──
        drawLabel('Población inicial', 90, H - 6, masterAlpha * 0.5);
        drawLabel('Evaluación', 255, H - 6, masterAlpha * 0.5);
        drawLabel('Crossover', W * 0.63, H - 6, clamp((t - 0.55) / 0.08, 0, 1) * masterAlpha * 0.5);
        drawLabel('Mejor solución', W - 80, H - 6, clamp((t - 0.72) / 0.08, 0, 1) * masterAlpha * 0.5);

        // ── SEPARATOR LINES ──
        const sepAlpha = masterAlpha * 0.06;
        cx.strokeStyle = CREAM(sepAlpha);
        cx.lineWidth = 1;
        [[165,0,165,H],[W*0.52,0,W*0.52,H],[W*0.74,0,W*0.74,H]].forEach(([x1,y1,x2,y2])=>{
            cx.beginPath(); cx.moveTo(x1,y1); cx.lineTo(x2,y2); cx.stroke();
        });

        // ── PHASE 0: POPULATION APPEARS ──
        const popAlpha = clamp(t / 0.18, 0, 1) * masterAlpha;

        // weak nodes (red border)
        const weakNodes = [candidates[0], candidates[2], candidates[5]];
        const strongNodes = [candidates[1], candidates[3], candidates[4]];

        // Eliminated after phase 0.42
        const elimT = clamp((t - 0.42) / 0.10, 0, 1);
        const weakAlpha = popAlpha * (1 - elimT);

        weakNodes.forEach(n => {
            drawNodeBox(n, weakAlpha, RED(0.6), weakAlpha * 0.9);
            if (elimT > 0.3) drawX(n, elimT * masterAlpha);
        });

        // Strong nodes stay
        const bestHighlight = clamp((t - 0.38) / 0.10, 0, 1);
        strongNodes.forEach(n => {
            const isTop = n === bestA || n === bestB;
            const glow = isTop ? GREEN(bestHighlight) : null;
            const a = popAlpha;
            drawNodeBox(n, a, glow, a * 0.9);
        });

        // ── PHASE 1: EVALUATION LINES (population → center zone) ──
        if (t > 0.15 && t < 0.65) {
            const evalA = clamp((t - 0.15) / 0.12, 0, 1) * masterAlpha;
            candidates.forEach(n => {
                // line from each node toward center fitness bar
                drawLine(n.cx + 18, n.cy, 168, n.cy, evalA * 0.4, true);
            });

            // Fitness spark particles flowing right
            if (t > 0.20 && t < 0.55) {
                const prog = ((t - 0.20) / 0.35) % 1;
                candidates.forEach((n, i) => {
                    const p = (prog + i * 0.17) % 1;
                    drawParticle(n.cx + 18, n.cy, 230, n.cy, p,
                        n.fitness > 0.75 ? GREEN(0.9) : RED(0.8));
                });
            }
        }

        // ── PHASE 2: CROSSOVER ──
        if (t > 0.55) {
            const crossA = clamp((t - 0.55) / 0.10, 0, 1) * masterAlpha;
            // Lines from best two to crossover node
            drawLine(bestA.cx + 18, bestA.cy, crossNode.cx, crossNode.cy, crossA * 0.7, false, GREEN(0.6));
            drawLine(bestB.cx + 18, bestB.cy, crossNode.cx, crossNode.cy, crossA * 0.7, false, GREEN(0.6));

            // Particles flowing into crossover node
            if (t > 0.58 && t < 0.78) {
                const p = ((t - 0.58) / 0.20) % 1;
                drawParticle(bestA.cx+18, bestA.cy, crossNode.cx, crossNode.cy, p, GREEN(0.9));
                drawParticle(bestB.cx+18, bestB.cy, crossNode.cx, crossNode.cy, (p+0.5)%1, FIRE_RGBA(0.9));
            }

            // Crossover node (spinning indicator)
            const spin = frame * 0.05;
            const cR = 18 + 2 * Math.sin(frame * 0.1);
            cx.save();
            cx.globalAlpha = crossA;
            cx.strokeStyle = GREEN(0.6);
            cx.lineWidth = 1.5;
            cx.shadowColor = '#50c878'; cx.shadowBlur = 12;
            cx.beginPath(); cx.arc(crossNode.cx, crossNode.cy, cR, 0, Math.PI*2);
            cx.stroke(); cx.shadowBlur = 0;
            // Rotating dots inside
            for (let i = 0; i < 4; i++) {
                const a = spin + i * Math.PI/2;
                const dx = crossNode.cx + 8 * Math.cos(a);
                const dy = crossNode.cy + 8 * Math.sin(a);
                cx.beginPath(); cx.arc(dx, dy, 2, 0, Math.PI*2);
                cx.fillStyle = i % 2 === 0 ? GREEN(0.9) : FIRE_RGBA(0.9);
                cx.fill();
            }
            cx.fillStyle = CREAM(0.25);
            cx.font = '600 7px DM Sans, sans-serif';
            cx.textAlign = 'center';
            cx.fillText('CROSSOVER', crossNode.cx, crossNode.cy + cR + 10);
            cx.restore();
        }

        // ── PHASE 3: WINNER APPEARS ──
        if (t > 0.72) {
            const winA = clamp((t - 0.72) / 0.12, 0, 1) * masterAlpha;

            // Line from crossover to winner
            drawLine(crossNode.cx + 20, crossNode.cy, winner.cx - 18, winner.cy, winA * 0.8, false, GREEN(0.7));

            // Particle flowing to winner
            if (t > 0.74 && t < 0.90) {
                const p = ((t - 0.74) / 0.16) % 1;
                drawParticle(crossNode.cx+20, crossNode.cy, winner.cx-18, winner.cy, p, GREEN(0.9));
            }

            // Draw winner node (bigger, golden glow)
            const bw = 44, bh = 44;
            const bx = winner.cx - bw/2, by = winner.cy - bh/2;
            cx.save();
            cx.globalAlpha = winA;
            cx.shadowColor = FIRE; cx.shadowBlur = 24;
            cx.strokeStyle = FIRE_RGBA(0.9);
            cx.lineWidth = 1.8;
            cx.strokeRect(bx, by, bw, bh);
            cx.shadowBlur = 0;
            cx.fillStyle = FIRE_RGBA(0.08);
            cx.fillRect(bx, by, bw, bh);
            winner.pieces.forEach(p => {
                SHAPES[p.s](bx + p.x, by + p.y, p.sz);
                cx.fillStyle = FIRE_RGBA(0.28);
                cx.fill();
                cx.strokeStyle = FIRE_RGBA(0.85);
                cx.lineWidth = 1;
                cx.stroke();
            });
            // Badge
            cx.fillStyle = FIRE;
            cx.beginPath();
            cx.roundRect(winner.cx - 20, by - 18, 40, 14, 7);
            cx.fill();
            cx.fillStyle = '#fff';
            cx.font = 'bold 8px DM Sans, sans-serif';
            cx.textAlign = 'center';
            cx.fillText('94% efic.', winner.cx, by - 8);
            cx.restore();
        }

        requestAnimationFrame(render);
    }

    render();
})();


