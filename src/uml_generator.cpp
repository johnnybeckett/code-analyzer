#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include <json/json.h> // Assuming we have JSON library for parsing

/**
 * @brief UML Class Diagram Generator
 *
 * This tool converts JSON output from the code analyzer into a self-contained
 * documentation page that supports:
 * - A 3D UML class diagram: class boxes with name/attribute/method
 *   compartments and generalization arrows between related classes
 * - Correct UML notation: visibility glyphs (+ - #), underlined static
 *   members, italic virtual members, italic abstract class names
 * - 3D rotation, zooming, and panning of the class diagram
 * - Filtering classes by name or regex pattern
 * - A side panel listing every class with its methods and member variables
 */
class UMLGenerator {
private:
    std::vector<std::string> input_files;
    std::vector<std::string> hidden_classes_regex;

public:
    /**
     * @brief Constructor
     * @param files List of JSON input files to process
     * @param hide_patterns Regex patterns for classes to hide
     */
    UMLGenerator(const std::vector<std::string>& files,
                 const std::vector<std::string>& hide_patterns)
        : input_files(files), hidden_classes_regex(hide_patterns) {}

    /**
     * @brief Process all input files and generate HTML output
     * @return True if successful, false otherwise
     */
    bool generate() {
        try {
            // Generate HTML with 3D visualization
            std::string html_content = generateHTML();

            // Write to output file
            std::ofstream output_file("uml_diagram.html");
            if (!output_file.is_open()) {
                std::cerr << "Error: Could not create output file uml_diagram.html\n";
                return false;
            }

            output_file << html_content;
            output_file.close();

            std::cout << "UML diagram generated successfully as uml_diagram.html\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error generating UML diagram: " << e.what() << "\n";
            return false;
        }
    }

private:
    /**
     * @brief Generate the self-contained documentation page
     * @return Complete HTML content with the class data embedded
     */
    std::string generateHTML() {
        // The class data is spliced into the template below; a raw string keeps
        // the JavaScript/HTML free of C++ escaping (it only ends at )HTMLDOC").
        std::string html = R"HTMLDOC(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>UML Class Diagram</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
    <style>
        * { box-sizing: border-box; }
        body {
            margin: 0;
            background: #0f172a;
            color: #e2e8f0;
            font-family: Arial, Helvetica, sans-serif;
        }
        #scene {
            position: absolute;
            top: 0; left: 0; bottom: 0;
            right: 380px;
        }
        #sidebar {
            position: absolute;
            top: 0; right: 0; bottom: 0;
            width: 380px;
            overflow-y: auto;
            background: #1e293b;
            border-left: 1px solid #334155;
            padding: 16px;
        }
        #sidebar h2 { margin: 0 0 12px; font-size: 16px; }
        #controls {
            position: absolute;
            top: 12px; left: 12px;
            background: rgba(15, 23, 42, 0.88);
            border: 1px solid #334155;
            padding: 12px;
            border-radius: 8px;
            z-index: 10;
            width: 268px;
        }
        #controls h3 { margin: 0 0 4px; font-size: 14px; }
        #controls input {
            width: 100%;
            padding: 6px 8px;
            margin: 6px 0;
            border-radius: 6px;
            border: 1px solid #475569;
            background: #0f172a;
            color: #e2e8f0;
        }
        #controls button {
            padding: 6px 12px;
            margin-right: 6px;
            border: 0;
            border-radius: 6px;
            background: #4285F4;
            color: #fff;
            cursor: pointer;
        }
        #controls button.secondary { background: #475569; }
        .legend { font-size: 11px; color: #94a3b8; margin: 6px 0 10px; line-height: 1.9; }
        .legend .sym {
            display: inline-block;
            border: 1px solid #475569;
            border-radius: 3px;
            padding: 0 5px;
            color: #e2e8f0;
            font-size: 10px;
            line-height: 1.5;
        }
        .legend .tri {
            width: 0; height: 0;
            border: 5px solid transparent;
            border-bottom: 8px solid #94a3b8;
            display: inline-block;
        }
        .legend .u { text-decoration: underline; }
        .legend .i { font-style: italic; }
        #hint { font-size: 11px; color: #64748b; margin-top: 8px; }
        #stats { margin-top: 6px; font-size: 12px; color: #94a3b8; }
        .classCard {
            background: #0f172a;
            border: 1px solid #334155;
            border-radius: 8px;
            padding: 12px;
            margin-bottom: 12px;
        }
        .classCard h3 { margin: 0 0 4px; font-size: 15px; color: #93c5fd; }
        .classCard .bases { font-size: 12px; color: #94a3b8; margin-bottom: 6px; }
        .classCard h4 {
            margin: 10px 0 4px;
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: #64748b;
        }
        .classCard ul {
            margin: 0;
            padding-left: 18px;
            font-family: "SF Mono", Consolas, monospace;
            font-size: 12px;
        }
        .classCard li { margin: 3px 0; }
        .classCard .ret { color: #7dd3fc; }
        .classCard .nm { color: #fbbf24; }
        .classCard .params { color: #94a3b8; }
        .badge {
            display: inline-block;
            font-size: 10px;
            padding: 1px 6px;
            border-radius: 8px;
            margin-left: 6px;
            vertical-align: middle;
        }
        .badge.static { background: #7c3aed; color: #fff; }
        .badge.vis { background: #334155; color: #cbd5e1; }
        .none { color: #64748b; font-size: 12px; }
    </style>
</head>
<body>
    <div id="scene"></div>
    <div id="controls">
        <h3>UML Class Diagram</h3>
        <div class="legend">
            <div><span class="tri"></span>&nbsp; generalization (extends)</div>
            <div><span class="sym">+</span> public &middot; <span class="sym">-</span> private &middot; <span class="sym">#</span> protected</div>
            <div><span class="sym u">member</span> static &middot; <span class="sym i">member</span> virtual</div>
        </div>
        <h3>Filter classes</h3>
        <input type="text" id="filterInput" placeholder="regex, e.g. ^Parser|Generator$"
               onkeydown="if (event.key === 'Enter') applyFilter()">
        <button onclick="applyFilter()">Apply Filter</button>
        <button class="secondary" onclick="resetFilter()">Reset</button>
        <button class="secondary" onclick="resetView()">Reset View</button>
        <div id="hint">drag: rotate &middot; wheel: zoom &middot; double-click a class to focus</div>
        <div id="stats"></div>
    </div>
    <div id="sidebar"><h2>Classes</h2></div>

    <script>
        // Class data produced by the code analyzer
        const rawClasses = __CLASSES_JSON__;

        const esc = s => String(s).replace(/[&<>"]/g,
            ch => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[ch]));

        // The analyzer may emit several records per class (e.g. one per source
        // file); merge same-named records into a single class.
        function mergeClasses(list) {
            const map = new Map();
            for (const c of list) {
                if (!map.has(c.name)) {
                    map.set(c.name, {
                        name: c.name,
                        namespace: c.namespace || '',
                        bases: new Set(),
                        methods: new Map(),
                        vars: new Map(),
                    });
                }
                const m = map.get(c.name);
                (c.inheritance || []).forEach(b => m.bases.add(b));
                (c.methods || []).forEach(mt =>
                    m.methods.set(JSON.stringify([mt.name, mt.parameters || []]), mt));
                (c.variables || []).forEach(v => m.vars.set(v.name, v));
            }
            return [...map.values()].map(m => ({
                name: m.name,
                namespace: m.namespace,
                inheritance: [...m.bases],
                methods: [...m.methods.values()],
                variables: [...m.vars.values()],
            }));
        }
        const classes = mergeClasses(rawClasses);

        // --- UML notation helpers ---
        const VIS_GLYPH = { public: '+', private: '-', protected: '#' };
        const visGlyph = v => VIS_GLYPH[v] || '+';

        // The parser records "static bool" / "virtual void" style return types;
        // split the modifiers off so they can drive UML styling.
        function cleanReturnType(m) {
            let ret = String(m.return_type || '').trim();
            let stat = !!m['static'];
            let virt = false;
            if (/^static\b/.test(ret)) { ret = ret.replace(/^static\b\s*/, ''); stat = true; }
            if (/^virtual\b/.test(ret)) { ret = ret.replace(/^virtual\b\s*/, ''); virt = true; }
            if (/\bvirtual\b/.test(ret)) { ret = ret.replace(/\bvirtual\b\s*/g, ''); virt = true; }
            ret = ret.replace(/\s+/g, ' ').trim();
            return { ret, stat, virt };
        }

        function attributeRow(v) {
            return {
                glyph: visGlyph(v.visibility),
                text: ((v.type || '').trim() + ' ' + v.name).trim(),
                italic: v.mutability === 'read_only',
                underline: !!v['static'],
            };
        }

        function methodRow(m) {
            const { ret, stat, virt } = cleanReturnType(m);
            const params = (m.parameters || []).map(p => p.trim()).filter(Boolean).join(', ');
            return {
                glyph: visGlyph(m.visibility),
                text: m.name + '(' + params + ')' + (ret ? ' : ' + ret : ''),
                italic: virt,
                underline: stat,
            };
        }

        // --- Render one UML class box (name / attributes / methods) ---
        const TEXT = '15px Arial, Helvetica, sans-serif';
        const NAME = 'bold 17px Arial, Helvetica, sans-serif';
        const PAD = 14, NAME_H = 40, ROW_H = 24, COMP_PAD = 10;

        function drawMemberRow(ctx, row, y) {
            const base = y + ROW_H / 2;
            ctx.font = (row.italic ? 'italic ' : '') + TEXT;
            ctx.fillStyle = '#0f172a';
            ctx.textAlign = 'left';
            ctx.textBaseline = 'middle';
            const pre = row.glyph + '  ';
            ctx.fillText(pre, PAD, base);
            const preW = ctx.measureText(pre).width;
            ctx.fillText(row.text, PAD + preW, base);
            if (row.underline) { // UML: static members are underlined
                const w = ctx.measureText(pre + row.text).width;
                ctx.strokeStyle = '#0f172a';
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.moveTo(PAD, base + 9);
                ctx.lineTo(PAD + w, base + 9);
                ctx.stroke();
            }
        }

        function buildClassCanvas(cls, abstract, external) {
            const attrs = cls.variables.map(attributeRow);
            const meths = cls.methods.map(methodRow);

            const probe = document.createElement('canvas').getContext('2d');
            probe.font = NAME;
            let width = Math.max(140, probe.measureText(cls.name).width);
            probe.font = TEXT;
            attrs.concat(meths).forEach(r => {
                width = Math.max(width, probe.measureText(r.glyph + '  ' + r.text).width);
            });
            width = Math.ceil(width) + PAD * 2;

            const height = NAME_H
                + (attrs.length ? COMP_PAD * 2 + attrs.length * ROW_H : 0)
                + (meths.length ? COMP_PAD * 2 + meths.length * ROW_H : 0);

            const scale = 2; // supersample for crisp text
            const canvas = document.createElement('canvas');
            canvas.width = width * scale;
            canvas.height = height * scale;
            const ctx = canvas.getContext('2d');
            ctx.scale(scale, scale);

            ctx.fillStyle = '#f8fafc';
            ctx.fillRect(0, 0, width, height);
            if (external) ctx.setLineDash([6, 4]);
            ctx.strokeStyle = '#0f172a';
            ctx.lineWidth = 2;
            ctx.strokeRect(1, 1, width - 2, height - 2);
            ctx.setLineDash([]);

            // Name compartment (italic when the class is abstract)
            ctx.fillStyle = '#0f172a';
            ctx.font = (abstract ? 'italic ' : '') + NAME;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(cls.name, width / 2, NAME_H / 2);

            const separator = y => {
                ctx.strokeStyle = '#94a3b8';
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(width, y);
                ctx.stroke();
            };

            let y = NAME_H;
            if (attrs.length || meths.length) separator(y);
            if (attrs.length) {
                y += COMP_PAD;
                attrs.forEach(r => { drawMemberRow(ctx, r, y); y += ROW_H; });
                y += COMP_PAD;
            }
            if (meths.length) {
                separator(y);
                y += COMP_PAD;
                meths.forEach(r => { drawMemberRow(ctx, r, y); y += ROW_H; });
            }

            return { canvas, width, height };
        }

        // --- 3D scene ---
        const WORLD = 0.022;   // canvas px -> world units
        const Y_GAP = 2.6;     // clearance between hierarchy levels
        const Z_GAP = 16;      // depth separation between independent hierarchies

        const container = document.getElementById('scene');
        const renderer = new THREE.WebGLRenderer({ antialias: true });
        renderer.setSize(container.clientWidth, container.clientHeight);
        container.appendChild(renderer.domElement);

        const scene = new THREE.Scene();
        scene.background = new THREE.Color(0x0f172a);
        const diagram = new THREE.Group();
        scene.add(diagram);

        function makeClassBox(cls, abstract, external) {
            const { canvas, width, height } = buildClassCanvas(cls, abstract, external);
            const tex = new THREE.CanvasTexture(canvas);
            tex.anisotropy = renderer.capabilities.getMaxAnisotropy();
            const front = new THREE.MeshBasicMaterial({ map: tex });
            const side = new THREE.MeshBasicMaterial({ color: 0x24344d });
            const mesh = new THREE.Mesh(
                new THREE.BoxGeometry(width * WORLD, height * WORLD, 0.3),
                [side, side, side, side, front, side]);
            return { mesh, side, w: width * WORLD, h: height * WORLD };
        }

        // --- Model: nodes (classes + unresolved base classes) and edges ---
        const byName = new Map(classes.map(c => [c.name, c]));
        const isAbstract = c => c.methods.some(m => /\bvirtual\b/.test(m.return_type || ''));

        const nodes = new Map();
        function getNode(name) {
            if (!nodes.has(name)) {
                const cls = byName.get(name) || null;
                nodes.set(name, {
                    name, cls, external: !cls,
                    parent: null, depth: 0, x: 0, y: 0, z: 0,
                    w: 0, h: 0, mesh: null, sideMat: null,
                });
            }
            return nodes.get(name);
        }

        const edges = [];
        for (const c of classes) {
            const n = getNode(c.name);
            const bases = [...new Set(c.inheritance || [])];
            if (bases.length) n.parent = bases[0]; // primary base drives layout
            bases.forEach(b => { getNode(b); edges.push({ from: n.name, to: b }); });
        }

        const children = new Map();
        for (const n of nodes.values()) {
            if (!n.parent) continue;
            if (!children.has(n.parent)) children.set(n.parent, []);
            children.get(n.parent).push(n);
        }

        // Tidy layout: leaves take consecutive x slots, parents center over
        // their children; each independent hierarchy gets its own depth slice.
        const placed = new Set();
        let cursor = 0;
        function place(n, depth, z) {
            if (placed.has(n.name)) return;
            placed.add(n.name);
            n.depth = depth;
            n.z = z;
            const kids = children.get(n.name) || [];
            if (!kids.length) { n.x = cursor++; }
            else {
                kids.forEach(k => place(k, depth + 1, z));
                n.x = (kids[0].x + kids[kids.length - 1].x) / 2;
            }
        }

        const roots = [...nodes.values()].filter(n => !n.parent);
        roots.forEach((r, i) => {
            r.z = (i - (roots.length - 1) / 2) * Z_GAP;
            place(r, 0, r.z);
        });

        // Build every box (unresolved bases become small dashed stubs)
        for (const n of nodes.values()) {
            const cls = n.cls || { name: n.name, methods: [], variables: [] };
            const box = makeClassBox(cls, !n.external && isAbstract(n.cls), n.external);
            n.w = box.w; n.h = box.h;
            n.mesh = box.mesh;
            n.sideMat = box.side;
        }

        // X pitch keeps adjacent boxes from touching
        const X_GAP = Math.max(...nodes.values().map(n => n.w)) + 3;

        // Stack hierarchy levels vertically with clearance
        const maxDepth = Math.max(...nodes.values().map(n => n.depth));
        const levelH = Array.from({ length: maxDepth + 1 }, () => 0);
        for (const n of nodes.values()) levelH[n.depth] = Math.max(levelH[n.depth], n.h);
        let totalH = 0;
        for (let d = 0; d <= maxDepth; d++) totalH += levelH[d] + Y_GAP;
        let acc = 0;
        const levelY = levelH.map(h => { const y = totalH / 2 - acc - h / 2; acc += h + Y_GAP; return y; });

        let cx = 0, cz = 0;
        for (const n of nodes.values()) { cx += n.x; cz += n.z; }
        cx /= nodes.size; cz /= nodes.size;
        for (const n of nodes.values()) {
            n.x = (n.x - cx) * X_GAP;
            n.y = levelY[n.depth];
            n.z -= cz;
            n.mesh.position.set(n.x, n.y, n.z);
        }

        // --- Generalization arrows: stem + hollow triangle at the superclass ---
        const TRI = 0.9, TRIW = 0.4;
        function makeEdge(n1, n2) {
            const start = new THREE.Vector3(n1.x, n1.y + n1.h / 2, n1.z);
            const tip = new THREE.Vector3(n2.x, n2.y - n2.h / 2, n2.z);
            const d = tip.clone().sub(start);
            if (d.length() < 1e-3) return null;
            d.normalize();
            const base = tip.clone().sub(d.clone().multiplyScalar(TRI));
            let perp = new THREE.Vector3().crossVectors(d, new THREE.Vector3(0, 0, 1));
            if (perp.lengthSq() < 1e-6) perp.set(1, 0, 0);
            perp.normalize();
            const b1 = base.clone().add(perp.clone().multiplyScalar(TRIW));
            const b2 = base.clone().sub(perp.clone().multiplyScalar(TRIW));
            const geo = new THREE.BufferGeometry().setFromPoints(
                [start, base, tip, b1, b1, b2, b2, tip]);
            return new THREE.Line(geo, new THREE.LineBasicMaterial({ color: 0xcbd5e1 }));
        }

        const edgeObjs = edges
            .map(e => ({ from: e.from, to: e.to, line: makeEdge(nodes.get(e.from), nodes.get(e.to)) }))
            .filter(e => e.line);
        edgeObjs.forEach(e => diagram.add(e.line));

        for (const n of nodes.values()) diagram.add(n.mesh);

        // Faint floor grid as a depth cue
        const span = Math.max(...nodes.values().map(n => Math.hypot(n.x, n.y, n.z) + Math.max(n.w, n.h)));
        const grid = new THREE.GridHelper(span * 2.5, 40, 0x273449, 0x1b2536);
        grid.position.y = Math.min(...nodes.values().map(n => n.y - n.h / 2)) - 3;
        diagram.add(grid);

        // --- Camera and interaction ---
        const initDist = span * 1.15 + 10;
        const camera = new THREE.PerspectiveCamera(
            50, container.clientWidth / container.clientHeight, 0.1, 5000);
        camera.position.set(0, 0, initDist);

        let autoRotate = true;
        let dragging = false, lastX = 0, lastY = 0;
        const focus = { active: false, rotX: 0, rotY: 0, dist: initDist };

        container.addEventListener('mousedown', e => {
            autoRotate = false;
            dragging = true;
            lastX = e.clientX; lastY = e.clientY;
            focus.active = false;
        });
        window.addEventListener('mouseup', () => { dragging = false; });
        window.addEventListener('mousemove', e => {
            if (!dragging) return;
            diagram.rotation.y += (e.clientX - lastX) * 0.005;
            diagram.rotation.x = Math.max(-1.2, Math.min(1.2,
                diagram.rotation.x + (e.clientY - lastY) * 0.005));
            lastX = e.clientX; lastY = e.clientY;
        });
        container.addEventListener('wheel', e => {
            e.preventDefault();
            focus.active = false;
            camera.position.z = Math.max(5, Math.min(initDist * 3,
                camera.position.z * (1 + e.deltaY * 0.001)));
        }, { passive: false });

        // Double-click a class to rotate and zoom onto it
        const ray = new THREE.Raycaster();
        container.addEventListener('dblclick', e => {
            const rect = container.getBoundingClientRect();
            const mouse = new THREE.Vector2(
                ((e.clientX - rect.left) / rect.width) * 2 - 1,
                -((e.clientY - rect.top) / rect.height) * 2 + 1);
            ray.setFromCamera(mouse, camera);
            const hits = ray.intersectObjects([...nodes.values()].map(n => n.mesh));
            if (!hits.length) return;
            const n = [...nodes.values()].find(nd => nd.mesh === hits[0].object);
            focusOn(n);
        });

        function focusOn(n) {
            autoRotate = false;
            focus.rotY = Math.atan2(n.x, n.z);
            focus.rotX = Math.atan2(n.y, Math.hypot(n.x, n.z));
            focus.dist = Math.max(6, Math.max(n.w, n.h) * 3.5);
            focus.active = true;
        }

        function resetView() {
            focus.rotX = 0; focus.rotY = 0; focus.dist = initDist; focus.active = true;
        }

        // Hover a sidebar card to highlight the box
        function setHighlight(n, on) {
            if (!n) return;
            n.sideMat.color.set(on ? 0x4285F4 : 0x24344d);
            const s = on ? 1.06 : 1.0;
            n.mesh.scale.set(s, s, s);
        }

        (function animate() {
            requestAnimationFrame(animate);
            if (autoRotate) diagram.rotation.y += 0.002;
            if (focus.active) {
                const k = 0.08;
                diagram.rotation.y += (focus.rotY - diagram.rotation.y) * k;
                diagram.rotation.x += (focus.rotX - diagram.rotation.x) * k;
                camera.position.z += (focus.dist - camera.position.z) * k;
                if (Math.abs(focus.rotY - diagram.rotation.y) < 0.002
                    && Math.abs(focus.rotX - diagram.rotation.x) < 0.002
                    && Math.abs(focus.dist - camera.position.z) < 0.05) {
                    focus.active = false;
                }
            }
            renderer.render(scene, camera);
        })();

        window.addEventListener('resize', () => {
            camera.aspect = container.clientWidth / container.clientHeight;
            camera.updateProjectionMatrix();
            renderer.setSize(container.clientWidth, container.clientHeight);
        });

        // --- Sidebar panel ---
        function signature(m) {
            const ret = (m.return_type || '').trim();
            const params = (m.parameters || []).map(p => p.trim()).filter(Boolean).join(', ');
            let html = '<span class="ret">' + esc(ret) + '</span> <span class="nm">'
                + esc(m.name) + '</span><span class="params">(' + esc(params) + ')</span>';
            if (m['static']) html += '<span class="badge static">static</span>';
            return html;
        }

        function buildSidebar() {
            const sidebar = document.getElementById('sidebar');
            classes.forEach(c => {
                const card = document.createElement('div');
                card.className = 'classCard';
                card.dataset.name = c.name;
                const node = nodes.get(c.name);
                card.addEventListener('mouseenter', () => setHighlight(node, true));
                card.addEventListener('mouseleave', () => setHighlight(node, false));

                let html = '<h3>' + esc(c.name) + '</h3>';
                const bases = c.inheritance || [];
                if (bases.length) html += '<div class="bases">«extends» ' + bases.map(esc).join(', ') + '</div>';

                const methods = c.methods || [];
                const variables = c.variables || [];
                if (methods.length) {
                    html += '<h4>Methods (' + methods.length + ')</h4><ul>'
                        + methods.map(m => '<li>' + signature(m) + '</li>').join('')
                        + '</ul>';
                }
                if (variables.length) {
                    html += '<h4>Members (' + variables.length + ')</h4><ul>'
                        + variables.map(v =>
                            '<li><span class="ret">' + esc((v.type || '').trim())
                            + '</span> <span class="nm">' + esc(v.name) + '</span></li>'
                        ).join('')
                        + '</ul>';
                }
                if (!methods.length && !variables.length) {
                    html += '<div class="none">no members detected</div>';
                }
                card.innerHTML = html;
                sidebar.appendChild(card);
            });
            updateStats();
        }

        function updateStats() {
            const cards = [...document.querySelectorAll('.classCard')];
            const shown = cards.filter(c => c.style.display !== 'none').length;
            document.getElementById('stats').textContent
                = shown + ' of ' + classes.length + ' classes shown';
        }

        // --- Filtering (classes are hidden in both the 3D scene and the panel) ---
        function setFilter(re) {
            const realVisible = new Map();
            nodes.forEach(n => {
                if (!n.external) realVisible.set(n.name, !re || re.test(n.name));
            });
            nodes.forEach(n => {
                let visible;
                if (!n.external) {
                    visible = realVisible.get(n.name);
                } else {
                    // Unresolved base: keep it while any of its children is shown
                    visible = edgeObjs.some(e => e.to === n.name && realVisible.get(e.from));
                }
                n.mesh.visible = visible;
            });
            edgeObjs.forEach(e => {
                const a = nodes.get(e.from), b = nodes.get(e.to);
                e.line.visible = a.mesh.visible && b.mesh.visible;
            });
            document.querySelectorAll('.classCard').forEach(card => {
                card.style.display = (!re || re.test(card.dataset.name)) ? '' : 'none';
            });
            updateStats();
        }

        function applyFilter() {
            const value = document.getElementById('filterInput').value.trim();
            if (!value) { setFilter(null); return; }
            let re;
            try { re = new RegExp(value); }
            catch (err) { alert('Invalid regex: ' + err.message); return; }
            setFilter(re);
        }

        function resetFilter() {
            document.getElementById('filterInput').value = '';
            setFilter(null);
        }

        buildSidebar();
    </script>
</body>
</html>)HTMLDOC";

        // Splice the real class data into the template
        const std::string placeholder = "__CLASSES_JSON__";
        const std::string classes_json = getClassesJSON(parseInputFiles());
        const size_t pos = html.find(placeholder);
        if (pos != std::string::npos) {
            html.replace(pos, placeholder.size(), classes_json);
        }

        return html;
    }

    /**
     * @brief Parse input JSON files and extract class data
     * @return Vector of class data strings
     */
    std::vector<std::string> parseInputFiles() {
        std::vector<std::string> class_data;

        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";

        for (const auto& filename : input_files) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Warning: Could not open file " << filename << std::endl;
                continue;
            }

            Json::CharReaderBuilder reader_builder;
            Json::Value root;
            std::string errors;
            if (!Json::parseFromStream(reader_builder, file, &root, &errors)) {
                std::cerr << "Warning: failed to parse " << filename << ": " << errors << std::endl;
                continue;
            }

            const Json::Value& classes = root["classes"];
            if (!classes.isArray()) {
                std::cerr << "Warning: no 'classes' array in " << filename << std::endl;
                continue;
            }

            // Serialize each class object so the browser receives the real data
            for (const auto& cls : classes) {
                class_data.push_back(Json::writeString(writer, cls));
            }
        }

        return class_data;
    }

    /**
     * @brief Convert class data to JSON string for JavaScript
     * @param class_data Vector of class data strings
     * @return JSON string representation
     */
    std::string getClassesJSON(const std::vector<std::string>& class_data) {
        std::ostringstream json;
        json << "[";

        for (size_t i = 0; i < class_data.size(); ++i) {
            if (i > 0) json << ",";
            json << class_data[i];
        }

        json << "]";
        return json.str();
    }
};

/**
 * @brief Print usage information for the UML generator
 */
void print_usage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_json_file>...\n";
    std::cout << "Options:\n";
    std::cout << "  --hide <regex>             Hide classes matching regex pattern\n";
    std::cout << "  -h, --help                 Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " data1.json data2.json\n";
    std::cout << "  " << program_name << " --hide \"^std::|Test$\" data.json\n";
}

/**
 * @brief Main entry point for the UML generator
 * @param argc Number of command line arguments
 * @param argv Command line arguments
 * @return Exit status
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Error: No input specified\n";
        print_usage(argv[0]);
        return 1;
    }

    std::vector<std::string> input_files;
    std::vector<std::string> hide_patterns;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--hide" && i + 1 < argc) {
            hide_patterns.push_back(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            input_files.push_back(arg);
        }
    }

    // Create and run the UML generator
    UMLGenerator generator(input_files, hide_patterns);

    if (generator.generate()) {
        std::cout << "UML diagram generated successfully!\n";
        return 0;
    } else {
        std::cerr << "Failed to generate UML diagram.\n";
        return 1;
    }
}
