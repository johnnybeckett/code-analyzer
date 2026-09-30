#pragma once

#include <string_view>

namespace uml {

// Self-contained UML class-diagram page: a three.js 3D scene with class
// boxes (visibility glyphs, static/virtual/abstract styling),
// generalization arrows, filtering, a class-list side panel, and the
// two-file diff highlighting (added = green, removed = red). The three
// __*__ tokens are spliced in by TemplateRenderer::spliceAll — the single
// left-to-right pass never re-scans emitted values, so class data that
// literally contains a token cannot corrupt the output.
inline constexpr std::string_view kTemplate = R"HTMLDOC(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>UML Class Diagram</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
    <style>
        * { box-sizing: border-box; }
        /* Colour schemes: the default (dark) values live on :root and the
           other themes override them. The 3D scene follows suit via THEMES3D
           in the script below. */
        :root {
            --bg: #0f172a;
            --panel: #1e293b;
            --border: #334155;
            --border-strong: #475569;
            --text: #e2e8f0;
            --muted: #94a3b8;
            --faint: #64748b;
            --accent: #4285F4;
            --btn2: #475569;
            --card-name: #93c5fd;
            --ret: #7dd3fc;
            --nm: #fbbf24;
            --badge-vis-bg: #334155;
            --badge-vis-fg: #cbd5e1;
        }
        body[data-theme="light"] {
            --bg: #eef2f7;
            --panel: #ffffff;
            --border: #cbd5e1;
            --border-strong: #94a3b8;
            --text: #0f172a;
            --muted: #64748b;
            --faint: #94a3b8;
            --accent: #2563eb;
            --btn2: #94a3b8;
            --card-name: #1d4ed8;
            --ret: #0e7490;
            --nm: #b45309;
            --badge-vis-bg: #cbd5e1;
            --badge-vis-fg: #0f172a;
        }
        /* Vim "darkblue": Blue2 background, gray85 text, LightSkyBlue accents */
        body[data-theme="blue"] {
            --bg: #000094;
            --panel: #0000b8;
            --border: #3d3dd6;
            --border-strong: #6262ff;
            --text: #d9d9d9;
            --muted: #87cefa;
            --faint: #7489e8;
            --accent: #4a6cf7;
            --btn2: #3b3bd0;
            --card-name: #87cefa;
            --ret: #87cefa;
            --nm: #ffd97d;
            --badge-vis-bg: #2c2cc0;
            --badge-vis-fg: #c9d4ff;
        }
        body {
            margin: 0;
            background: var(--bg);
            color: var(--text);
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
            background: var(--panel);
            border-left: 1px solid var(--border);
            padding: 16px;
        }
        #sidebar h2 { margin: 0 0 12px; font-size: 16px; }
        #controls {
            position: absolute;
            top: 12px; left: 12px;
            background: var(--panel);
            border: 1px solid var(--border);
            padding: 12px;
            border-radius: 8px;
            z-index: 10;
            width: 268px;
        }
        #controls h3 { margin: 0 0 4px; font-size: 14px; }
        #controls input, #controls select {
            width: 100%;
            padding: 6px 8px;
            margin: 6px 0;
            border-radius: 6px;
            border: 1px solid var(--border-strong);
            background: var(--bg);
            color: var(--text);
        }
        #controls button {
            padding: 6px 12px;
            margin-right: 6px;
            border: 0;
            border-radius: 6px;
            background: var(--accent);
            color: #fff;
            cursor: pointer;
        }
        #controls button.secondary { background: var(--btn2); }
        .legend { font-size: 11px; color: var(--muted); margin: 6px 0 10px; line-height: 1.9; }
        .legend .sym {
            display: inline-block;
            border: 1px solid var(--border-strong);
            border-radius: 3px;
            padding: 0 5px;
            color: var(--text);
            font-size: 10px;
            line-height: 1.5;
        }
        .legend .tri {
            width: 0; height: 0;
            border: 5px solid transparent;
            border-bottom: 8px solid var(--muted);
            display: inline-block;
        }
        .legend .u { text-decoration: underline; }
        .legend .i { font-style: italic; }
        #hint { font-size: 11px; color: var(--faint); margin-top: 8px; }
        #stats { margin-top: 6px; font-size: 12px; color: var(--muted); }
        .classCard {
            background: var(--bg);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 12px;
            margin-bottom: 12px;
        }
        .classCard h3 { margin: 0 0 4px; font-size: 15px; color: var(--card-name); }
        .classCard .bases { font-size: 12px; color: var(--muted); margin-bottom: 6px; }
        .classCard h4 {
            margin: 10px 0 4px;
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: var(--faint);
        }
        .classCard ul {
            margin: 0;
            padding-left: 18px;
            font-family: "SF Mono", Consolas, monospace;
            font-size: 12px;
        }
        .classCard li { margin: 3px 0; }
        .classCard .ret { color: var(--ret); }
        .classCard .nm { color: var(--nm); }
        .classCard .params { color: var(--muted); }
        .badge {
            display: inline-block;
            font-size: 10px;
            padding: 1px 6px;
            border-radius: 8px;
            margin-left: 6px;
            vertical-align: middle;
        }
        .badge.static { background: #7c3aed; color: #fff; }
        .badge.vis { background: var(--badge-vis-bg); color: var(--badge-vis-fg); }
        .badge.diff { font-weight: 600; }
        .badge.diff.added   { background: #188038; color: #fff; }
        .badge.diff.removed { background: #c5221f; color: #fff; }
        .badge.diff.modified{ background: #b06000; color: #fff; }
        .classCard li.dm.added   { color: #188038; }
        .classCard li.dm.added .ret,
        .classCard li.dm.added .nm,
        .classCard li.dm.added .params { color: #188038; }
        .classCard li.dm.removed { color: #c5221f; text-decoration: line-through; }
        .classCard li.dm.removed .ret,
        .classCard li.dm.removed .nm,
        .classCard li.dm.removed .params { color: #c5221f; }
        .none { color: var(--faint); font-size: 12px; }
        /* Sidebar tree: one collapsible node per namespace */
        .nsHeader {
            display: flex;
            align-items: center;
            gap: 8px;
            background: var(--bg);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 8px 10px;
            margin-bottom: 8px;
            cursor: pointer;
            font-size: 13px;
            font-weight: bold;
            user-select: none;
        }
        .nsHeader .caret { color: var(--muted); font-size: 11px; width: 12px; }
        .nsHeader .count {
            margin-left: auto;
            color: var(--muted);
            font-weight: normal;
            font-size: 11px;
        }
        .nsChildren {
            margin-left: 9px;
            border-left: 1px solid var(--border);
            padding-left: 11px;
        }
        /* Controls panel: minimize to a compact header bar */
        #controlsHead {
            display: flex;
            align-items: center;
            justify-content: space-between;
        }
        #controlsHead h3 { margin: 0; }
        #controls #minimizeBtn { padding: 2px 9px; margin: 0 0 0 8px; font-size: 13px; }
        #controls.minimized { width: auto; padding: 0; border: 0; background: transparent; }
        #controls.minimized #controlsBody { display: none; }
        #controls.minimized #controlsHead {
            background: var(--panel);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 8px;
        }
    </style>
</head>
<body>
    <div id="scene"></div>
    <div id="controls">
        <div id="controlsHead">
            <h3>UML Class Diagram</h3>
            <button id="minimizeBtn" class="secondary" title="Minimize panel">&ndash;</button>
        </div>
        <div id="controlsBody">
        <div class="legend">
            <div><span class="tri"></span>&nbsp; generalization (extends)</div>
            <div><span class="sym">+</span> public &middot; <span class="sym">-</span> private &middot; <span class="sym">#</span> protected</div>
            <div><span class="sym u">member</span> static &middot; <span class="sym i">member</span> virtual</div>
            <div id="diffLegend" style="display:none">
                <span style="display:inline-block;width:10px;height:10px;background:#188038;border-radius:2px;margin-right:4px;vertical-align:baseline"></span> added
                &middot;
                <span style="display:inline-block;width:10px;height:10px;background:#c5221f;border-radius:2px;margin:0 4px 0 8px;vertical-align:baseline"></span> removed
                &middot;
                <span style="display:inline-block;width:10px;height:10px;background:#b06000;border-radius:2px;margin:0 4px 0 8px;vertical-align:baseline"></span> modified
            </div>
        </div>
        <h3>Filter classes</h3>
        <input type="text" id="filterInput" placeholder="regex, e.g. ^Parser|Generator$"
               onkeydown="if (event.key === 'Enter') applyFilter()">
        <button onclick="applyFilter()">Apply Filter</button>
        <button class="secondary" onclick="resetFilter()">Reset</button>
        <button class="secondary" onclick="resetView()">Reset View</button>
        <div id="diffWrap" style="display:none">
            <h3>Diff view</h3>
            <select id="diffSelect">
                <option value="changes" selected>Changes only</option>
                <option value="everything">Show everything</option>
            </select>
        </div>
        <h3>Layout</h3>
        <select id="layoutSelect">
            <option value="linear">Linear (hierarchy)</option>
            <option value="namespace">Grouped by namespace</option>
            <option value="circular">Circular</option>
        </select>
        <div id="nsLevelWrap" style="display:none">
            <h3>Namespace level</h3>
            <select id="nsLevelSelect">
                <option value="1" selected>Level 1 (top namespace)</option>
                <option value="2">Level 2</option>
                <option value="3">Level 3</option>
                <option value="4">Level 4</option>
                <option value="5">Level 5</option>
                <option value="6">Level 6</option>
                <option value="7">Level 7</option>
                <option value="8">Level 8</option>
                <option value="9">Level 9</option>
                <option value="10">Level 10</option>
                <option value="11">Level 11</option>
                <option value="12">Level 12 (full namespace)</option>
            </select>
        </div>
        <h3>Colour scheme</h3>
        <select id="themeSelect">
            <option value="dark">Dark</option>
            <option value="light">Light</option>
            <option value="blue">Vim darkblue</option>
        </select>
        <div id="hint">drag: orbit &middot; wheel / ctrl+drag: zoom &middot; shift+drag: pan &middot; alt: fine &middot; double-click a class to focus, or double-click a member&rsquo;s type to jump to that class &middot; click a class name in the sidebar to focus &middot; keys: n / &rarr; next class &middot; p / &larr; previous class &middot; c center view</div>
        <div id="stats"></div>
        <div id="diffStats" style="display:none"></div>
        </div>
    </div>
    <div id="sidebar"><h2>Classes</h2></div>

    <script>
        // Class data produced by the code analyzer.
        //   DIFF_MODE true  -> two files given (older newer): OLD_CLASSES is the
        //                      baseline, NEW_CLASSES the new state.
        //   DIFF_MODE false -> one file: NEW_CLASSES holds the combined set,
        //                      OLD_CLASSES is [].
        const DIFF_MODE = __DIFF_MODE__;
        const OLD_CLASSES = __OLD_CLASSES_JSON__;
        const NEW_CLASSES = __NEW_CLASSES_JSON__;

        const esc = s => String(s).replace(/[&<>"]/g,
            ch => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[ch]));

        // The analyzer may emit several records per class (e.g. one per source
        // file); merge same-named records into a single class. Key on
        // namespace + name so same-named classes in different namespaces
        // stay distinct.
        function mergeClasses(list) {
            const map = new Map();
            for (const c of list) {
                const key = (c.namespace || '') + '::' + c.name;
                if (!map.has(key)) {
                    map.set(key, {
                        name: c.name,
                        namespace: c.namespace || '',
                        kind: c.kind || 'class',
                        file: c.file || '',
                        bases: new Set(),
                        methods: new Map(),
                        vars: new Map(),
                    });
                }
                const m = map.get(key);
                if (c.file) m.file = c.file;
                (c.inheritance || []).forEach(b => m.bases.add(b));
                (c.methods || []).forEach(mt =>
                    m.methods.set(JSON.stringify([mt.name, mt.parameters || []]), mt));
                (c.variables || []).forEach(v => m.vars.set(v.name, v));
            }
            return [...map.values()].map(m => ({
                name: m.name,
                namespace: m.namespace,
                kind: m.kind || 'class',
                file: m.file,
                inheritance: [...m.bases],
                methods: [...m.methods.values()],
                variables: [...m.vars.values()],
            }));
        }
        const newMerged = mergeClasses(NEW_CLASSES);
        const oldMerged = DIFF_MODE ? mergeClasses(OLD_CLASSES) : [];

        // Diff two merged class sets (old baseline vs new state) and annotate
        // every class with a status and every method/variable with an __diff
        // marker:
        //   class   status: added | removed | modified | unchanged
        //   member  __diff: added | removed | unchanged
        // Identity keys match mergeClasses(): class = namespace::name,
        // method = name + parameters, variable = name.
        function computeDiff(oldList, newList) {
            const mkey = mt => JSON.stringify([mt.name, mt.parameters || []]);
            const vkey = v => v.name;

            function diffMembers(oldArr, newArr, keyFn) {
                const om = new Map((oldArr || []).map(x => [keyFn(x), x]));
                const nm = new Map((newArr || []).map(x => [keyFn(x), x]));
                const list = [];
                let changed = false;
                for (const [k, nv] of nm) {
                    const st = om.has(k) ? 'unchanged' : 'added';
                    if (st === 'added') changed = true;
                    const cp = Object.assign({}, nv);
                    cp.__diff = st;
                    list.push(cp);
                }
                for (const [k, ov] of om) {
                    if (nm.has(k)) continue;
                    changed = true;
                    const cp = Object.assign({}, ov);
                    cp.__diff = 'removed';
                    list.push(cp);
                }
                return { list, changed };
            }

            const omap = new Map(oldList.map(c => [c.namespace + '::' + c.name, c]));
            const nmap = new Map(newList.map(c => [c.namespace + '::' + c.name, c]));
            const result = [];

            for (const [key, n] of nmap) {
                const o = omap.get(key);
                const c = Object.assign({}, n);
                if (!o) {
                    c.status = 'added';
                    c.methods = n.methods.map(m => Object.assign({}, m, { __diff: 'added' }));
                    c.variables = n.variables.map(v => Object.assign({}, v, { __diff: 'added' }));
                } else {
                    const md = diffMembers(o.methods, n.methods, mkey);
                    const vd = diffMembers(o.variables, n.variables, vkey);
                    // A change to the base classes is also a structural edit,
                    // so the class must not read as "unchanged"
                    const inheritChanged =
                        JSON.stringify(o.inheritance || []) !==
                        JSON.stringify(n.inheritance || []);
                    c.status = (md.changed || vd.changed || inheritChanged)
                        ? 'modified' : 'unchanged';
                    c.methods = md.list;
                    c.variables = vd.list;
                }
                result.push(c);
            }

            for (const [key, o] of omap) {
                if (nmap.has(key)) continue;
                const c = Object.assign({}, o);
                c.status = 'removed';
                c.methods = o.methods.map(m => Object.assign({}, m, { __diff: 'removed' }));
                c.variables = o.variables.map(v => Object.assign({}, v, { __diff: 'removed' }));
                result.push(c);
            }

            return result;
        }

        const classes = DIFF_MODE ? computeDiff(oldMerged, newMerged) : newMerged;

        // --- Visibility state (diff filter + name filter, applied together) ---
        // Default: in diff mode hide unchanged classes so only the changes show.
        let showOnlyChanges = DIFF_MODE;
        let currentRegex = null;
        // name -> status (undefined in non-diff mode). Used to hide 'unchanged'
        // nodes when "Changes only" is active. (Name-keyed: same-named classes in
        // different namespaces are already merged into one node — pre-existing.)
        const statusByName = new Map();
        classes.forEach(c => { if (c.status) statusByName.set(c.name, c.status); });
        const diffVisible = status => !(DIFF_MODE && showOnlyChanges && status === 'unchanged');
        const regexVisible = name => !currentRegex || currentRegex.test(name);
        const classVisible = name =>
            regexVisible(name) && diffVisible(statusByName.get(name));

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
                // Raw type strings, resolved to a class on double-click
                types: [String(v.type || '').trim()].filter(Boolean),
                status: v.__diff || null,
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
                types: [ret, ...(m.parameters || [])]
                    .map(t => String(t || '').trim()).filter(Boolean),
                status: m.__diff || null,
            };
        }

        // --- Render one UML class box (name / attributes / methods) ---
        const TEXT = '15px Arial, Helvetica, sans-serif';
        const NAME = 'bold 17px Arial, Helvetica, sans-serif';
        const PAD = 14, NAME_H = 40, ROW_H = 24, COMP_PAD = 10;

        // The kind (class / struct / union) is shown as a prefix in the
        // name compartment; plain names for the default "class" kind
        const displayName = c => {
            const k = c && c.kind;
            return (k && k !== 'class' ? k + ' ' : '') + (c ? c.name : '');
        };

        // Diff coloring (the class box is a self-contained canvas, so these are
        // theme-independent). added = green, removed = red, modified = amber.
        const MEMBER_COLOR = { added: '#188038', removed: '#c5221f' };
        const DIFF_STYLE = {
            added:     { bg: '#e6f4ea', border: '#188038' },
            removed:   { bg: '#fce8e6', border: '#c5221f' },
            modified:  { bg: '#fef7e0', border: '#b06000' },
            unchanged: { bg: '#f8fafc', border: '#0f172a' },
        };
        const diffStyle = s => DIFF_STYLE[s] || DIFF_STYLE.unchanged;

        function drawMemberRow(ctx, row, y) {
            const base = y + ROW_H / 2;
            ctx.font = (row.italic ? 'italic ' : '') + TEXT;
            const color = MEMBER_COLOR[row.status] || '#0f172a';
            ctx.fillStyle = color;
            ctx.textAlign = 'left';
            ctx.textBaseline = 'middle';
            const pre = row.glyph + '  ';
            ctx.fillText(pre, PAD, base);
            const preW = ctx.measureText(pre).width;
            ctx.fillText(row.text, PAD + preW, base);
            if (row.underline) { // UML: static members are underlined
                const w = ctx.measureText(pre + row.text).width;
                ctx.strokeStyle = color;
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.moveTo(PAD, base + 9);
                ctx.lineTo(PAD + w, base + 9);
                ctx.stroke();
            }
            if (row.status === 'removed') { // UML-agnostic: strike through drops
                const w = ctx.measureText(pre + row.text).width;
                ctx.strokeStyle = color;
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.moveTo(PAD, base);
                ctx.lineTo(PAD + w, base);
                ctx.stroke();
            }
        }

        function buildClassCanvas(cls, abstract, external) {
            const attrs = cls.variables.map(attributeRow);
            const meths = cls.methods.map(methodRow);

            const probe = document.createElement('canvas').getContext('2d');
            probe.font = NAME;
            let width = Math.max(140, probe.measureText(displayName(cls)).width);
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

            const style = diffStyle(cls.status || 'unchanged');
            ctx.fillStyle = style.bg;
            ctx.fillRect(0, 0, width, height);
            if (external) ctx.setLineDash([6, 4]);
            ctx.strokeStyle = style.border;
            ctx.lineWidth = 2;
            ctx.strokeRect(1, 1, width - 2, height - 2);
            ctx.setLineDash([]);

            // Name compartment (italic when the class is abstract)
            ctx.fillStyle = '#0f172a';
            ctx.font = (abstract ? 'italic ' : '') + NAME;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(displayName(cls), width / 2, NAME_H / 2);

            const separator = y => {
                ctx.strokeStyle = '#94a3b8';
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(width, y);
                ctx.stroke();
            };

            // Record each member row's vertical band so a double-click can be
            // mapped back to "which member was hit" and its types resolved
            const rows = [];
            const band = (r, top) => {
                drawMemberRow(ctx, r, top);
                rows.push({ y0: top, y1: top + ROW_H, types: r.types || [] });
            };
            let y = NAME_H;
            if (attrs.length || meths.length) separator(y);
            if (attrs.length) {
                y += COMP_PAD;
                attrs.forEach(r => { band(r, y); y += ROW_H; });
                y += COMP_PAD;
            }
            if (meths.length) {
                separator(y);
                y += COMP_PAD;
                meths.forEach(r => { band(r, y); y += ROW_H; });
            }

            return { canvas, width, height, rows };
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
            const { canvas, width, height, rows } = buildClassCanvas(cls, abstract, external);
            const tex = new THREE.CanvasTexture(canvas);
            tex.anisotropy = renderer.capabilities.getMaxAnisotropy();
            // A BoxGeometry's front and back faces both present a texture
            // upright and unmirrored when viewed from the outside, so the
            // very same UML texture reads correctly on both sides
            const face = new THREE.MeshBasicMaterial({ map: tex });
            const side = new THREE.MeshBasicMaterial({ color: 0x24344d });
            const mesh = new THREE.Mesh(
                new THREE.BoxGeometry(width * WORLD, height * WORLD, 0.3),
                [side, side, side, side, face, face]);
            // ch = canvas height in logical px (pre-supersampling); a hit's
            // UV y maps into the recorded member-row bands with it
            return { mesh, side, w: width * WORLD, h: height * WORLD, rows, ch: height };
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

        // Resolve a base-class name to the analyzed class it refers to, so
        // inheritance edges link real class boxes instead of dashed stubs.
        // Handles bare names (`Test`) and qualified names (`testing::Test`) by
        // matching the fully-qualified `namespace::name` form. Returns the
        // class, or null when the base is external (e.g. `std::enable_...`).
        function resolveBase(b) {
            if (byName.has(b)) return byName.get(b);
            if (b.includes('::')) {
                for (const c of classes) {
                    if ((c.namespace || '') + '::' + c.name === b) return c;
                }
            }
            return null;
        }

        const edges = [];
        for (const c of classes) {
            const n = getNode(c.name);
            const bases = [...new Set(c.inheritance || [])];
            // Map each base to the node it should link to: the resolved class
            // (a real box) when it's in the analysis, otherwise an external
            // stub named after the base.
            const targets = bases.map(b => {
                const resolved = resolveBase(b);
                return resolved ? resolved.name : b;
            });
            if (targets.length) n.parent = targets[0]; // primary base drives layout
            targets.forEach(t => { getNode(t); edges.push({ from: n.name, to: t }); });
        }

        const children = new Map();
        for (const n of nodes.values()) {
            if (!n.parent) continue;
            if (!children.has(n.parent)) children.set(n.parent, []);
            children.get(n.parent).push(n);
        }

        // Build every box (unresolved bases become small dashed stubs)
        for (const n of nodes.values()) {
            const cls = n.cls || { name: n.name, methods: [], variables: [] };
            const box = makeClassBox(cls, !n.external && isAbstract(n.cls), n.external);
            n.w = box.w; n.h = box.h;
            n.rows = box.rows; n.ch = box.ch;
            n.mesh = box.mesh;
            n.sideMat = box.side;
        }

        // --- Layouts ---
        // Each layout assigns n.x/n.y/z to every node; applyLayout() then
        // moves the meshes and rebuilds the arrows, namespace frames and grid.
        let currentGroups = [];   // namespace frames (namespace layout only)

        // Tidy placement of one set of classes: leaves take consecutive x
        // slots, parents center over their children, levels stack vertically.
        function tidyLayout(list) {
            const inner = new Set(list.map(n => n.name));
            const kids = new Map();
            for (const n of list) {
                if (!n.parent || !inner.has(n.parent)) continue;
                if (!kids.has(n.parent)) kids.set(n.parent, []);
                kids.get(n.parent).push(n);
            }
            const placed = new Set();
            let cursor = 0;
            const place = n => {
                if (placed.has(n.name)) return;
                placed.add(n.name);
                const ch = kids.get(n.name) || [];
                if (!ch.length) { n.x = cursor++; }
                else {
                    ch.forEach(k => place(k));
                    n.x = (ch[0].x + ch[ch.length - 1].x) / 2;
                }
            };
            const roots = list.filter(n => !n.parent || !inner.has(n.parent));
            roots.forEach(r => place(r));
            const setDepth = (n, d) => {
                n.depth = d;
                (kids.get(n.name) || []).forEach(k => setDepth(k, d + 1));
            };
            roots.forEach(r => setDepth(r, 0));

            const X_GAP = Math.max(...list.map(n => n.w)) + 3;  // pitch keeps boxes apart
            const maxDepth = Math.max(...list.map(n => n.depth));
            const levelH = Array.from({ length: maxDepth + 1 }, () => 0);
            for (const n of list) levelH[n.depth] = Math.max(levelH[n.depth], n.h);
            let totalH = 0;
            for (let d = 0; d <= maxDepth; d++) totalH += levelH[d] + Y_GAP;
            let acc = 0;
            const levelY = levelH.map(h => {
                const y = totalH / 2 - acc - h / 2; acc += h + Y_GAP; return y;
            });

            let cx = 0;
            for (const n of list) { n.x *= X_GAP; n.y = levelY[n.depth]; cx += n.x; }
            cx /= list.length;
            for (const n of list) { n.x -= cx; n.z = 0; }
        }

        // Linear: one tidy layout over all classes; each independent
        // hierarchy keeps its own depth slice (the original arrangement).
        function layoutLinear() {
            const roots = [...nodes.values()].filter(n => !n.parent);
            const hierarchies = [];
            for (const r of roots) {
                const members = [r];
                const seen = new Set([r.name]);
                const stack = [...(children.get(r.name) || [])];
                while (stack.length) {
                    const k = stack.pop();
                    if (seen.has(k.name)) continue;
                    seen.add(k.name);
                    members.push(k);
                    (children.get(k.name) || [])
                        .forEach(c => { if (!seen.has(c.name)) stack.push(c); });
                }
                hierarchies.push(members);
            }
            hierarchies.forEach((members, i) => {
                tidyLayout(members);
                const z = (i - (hierarchies.length - 1) / 2) * Z_GAP;
                members.forEach(n => { n.z += z; });
            });
            let cz = 0;
            for (const n of nodes.values()) cz += n.z;
            cz /= nodes.size;
            nodes.forEach(n => { n.z -= cz; });
        }

        // Namespace: classes are grouped by namespace, falling back to the
        // source-file directory; each group sits in its own labelled frame.
        // Group key: the first `level` "::"-separated segments of the
        // namespace (levels 1..12 from the selector; Infinity keeps the
        // full namespace, as the sidebar tree does). Namespaces with fewer
        // segments keep all of them. Empty namespace falls back to the
        // source-file directory (or '' => "(global)").
        function classGroupKey(c, level = Infinity) {
            const ns = String(c.namespace || '').trim();
            if (ns) {
                const segs = ns.split('::').filter(Boolean);
                if (segs.length) return segs.slice(0, level).join('::');
                return ns;
            }
            const parts = String(c.file || '').trim().split('/').filter(Boolean);
            return parts.length > 1 ? parts.slice(0, -1).join('/') : '';
        }
        let nsLevel = 1;
        const groupKey = n => (n.cls ? classGroupKey(n.cls, nsLevel) : '');
        const groupLabel = key => key || '(global)';

        const NS_PAD = 4;        // clearance between a group's boxes and its frame
        const NS_CELL_X = 22;   // clearance between adjacent frames
        const NS_CELL_Y = 26;

        function layoutNamespaces() {
            const groups = new Map();
            for (const n of nodes.values()) {
                const key = groupKey(n);
                if (!groups.has(key)) groups.set(key, []);
                groups.get(key).push(n);
            }
            const keys = [...groups.keys()].sort((a, b) =>
                (a === '') - (b === '') || a.localeCompare(b));

            const laid = [];
            for (const key of keys) {
                const members = groups.get(key);
                tidyLayout(members);
                let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
                for (const n of members) {
                    minX = Math.min(minX, n.x - n.w / 2);
                    maxX = Math.max(maxX, n.x + n.w / 2);
                    minY = Math.min(minY, n.y - n.h / 2);
                    maxY = Math.max(maxY, n.y + n.h / 2);
                }
                laid.push({ key, members, minX, minY, maxX, maxY });
            }

            // Place the groups in a grid, top to bottom, left to right
            const cols = Math.max(1, Math.round(Math.sqrt(laid.length)));
            const rows = Math.ceil(laid.length / cols);
            const cellW = Math.max(...laid.map(g => g.maxX - g.minX)) + NS_PAD * 2 + NS_CELL_X;
            const cellH = Math.max(...laid.map(g => g.maxY - g.minY)) + NS_PAD * 2 + NS_CELL_Y;
            laid.forEach((g, i) => {
                const col = i % cols, row = Math.floor(i / cols);
                const ox = (col - (cols - 1) / 2) * cellW;
                const oy = (rows - 1 - row) * cellH;
                const gx = (g.minX + g.maxX) / 2, gy = (g.minY + g.maxY) / 2;
                g.members.forEach(n => { n.x += ox - gx; n.y += oy - gy; });
                g.minX += ox - gx; g.maxX += ox - gx;
                g.minY += oy - gy; g.maxY += oy - gy;
                g.x = (g.minX + g.maxX) / 2;
                g.y = (g.minY + g.maxY) / 2;
                g.w = g.maxX - g.minX + NS_PAD * 2;
                g.h = g.maxY - g.minY + NS_PAD * 2;
            });

            currentGroups = laid.map(g => ({
                label: groupLabel(g.key),
                x: g.x, y: g.y, w: g.w, h: g.h,
                names: g.members.map(n => n.name),
            }));
        }

        // Circular: namespace groups occupy angular sectors around the
        // origin in the x-y plane (z = 0 for every node); within a sector,
        // members sit on concentric rings by inheritance depth (roots
        // innermost). One group or all-global data degrade to a single full
        // 2*PI sector, so the same code path covers every case.
        function layoutCircular() {
            const groups = new Map();
            for (const n of nodes.values()) {
                const key = groupKey(n);
                if (!groups.has(key)) groups.set(key, []);
                groups.get(key).push(n);
            }
            const keys = [...groups.keys()].sort((a, b) =>
                (a === '') - (b === '') || a.localeCompare(b));
            const maxW = Math.max(10, ...nodes.values().map(n => n.w));
            const GAP = 3;
            const RING_GAP = maxW + GAP * 2;
            const R0 = maxW + GAP * 2;
            const TWO_PI = Math.PI * 2;
            const sector = TWO_PI / Math.max(1, keys.length);
            keys.forEach((key, gi) => {
                const members = groups.get(key);
                const inner = new Set(members.map(n => n.name));
                // Inheritance depth within this sector (roots = 0)
                const depthOf = n => {
                    let d = 0, cur = n, guard = 0;
                    while (cur.parent && inner.has(cur.parent) && guard++ < 1000) {
                        d++; cur = nodes.get(cur.parent);
                    }
                    return d;
                };
                const byDepth = new Map();
                for (const n of members) {
                    const d = depthOf(n);
                    if (!byDepth.has(d)) byDepth.set(d, []);
                    byDepth.get(d).push(n);
                }
                const start = -Math.PI / 2 + gi * sector;
                // Innermost first, so rings sharing angles (same member
                // count place their boxes at identical odd-multiple slots)
                // can be pushed strictly apart ring by ring.
                const depths = [...byDepth.keys()].sort((a, b) => a - b);
                const ringRadius = new Map();   // ring length -> largest radius used
                for (const d of depths) {
                    const ring = byDepth.get(d);
                    const slot = sector / ring.length;
                    // Radius large enough that the chord between adjacent
                    // boxes is >= maxW + 2*GAP; clamp sin so a tiny slot
                    // can't blow up the radius. A singleton ring has no
                    // in-ring neighbours, so it only needs clearance from
                    // the adjacent sector — and a lone group (full 2*PI
                    // sector) has no neighbours at all, so no lower bound.
                    let need = 0;
                    if (ring.length > 1) {
                        need = (maxW / 2 + GAP) / Math.max(0.2, Math.sin(slot / 2));
                    } else if (keys.length > 1) {
                        need = (maxW / 2 + GAP) / Math.max(0.2, Math.sin(sector / 2));
                    }
                    let r = Math.max(R0 + d * RING_GAP, need);
                    const prev = ringRadius.get(ring.length);
                    if (prev !== undefined) r = Math.max(r, prev + RING_GAP);
                    ringRadius.set(ring.length, r);
                    ring.forEach((n, i) => {
                        const a = start + (i + 0.5) * slot;
                        n.x = r * Math.cos(a);
                        n.y = r * Math.sin(a);
                        n.z = 0;
                    });
                }
            });
        }

        // --- Generalization arrows: stem + hollow triangle at the superclass ---
        const TRI = 0.9, TRIW = 0.4;
        // One shared material so the theme switch can re-tint all arrows
        const edgeMat = new THREE.LineBasicMaterial({ color: 0xcbd5e1 });
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
            return new THREE.Line(geo, edgeMat);
        }

        let edgeObjs = [];
        function rebuildEdges() {
            for (const e of edgeObjs) {
                diagram.remove(e.line);
                e.line.geometry.dispose();   // edgeMat is shared — keep it
            }
            edgeObjs = edges
                .map(e => ({ from: e.from, to: e.to, line: makeEdge(nodes.get(e.from), nodes.get(e.to)) }))
                .filter(e => e.line);
            edgeObjs.forEach(e => diagram.add(e.line));
        }

        for (const n of nodes.values()) diagram.add(n.mesh);

        // Faint floor grid as a depth cue (rebuilt when the layout or theme changes)
        let span = 100, gridY = 0;
        function computeSpan() {
            span = Math.max(...nodes.values().map(n => Math.hypot(n.x, n.y, n.z) + Math.max(n.w, n.h)));
            gridY = Math.min(...nodes.values().map(n => n.y - n.h / 2)) - 3;
        }
        let grid = null;
        function syncGrid() {
            const c1 = cur3d ? cur3d.grid[0] : 0x273449;
            const c2 = cur3d ? cur3d.grid[1] : 0x1b2536;
            if (grid) {
                diagram.remove(grid);
                grid.geometry.dispose();
                grid.material.dispose();
            }
            grid = new THREE.GridHelper(span * 2.5, 40, c1, c2);
            grid.position.y = gridY;
            diagram.add(grid);
        }

        // --- Namespace frames: a wide slab behind each group. The group
        //     name is printed large on the slab's top and bottom faces so it
        //     is legible when the diagram is zoomed out and viewed from
        //     above or from below; the side faces keep a normal-width edge ---
        const PANEL_DEPTH = 4;    // slab thickness (world units)
        const PANEL_Z = -0.3;     // front face, just behind the class boxes
        const PANEL_PX = 16;      // canvas pixels per world unit

        // Fill + a normal-width border for the slab's four side faces
        function makeSideCanvas() {
            const c = document.createElement('canvas');
            c.width = 64; c.height = 64;
            const ctx = c.getContext('2d');
            const t = cur3d.panel;
            ctx.fillStyle = t.fill;
            ctx.fillRect(0, 0, 64, 64);
            ctx.strokeStyle = t.border;
            ctx.lineWidth = 2;
            ctx.strokeRect(1, 1, 62, 62);
            return c;
        }

        // The group name printed across the slab's top/bottom faces in a
        // large font (shrinking to fit narrow frames). A BoxGeometry's +Y
        // and -Y faces both present the canvas left edge to world -X and,
        // from the natural above/below view, present the canvas top toward
        // the top of the screen, so the name reads correctly from either
        // side with no mirroring
        function makeNameCanvas(label, widthWorld) {
            const w = Math.max(96, Math.ceil(widthWorld * PANEL_PX));
            const h = Math.max(64, Math.ceil(PANEL_DEPTH * PANEL_PX));
            const scale = 2;   // supersample for crispness
            const canvas = document.createElement('canvas');
            canvas.width = w * scale; canvas.height = h * scale;
            const ctx = canvas.getContext('2d');
            ctx.scale(scale, scale);
            const t = cur3d.panel;
            ctx.fillStyle = t.fill;
            ctx.fillRect(0, 0, w, h);
            ctx.strokeStyle = t.border;
            ctx.lineWidth = 2;
            ctx.strokeRect(1, 1, w - 2, h - 2);
            let fs = Math.floor(h * 0.62);
            ctx.font = 'bold ' + fs + 'px Arial, Helvetica, sans-serif';
            const maxW = w - 24;
            while (fs > 12 && ctx.measureText(label).width > maxW) {
                fs -= 2;
                ctx.font = 'bold ' + fs + 'px Arial, Helvetica, sans-serif';
            }
            ctx.fillStyle = t.text;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(label, w / 2, h / 2 + 1);
            return canvas;
        }

        let panels = [];
        function refreshPanels() {
            for (const p of panels) {
                diagram.remove(p.mesh);
                p.mesh.geometry.dispose();
                for (const m of p.mesh.material) {
                    if (m.map) m.map.dispose();
                    m.dispose();
                }
            }
            panels = [];
            if (!currentGroups.length || !cur3d) return;
            for (const g of currentGroups) {
                const nameTex = new THREE.CanvasTexture(makeNameCanvas(g.label, g.w));
                nameTex.anisotropy = renderer.capabilities.getMaxAnisotropy();
                const sideTex = new THREE.CanvasTexture(makeSideCanvas());
                sideTex.anisotropy = renderer.capabilities.getMaxAnisotropy();
                const nameMat = new THREE.MeshBasicMaterial({ map: nameTex, transparent: true });
                const sideMat = new THREE.MeshBasicMaterial({ map: sideTex, transparent: true });
                const mesh = new THREE.Mesh(
                    new THREE.BoxGeometry(g.w, g.h, PANEL_DEPTH),
                    [sideMat, sideMat, nameMat, nameMat, sideMat, sideMat]);
                mesh.position.set(g.x, g.y, PANEL_Z - PANEL_DEPTH / 2);
                diagram.add(mesh);
                panels.push({ mesh, names: g.names });
            }
        }

        // Switch layout: re-place every node, then rebuild the arrows, the
        // namespace frames and the grid to match
        function applyLayout(mode) {
            if (mode === 'namespace') layoutNamespaces();
            else if (mode === 'circular') { currentGroups = []; layoutCircular(); }
            else { currentGroups = []; layoutLinear(); }
            for (const n of nodes.values()) n.mesh.position.set(n.x, n.y, n.z);
            rebuildEdges();
            refreshPanels();
            computeSpan();
            syncGrid();
            resetView();
            applyFilter();   // re-apply any active filter to the rebuilt arrows
        }

        // Initial arrangement; edges, frames and the grid are created once
        // the theme state exists, at the end of the script
        layoutLinear();
        computeSpan();

        // --- Camera and interaction ---
        // Orbit camera: the diagram stays put and the camera orbits a target
        // point. yaw/pitch rotate around it, dist zooms along the view axis,
        // and panning slides the target in the camera plane.
        const FOV = 50 * Math.PI / 180;
        const initDist = span * 1.15 + 10;
        const MIN_DIST = 4, MAX_DIST = initDist * 3;
        const camera = new THREE.PerspectiveCamera(
            50, container.clientWidth / container.clientHeight, 0.1, 5000);

        const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
        const view = { yaw: 0, pitch: 0, dist: initDist,
                       target: { x: 0, y: 0, z: 0 } };

        function worldPerPixel() {
            return (2 * view.dist * Math.tan(FOV / 2)) / container.clientHeight;
        }

        function updateCamera() {
            const cp = Math.cos(view.pitch), sp = Math.sin(view.pitch);
            const cy = Math.cos(view.yaw), sy = Math.sin(view.yaw);
            camera.position.set(
                view.target.x + view.dist * cp * sy,
                view.target.y + view.dist * sp,
                view.target.z + view.dist * cp * cy);
            camera.lookAt(view.target.x, view.target.y, view.target.z);
        }
        updateCamera();

        let autoRotate = true;
        let dragging = false, lastX = 0, lastY = 0;
        // The "home" view: centred, head-on, initial zoom — the destination
        // of both the initial setup and "Reset View"
        function homeView() {
            return { yaw: 0, pitch: 0, dist: initDist, tx: 0, ty: 0, tz: 0 };
        }
        const focus = { active: false, ...homeView() };

        container.addEventListener('mousedown', e => {
            if (e.button !== 0) return;  // left button only (others may context-menu)
            autoRotate = false;
            focus.active = false;
            dragging = true;
            lastX = e.clientX; lastY = e.clientY;
        });
        window.addEventListener('mouseup', () => { dragging = false; });
        window.addEventListener('mousemove', e => {
            if (!dragging) return;
            const dx = e.clientX - lastX, dy = e.clientY - lastY;
            lastX = e.clientX; lastY = e.clientY;
            const fine = e.altKey ? 0.25 : 1;   // Alt = fine adjustments

            if (e.shiftKey) {
                // Pan the look-at target in the camera plane (x / y)
                const s = worldPerPixel() * fine;
                const cp = Math.cos(view.pitch), sp = Math.sin(view.pitch);
                const cy = Math.cos(view.yaw), sy = Math.sin(view.yaw);
                const rx = cy,      ry = 0,    rz = -sy;         // camera right
                const ux = -sp * sy, uy = cp, uz = -sp * cy;     // camera up
                view.target.x += (-dx * rx + dy * ux) * s;
                view.target.y += (-dx * ry + dy * uy) * s;
                view.target.z += (-dx * rz + dy * uz) * s;
            } else if (e.ctrlKey) {
                // Zoom in / out along the view axis (z)
                view.dist = clamp(view.dist * (1 + dy * 0.002 * fine),
                                  MIN_DIST, MAX_DIST);
            } else {
                view.yaw += dx * 0.005 * fine;
                view.pitch = clamp(view.pitch + dy * 0.005 * fine, -1.4, 1.4);
            }
        });
        container.addEventListener('wheel', e => {
            e.preventDefault();
            focus.active = false;
            view.dist = clamp(view.dist * (1 + e.deltaY * 0.001), MIN_DIST, MAX_DIST);
        }, { passive: false });

        // Double-click a class to point the camera at it; a double-click on
        // a member row jumps to the class named by that member's type
        // (e.g. a "Mutability" field flies to the Mutability class)
        const ray = new THREE.Raycaster();
        const TYPE_JUNK = /^(void|bool|char|short|int|long|float|double|auto|unsigned|signed|const|static|virtual|mutable|string|size_t|wchar_t|std)$/;
        function classInType(t) {
            const ids = String(t || '').match(/[A-Za-z_][A-Za-z0-9_]*/g) || [];
            for (const id of ids) {
                if (!TYPE_JUNK.test(id) && nodes.has(id)) return id;
            }
            return null;
        }
        container.addEventListener('dblclick', e => {
            const rect = container.getBoundingClientRect();
            const mouse = new THREE.Vector2(
                ((e.clientX - rect.left) / rect.width) * 2 - 1,
                -((e.clientY - rect.top) / rect.height) * 2 + 1);
            ray.setFromCamera(mouse, camera);
            const hits = ray.intersectObjects([...nodes.values()].map(n => n.mesh));
            if (!hits.length) return;
            const n = [...nodes.values()].find(nd => nd.mesh === hits[0].object);
            const hit = hits[0];
            if (hit.uv && n.rows && n.ch) {
                const py = (1 - hit.uv.y) * n.ch;
                const row = n.rows.find(r => py >= r.y0 && py < r.y1);
                if (row) {
                    for (const t of row.types) {
                        const target = classInType(t);
                        if (target) { focusOn(nodes.get(target)); return; }
                    }
                }
            }
            focusOn(n);
        });

        // Bring yaw onto [-pi, pi] so the focus animation takes the short way
        function wrapYaw() {
            view.yaw = ((view.yaw + Math.PI) % (2 * Math.PI) + 2 * Math.PI)
                       % (2 * Math.PI) - Math.PI;
        }

        function focusOn(n) {
            autoRotate = false;
            wrapYaw();
            // Head-on view: the class front faces the camera, centered
            focus.yaw = 0;
            focus.pitch = 0;
            focus.dist = Math.max(8, Math.max(n.w, n.h) * 3);
            focus.tx = n.x; focus.ty = n.y; focus.tz = n.z;
            focus.active = true;
        }

        function resetView() {
            wrapYaw();
            Object.assign(focus, homeView());
            focus.active = true;
        }

        // --- Colour themes ---
        // The HTML chrome is themed through the CSS variables set on
        // body[data-theme]; the 3D scene needs its palette applied here.
        // The class boxes stay white in every theme — their dark outline
        // keeps them legible against any background.
        const THEMES3D = {
            dark:  { bg: 0x0f172a, side: 0x24344d, highlight: 0x4285F4,
                     grid: [0x273449, 0x1b2536], edge: 0xcbd5e1,
                     panel: { fill: 'rgba(148, 163, 184, 0.10)',
                              strip: 'rgba(71, 85, 105, 0.95)',
                              border: '#475569', text: '#e2e8f0' } },
            light: { bg: 0xe2e8f0, side: 0xc7d2e0, highlight: 0x2563eb,
                     grid: [0xbcc7d6, 0xd8e0ea], edge: 0x475569,
                     panel: { fill: 'rgba(51, 65, 85, 0.08)',
                              strip: 'rgba(148, 163, 184, 0.95)',
                              border: '#94a3b8', text: '#0f172a' } },
            blue:  { bg: 0x0000a8, side: 0x2b2b9e, highlight: 0x6d8cff,
                     grid: [0x2f2fae, 0x1a1a70], edge: 0x9db8ff,
                     panel: { fill: 'rgba(135, 206, 250, 0.10)',
                              strip: 'rgba(91, 91, 224, 0.95)',
                              border: '#5b5be0', text: '#c9d4ff' } },
        };
        let cur3d = THEMES3D.dark;

        function applyTheme(name) {
            if (!THEMES3D[name]) name = 'dark';
            cur3d = THEMES3D[name];
            document.body.dataset.theme = name;
            scene.background.set(cur3d.bg);
            edgeMat.color.set(cur3d.edge);
            syncGrid();
            refreshPanels();
            // Re-tint the box sides, keeping whichever are highlighted now
            for (const n of nodes.values()) {
                n.sideMat.color.set(n.mesh.scale.x !== 1 ? cur3d.highlight
                                                         : cur3d.side);
            }
            try { localStorage.setItem('umlTheme', name); } catch (err) {}
        }

        // Hover a sidebar card to highlight the box
        function setHighlight(n, on) {
            if (!n) return;
            n.sideMat.color.set(on ? cur3d.highlight : cur3d.side);
            const s = on ? 1.06 : 1.0;
            n.mesh.scale.set(s, s, s);
        }

        (function animate() {
            requestAnimationFrame(animate);
            if (autoRotate) view.yaw += 0.002;
            if (focus.active) {
                const k = 0.08;
                view.yaw += (focus.yaw - view.yaw) * k;
                view.pitch += (focus.pitch - view.pitch) * k;
                view.dist += (focus.dist - view.dist) * k;
                view.target.x += (focus.tx - view.target.x) * k;
                view.target.y += (focus.ty - view.target.y) * k;
                view.target.z += (focus.tz - view.target.z) * k;
                if (Math.abs(focus.yaw - view.yaw) < 0.002
                    && Math.abs(focus.pitch - view.pitch) < 0.002
                    && Math.abs(focus.dist - view.dist) < 0.05
                    && Math.hypot(focus.tx - view.target.x,
                                focus.ty - view.target.y,
                                focus.tz - view.target.z) < 0.05) {
                    focus.active = false;
                }
            }
            updateCamera();
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

        function makeClassCard(c) {
            const card = document.createElement('div');
            card.className = 'classCard';
            card.dataset.name = c.name;
            card.dataset.status = c.status || '';
            const node = nodes.get(c.name);
            card.addEventListener('mouseenter', () => setHighlight(node, true));
            card.addEventListener('mouseleave', () => setHighlight(node, false));

            let html = '<h3>' + esc(displayName(c));
            if (c.status) {
                html += ' <span class="badge diff ' + c.status + '">' + c.status + '</span>';
            }
            html += '</h3>';
            const bases = c.inheritance || [];
            if (bases.length) html += '<div class="bases">«extends» ' + bases.map(esc).join(', ') + '</div>';

            const methods = c.methods || [];
            const variables = c.variables || [];
            if (methods.length) {
                html += '<h4>Methods (' + methods.length + ')</h4><ul>'
                    + methods.map(m =>
                        '<li class="dm ' + (m.__diff || 'unchanged') + '">'
                        + signature(m) + '</li>'
                    ).join('')
                    + '</ul>';
            }
            if (variables.length) {
                html += '<h4>Members (' + variables.length + ')</h4><ul>'
                    + variables.map(v =>
                        '<li class="dm ' + (v.__diff || 'unchanged') + '">'
                        + '<span class="ret">' + esc((v.type || '').trim())
                        + '</span> <span class="nm">' + esc(v.name) + '</span>'
                        + '</li>'
                    ).join('')
                    + '</ul>';
            }
            if (!methods.length && !variables.length) {
                html += '<div class="none">no members detected</div>';
            }
            card.innerHTML = html;
            // Clicking the class name flies the camera to that box
            const title = card.querySelector('h3');
            title.style.cursor = 'pointer';
            title.title = 'Click to focus the camera on this class';
            title.addEventListener('click', () => focusOn(node));
            return card;
        }

        // The classes list is a tree: one collapsible node per namespace
        // (falling back to the source directory, as in the 3D frames) with
        // its class cards nested underneath
        function buildSidebar() {
            const sidebar = document.getElementById('sidebar');
            const groups = new Map();
            for (const c of classes) {
                const key = classGroupKey(c);
                if (!groups.has(key)) groups.set(key, []);
                groups.get(key).push(c);
            }
            const keys = [...groups.keys()].sort((a, b) =>
                (a === '' ? -1 : 0) - (b === '' ? -1 : 0) || a.localeCompare(b));
            for (const key of keys) {
                const list = groups.get(key);
                const group = document.createElement('div');
                group.className = 'nsGroup';
                const header = document.createElement('div');
                header.className = 'nsHeader';
                header.innerHTML = '<span class="caret">&#9662;</span>'
                    + '<span class="nsName">' + esc(groupLabel(key)) + '</span>'
                    + '<span class="count">' + list.length + '</span>';
                const children = document.createElement('div');
                children.className = 'nsChildren';
                for (const c of list) children.appendChild(makeClassCard(c));
                // Clicking the namespace header collapses / expands its classes
                header.addEventListener('click', () => {
                    const collapsed = children.style.display === 'none';
                    children.style.display = collapsed ? '' : 'none';
                    header.querySelector('.caret').innerHTML
                        = collapsed ? '&#9662;' : '&#9656;';
                });
                group.appendChild(header);
                group.appendChild(children);
                sidebar.appendChild(group);
            }
            updateStats();
        }

        function updateStats() {
            const cards = [...document.querySelectorAll('.classCard')];
            const shown = cards.filter(c => {
                if (c.style.display === 'none') return false;
                const group = c.closest('.nsChildren');
                return !(group && group.style.display === 'none');
            }).length;
            document.getElementById('stats').textContent
                = shown + ' of ' + classes.length + ' classes shown';
        }

        // --- Visibility: apply the diff filter and the name filter together
        // --- (classes are hidden in both the 3D scene and the panel)
        function applyVisibility() {
            const realVisible = new Map();
            nodes.forEach(n => {
                if (!n.external) realVisible.set(n.name, classVisible(n.name));
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
            // A namespace frame stays while any class inside it is shown
            panels.forEach(p => {
                const show = p.names.some(name => {
                    const n = nodes.get(name);
                    return n && !n.external && n.mesh.visible;
                });
                p.mesh.visible = show;
            });
            document.querySelectorAll('.classCard').forEach(card => {
                const vis = regexVisible(card.dataset.name)
                    && diffVisible(card.dataset.status);
                card.style.display = vis ? '' : 'none';
            });
            // Hide a namespace group whose classes are all hidden
            document.querySelectorAll('.nsGroup').forEach(g => {
                const anyShown = [...g.querySelectorAll('.classCard')]
                    .some(card => card.style.display !== 'none');
                g.style.display = anyShown ? '' : 'none';
            });
            updateStats();
        }

        function applyFilter() {
            const value = document.getElementById('filterInput').value.trim();
            if (!value) { currentRegex = null; applyVisibility(); return; }
            let re;
            try { re = new RegExp(value); }
            catch (err) { alert('Invalid regex: ' + err.message); return; }
            currentRegex = re;
            applyVisibility();
        }

        function resetFilter() {
            document.getElementById('filterInput').value = '';
            currentRegex = null;
            applyVisibility();
        }

        buildSidebar();

        // --- Keyboard navigation: next / previous / center ---
        // n or → steps to the next class, p or ← to the previous one — both
        // wrap around the classes currently shown in the sidebar — and c (or
        // Home) returns to the home view, the same destination as the
        // "Reset View" button. The current class stays highlighted until the
        // camera moves on. Keys are ignored while typing in a field (e.g.
        // the filter box) or while a modifier is held, so browser shortcuts
        // and normal typing keep working.
        let navIndex = -1;
        let navHighlight = null;

        // A card counts as shown the same way updateStats does: not hidden
        // by the filters and not inside a collapsed namespace group
        function cardShown(card) {
            const group = card.closest('.nsChildren');
            return card.style.display !== 'none'
                && !(group && group.style.display === 'none');
        }

        // Highlight the current class, clearing whichever one was current
        function setNavHighlight(node) {
            if (navHighlight && navHighlight !== node) setHighlight(navHighlight, false);
            if (node) setHighlight(node, true);
            navHighlight = node;
        }

        function navTo(dir) {
            const cards = [...document.querySelectorAll('.classCard')].filter(cardShown);
            if (!cards.length) return;
            // A stale index (filters changed since the last step) re-anchors
            // to the first / last shown class instead of a random middle one
            if (navIndex < 0 || navIndex >= cards.length) {
                navIndex = dir > 0 ? 0 : cards.length - 1;
            } else {
                navIndex = (navIndex + dir + cards.length) % cards.length;
            }
            const card = cards[navIndex];
            const node = nodes.get(card.dataset.name);
            if (!node) return;
            setNavHighlight(node);
            focusOn(node);
            card.scrollIntoView({ block: 'nearest' });
        }

        function navCenter() {
            setNavHighlight(null);
            navIndex = -1;
            resetView();
        }

        window.addEventListener('keydown', e => {
            if (e.ctrlKey || e.metaKey || e.altKey) return;
            const tag = (e.target && e.target.tagName) || '';
            if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;
            if (e.key === 'n' || e.key === 'N' || e.key === 'ArrowRight') { e.preventDefault(); navTo(1); }
            else if (e.key === 'p' || e.key === 'P' || e.key === 'ArrowLeft') { e.preventDefault(); navTo(-1); }
            else if (e.key === 'c' || e.key === 'C' || e.key === 'Home') { e.preventDefault(); navCenter(); }
        });

        // Restore the saved colour scheme (falling back to dark) and wire
        // up the selector
        const themeSelect = document.getElementById('themeSelect');
        let savedTheme = 'dark';
        try { savedTheme = localStorage.getItem('umlTheme') || 'dark'; }
        catch (err) {}
        themeSelect.value = THEMES3D[savedTheme] ? savedTheme : 'dark';
        themeSelect.addEventListener('change', () => applyTheme(themeSelect.value));
        applyTheme(themeSelect.value);

        // Layout: linear (the original arrangement) by default
        const layoutSelect = document.getElementById('layoutSelect');
        const nsLevelSelect = document.getElementById('nsLevelSelect');
        const nsLevelWrap = document.getElementById('nsLevelWrap');
        nsLevelSelect.addEventListener('change', () => {
            const v = parseInt(nsLevelSelect.value, 10);
            nsLevel = Math.max(1, Math.min(12, isNaN(v) ? 1 : v));
            nsLevelSelect.value = nsLevel;
            if (layoutSelect.value === 'namespace') applyLayout('namespace');
        });
        layoutSelect.addEventListener('change', () => {
            // The namespace-level selector only makes sense in namespace mode
            nsLevelWrap.style.display = (layoutSelect.value === 'namespace') ? '' : 'none';
            applyLayout(layoutSelect.value);
        });
        applyLayout(layoutSelect.value);

        // Diff mode: reveal the toggle and legend, report the counts, and wire
        // the "changes only / everything" selector (default: changes only)
        if (DIFF_MODE) {
            document.getElementById('diffWrap').style.display = '';
            document.getElementById('diffLegend').style.display = '';
            const counts = { added: 0, removed: 0, modified: 0 };
            classes.forEach(c => { if (counts[c.status] !== undefined) counts[c.status]++; });
            const diffStats = document.getElementById('diffStats');
            diffStats.textContent = 'Diff: ' + counts.added + ' added · '
                + counts.removed + ' removed · ' + counts.modified + ' modified';
            diffStats.style.display = '';
            const diffSelect = document.getElementById('diffSelect');
            diffSelect.addEventListener('change', () => {
                showOnlyChanges = (diffSelect.value === 'changes');
                applyVisibility();
            });
        }

        // Minimize the controls panel to a compact bar, and back again
        const controls = document.getElementById('controls');
        const minimizeBtn = document.getElementById('minimizeBtn');
        minimizeBtn.addEventListener('click', () => {
            const minimized = controls.classList.toggle('minimized');
            minimizeBtn.innerHTML = minimized ? '+' : '&ndash;';
            minimizeBtn.title = minimized ? 'Expand panel' : 'Minimize panel';
        });
    </script>
</body>
</html>)HTMLDOC";

}  // namespace uml
