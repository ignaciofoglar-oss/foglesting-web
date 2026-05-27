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
