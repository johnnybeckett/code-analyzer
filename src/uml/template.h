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
            /* Source-pane syntax colours, referenced by the .syn-* spans so
               the highlighter tracks the theme without any JS logic */
            --syn-keyword: #c792ea;
            --syn-type: #82aaff;
            --syn-string: #c3e88d;
            --syn-comment: #5b7086;
            --syn-number: #f78c6c;
            --syn-text: #e2e8f0;
            --pane-h: 33vh;   /* source-pane height; the drag handle retunes it */
            /* Ctrl+wheel content zoom: a shared multiplier the source pane,
               rendered diagrams, and the call-graph scale through (see the
               calc() sizes and applyContentZoom in the script). */
            --cz: 1;
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
            --syn-keyword: #7c3aed;
            --syn-type: #1d4ed8;
            --syn-string: #15803d;
            --syn-comment: #7d8fa5;
            --syn-number: #b45309;
            --syn-text: #0f172a;
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
            --syn-keyword: #d3b6ff;
            --syn-type: #87cefa;
            --syn-string: #98fb98;
            --syn-comment: #8f97e8;
            --syn-number: #ffd97d;
            --syn-text: #d9d9d9;
        }
        body {
            margin: 0;
            background: var(--bg);
            color: var(--text);
            font-family: Arial, Helvetica, sans-serif;
        }
        #scene {
            position: absolute;
            top: 0; left: 0; bottom: var(--pane-h);
            right: 380px;
        }
        #sidebar {
            position: absolute;
            top: 0; right: 0; bottom: var(--pane-h);
            width: 380px;
            overflow-y: auto;
            background: var(--panel);
            border-left: 1px solid var(--border);
            padding: 16px;
        }

        /* Resizable sidebar handle */
        #sidebar-resize-handle {
            position: absolute;
            left: -5px;
            top: 0;
            bottom: 0;
            width: 10px;
            cursor: col-resize;
            z-index: 100;
            background: var(--border);
            opacity: 0.3;
        }

        #sidebar-resize-handle:hover {
            opacity: 0.6;
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
        .filterLabel { margin: 8px 0 0; font-size: 11px; color: var(--muted); }
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
        #touchpad {
            display: none;
            position: fixed;
            bottom: 12px;
            left: 50%;
            transform: translateX(-50%);
            z-index: 40;
            background: var(--bg);
            border: 1px solid var(--border);
            border-radius: 10px;
            padding: 8px;
            box-shadow: 0 4px 16px rgba(0, 0, 0, 0.35);
        }
        body.touch #touchpad { display: block; }
        #touchpad .pad {
            display: grid;
            grid-template-columns: repeat(3, 48px);
            grid-auto-rows: 40px;
            gap: 6px;
            place-items: stretch;
        }
        #touchpad button {
            width: 48px;
            height: 40px;
            font-size: 18px;
            line-height: 1;
            color: var(--fg);
            background: var(--panel, rgba(128, 128, 128, 0.15));
            border: 1px solid var(--border);
            border-radius: 8px;
            cursor: pointer;
            touch-action: none;
            -webkit-user-select: none;
            user-select: none;
            -webkit-tap-highlight-color: transparent;
        }
        #touchpad button:active { opacity: 0.6; }
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
        /* Source pane: a resizable band across the bottom of the window that
           shows the focused class and the source files that implement it.
           Its height is the --pane-h custom property, retuned by the drag handle. */
        #source-pane {
            position: absolute;
            left: 0; right: 0; bottom: 0;
            height: var(--pane-h);
            background: var(--panel);
            border-top: 1px solid var(--border);
            display: flex;
            flex-direction: column;
            z-index: 5;
        }
        /* The thin drag bar at the pane's top edge; grab it to resize. */
        #pane-handle {
            flex: 0 0 auto;
            height: 8px;
            margin-top: -1px;
            cursor: ns-resize;
            background: transparent;
            position: relative;
        }
        #pane-handle::after {
            content: "";
            position: absolute;
            left: 50%; top: 50%;
            width: 44px; height: 3px;
            transform: translate(-50%, -50%);
            border-radius: 2px;
            background: var(--border);
        }
        #pane-head {
            flex: 0 0 auto;
            padding: 4px 14px 6px;
            border-bottom: 1px solid var(--border);
        }
        #pane-title {
            font-size: calc(15px * var(--cz));
            font-weight: 600;
            font-family: "SFMono-Regular", Consolas, "Liberation Mono", monospace;
        }
        #pane-vars {
            margin-top: 3px;
            font-size: calc(12px * var(--cz));
            color: var(--muted);
            font-family: "SFMono-Regular", Consolas, "Liberation Mono", monospace;
            line-height: 1.5;
        }
        .pane-var {
            margin-right: 14px;
            white-space: nowrap;
        }
        .pane-var a {
            color: var(--syn-type);
            text-decoration: underline dotted;
            cursor: pointer;
        }
        .pane-var a:hover { color: var(--syn-keyword); }
        /* Known type names in the source pane read as links (focus that class). */
        #pane-code a.syn-link {
            color: var(--syn-type);
            text-decoration: underline dotted;
            cursor: pointer;
        }
        #pane-code a.syn-link:hover { color: var(--syn-keyword); }
        #pane-tabs {
            flex: 0 0 auto;
            display: flex;
            flex-wrap: wrap;
            gap: 4px;
            padding: 6px 12px;
            border-bottom: 1px solid var(--border);
        }
        .pane-tab {
            padding: 3px 10px;
            font-size: calc(12px * var(--cz));
            border: 1px solid var(--border);
            border-radius: 6px;
            cursor: pointer;
            color: var(--text);
            background: var(--bg);
            max-width: 320px;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
        }
        .pane-tab:hover { border-color: var(--accent); }
        .pane-tab.active {
            border-color: var(--accent);
            background: var(--accent);
            color: var(--accent-contrast, #fff);
        }
        /* The code body: a scrollable monospace area with the highlighted source. */
        #pane-code {
            flex: 1 1 auto;
            overflow: auto;
            margin: 0;
            padding: 10px 14px;
            font-family: "SFMono-Regular", Consolas, "Liberation Mono", monospace;
            font-size: calc(12.5px * var(--cz));
            line-height: 1.55;
            white-space: pre;
            tab-size: 4;
            color: var(--syn-text);
            background: var(--bg);
        }
        #pane-code .syn-keyword { color: var(--syn-keyword); }
        #pane-code .syn-type { color: var(--syn-type); }
        #pane-code .syn-string { color: var(--syn-string); }
        #pane-code .syn-comment { color: var(--syn-comment); font-style: italic; }
        #pane-code .syn-number { color: var(--syn-number); }
        #pane-code .syn-placeholder { color: var(--muted); font-style: italic; }
        /* Diff mode: OLD | NEW columns of aligned line rows. Rows keep the
           pane's monospace/pre formatting; long lines widen the row and the
           pane scrolls, like the single-file view. */
        #pane-code .diff-split { display: flex; align-items: stretch; }
        #pane-code .diff-col { flex: 1 1 0; min-width: 0; }
        #pane-code .diff-col + .diff-col { border-left: 1px solid var(--border); }
        #pane-code .diff-col-head {
            padding: 4px 8px;
            margin: 0 0 6px;
            font-size: 11px;
            color: var(--muted);
            border-bottom: 1px solid var(--border);
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        #pane-code .diff-col-none { font-style: italic; }
        #pane-code .dl { display: flex; min-height: 1.55em; }
        #pane-code .dl-ln {
            flex: 0 0 auto;
            min-width: 3.5ch;
            padding-right: 8px;
            margin-right: 6px;
            text-align: right;
            color: var(--muted);
            border-right: 1px solid var(--border);
            user-select: none;
        }
        #pane-code .dl-src { flex: 1 1 auto; }
        #pane-code .dl-del { background: rgba(197, 34, 31, 0.18); }
        #pane-code .dl-add { background: rgba(24, 128, 56, 0.18); }
        #pane-code .dl-gap { height: 1.55em; }

        /* --- Renderable files: markdown HTML and server-rendered diagrams - */
        /* The pane is monospace/pre for code; rendered markdown needs normal
           block flow, so the wrappers switch it back. */
        #pane-code .md-body,
        #pane-code .dl-src.md-row { white-space: normal; }
        #pane-code .md-body h1, #pane-code .md-body h2,
        #pane-code .md-body h3, #pane-code .md-body h4 {
            margin: 10px 0 4px; font-size: 1.1em; color: var(--syn-type);
        }
        #pane-code .md-body p { margin: 4px 0; }
        #pane-code .md-body ul, #pane-code .md-body ol {
            margin: 4px 0 4px 22px; padding: 0;
        }
        #pane-code .md-body blockquote {
            margin: 6px 0; padding-left: 10px;
            border-left: 3px solid var(--border); color: var(--muted);
        }
        #pane-code .md-body hr {
            border: none; border-top: 1px solid var(--border); margin: 8px 0;
        }
        #pane-code .md-code,
        #pane-code .dl-src.md-row .md-code {
            background: rgba(127, 127, 127, 0.18);
            border-radius: 3px; padding: 0 3px;
        }
        #pane-code .md-link { color: var(--syn-string); }
        #pane-code .md-pre {
            background: rgba(127, 127, 127, 0.10);
            border: 1px solid var(--border); border-radius: 4px;
            padding: 6px 8px; margin: 6px 0; overflow-x: auto;
        }
        /* A markdown diff row: one source line per row, so headings keep the
           row's text size and each line's paragraph flows inline. */
        #pane-code .dl-src.md-row h1, #pane-code .dl-src.md-row h2,
        #pane-code .dl-src.md-row h3, #pane-code .dl-src.md-row h4,
        #pane-code .dl-src.md-row h5, #pane-code .dl-src.md-row h6 {
            font-size: 1em; font-weight: 600; margin: 0;
        }
        #pane-code .dl-src.md-row p { margin: 0; display: inline; }
        /* Zoom scales the wrap, and the svg's max-width:100% follows it, so
           the diagram grows/shrinks inside the (scrollable) pane. */
        #pane-code .diagram-wrap { padding: 10px; width: calc(100% * var(--cz)); }
        #pane-code .diagram-wrap svg {
            max-width: 100%; max-height: 65vh; height: auto;
        }
        #pane-code .render-fallback {
            padding: 4px 8px; margin-bottom: 6px;
            font-size: 12px; font-style: italic; color: var(--muted);
            border-bottom: 1px dashed var(--border);
        }

        /* --- Call-graph view: whole-project method call edges -------------- */
        /* A full-screen overlay of one node per (class, method) with directed
           edges from each method's called_methods. It lives in #callgraph and
           is hidden by default; Ctrl+wheel scales #callgraph-inner in place. */
        #callgraph {
            position: fixed; inset: 0; z-index: 40;
            background: var(--bg);
            display: flex; flex-direction: column;
        }
        #callgraph.hidden { display: none; }
        #callgraph-head {
            display: flex; align-items: center; justify-content: space-between;
            padding: 10px 14px;
            background: var(--panel);
            border-bottom: 1px solid var(--border);
        }
        #callgraph-title { font-size: 15px; font-weight: 600; color: var(--text); }
        #callgraph-status {
            padding: 8px 14px; font-size: 12px; color: var(--muted);
            border-bottom: 1px solid var(--border);
        }
        #callgraph-status .amb { color: #b06000; }
        #callgraph-scroll { flex: 1 1 auto; overflow: auto; padding: 14px; }
        /* The graph is drawn at a fixed pixel size; Ctrl+wheel zoom is a
           transform:scale on this wrapper (transform-origin: top left), so it
           is scale-invariant and the scroll region grows with it. */
        #callgraph-inner {
            display: inline-block;
            transform-origin: top left;
            transform: scale(var(--cz));   /* Ctrl+wheel content zoom */
        }
        #callgraph-inner text { fill: var(--text); }
        #callgraph-inner .cgcol { fill: var(--muted); }
        #callgraph-inner .cgnode rect { fill: var(--panel); stroke: var(--border-strong); }
        #callgraph-inner .cgedge { fill: none; stroke: var(--muted); stroke-width: 1.25; }
        #callgraph-inner .cgedge.amb { stroke: #b06000; stroke-dasharray: 4 3; }

        /* --- Diff mode: left file list, context menu, review comments ------ */
        #file-list {
            position: absolute;
            top: 0; left: 0; bottom: var(--pane-h);
            width: 210px;
            background: var(--panel);
            border-right: 1px solid var(--border);
            display: flex;
            flex-direction: column;
            z-index: 5;
        }
        #file-list-head {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 8px 10px;
            border-bottom: 1px solid var(--border);
            font-weight: 600;
            font-size: 0.85rem;
        }
        #file-list-head a {
            font-size: 0.7rem;
            font-weight: 500;
            padding: 3px 9px;
            background: var(--btn2);
            color: var(--text);
            border: 1px solid var(--border-strong);
            border-radius: 4px;
            text-decoration: none;
        }
        #file-list-head a:hover { background: var(--accent); }
        #file-list-body { flex: 1 1 auto; overflow-y: auto; padding: 4px 0; }
        .fileEntry {
            display: flex;
            align-items: center;
            gap: 6px;
            padding: 4px 10px;
            font-size: 0.8rem;
            color: var(--muted);
            cursor: pointer;
        }
        .fileEntry:hover { background: rgba(66, 133, 244, 0.12); color: var(--text); }
        .fileEntry.active { background: rgba(66, 133, 244, 0.24); color: var(--text); }
        .fileEntry-name {
            flex: 1 1 auto;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        .fileEntry-badge {
            flex: none;
            font-size: 0.6rem;
            text-transform: uppercase;
            letter-spacing: 0.03em;
            padding: 1px 5px;
            border-radius: 3px;
            color: #fff;
        }
        .st-added { background: rgba(24, 128, 56, 0.92); }
        .st-removed { background: rgba(197, 34, 31, 0.92); }
        .st-modified { background: rgba(224, 140, 0, 0.9); }
        .st-unchanged { background: rgba(100, 116, 139, 0.75); }
        /* The left list takes 210px; slide the scene and controls clear of it. */
        body.diffActive #scene { left: 214px; }
        body.diffActive #controls { left: 226px; }

        #ctxMenu {
            position: fixed;
            z-index: 60;
            min-width: 190px;
            background: var(--panel);
            border: 1px solid var(--border-strong);
            border-radius: 6px;
            box-shadow: 0 10px 28px rgba(0, 0, 0, 0.5);
            padding: 4px 0;
        }
        .ctxItem {
            padding: 6px 14px;
            font-size: 0.82rem;
            color: var(--text);
            cursor: pointer;
            white-space: nowrap;
        }
        .ctxItem:hover { background: rgba(66, 133, 244, 0.22); }
        .ctxItem.disabled { color: var(--faint); cursor: default; }
        .ctxItem.disabled:hover { background: none; }
        .ctxItem a { color: var(--text); text-decoration: none; }
        .ctxSep { height: 1px; background: var(--border); margin: 4px 0; }

        /* Inline review-comment badge, appended to a diff row. */
        .cm-badge {
            flex: none;
            min-width: 16px;
            height: 16px;
            line-height: 16px;
            text-align: center;
            padding: 0 4px;
            border-radius: 8px;
            background: var(--accent);
            color: #fff;
            font-size: 0.62rem;
            cursor: help;
        }

        /* Floating comment-composition panel (kept out of the two-column diff
           so it never shifts the row alignment). */
        #commentForm {
            position: fixed;
            z-index: 60;
            width: 340px;
            box-sizing: border-box;
            background: var(--panel);
            border: 1px solid var(--border-strong);
            border-radius: 6px;
            box-shadow: 0 10px 28px rgba(0, 0, 0, 0.5);
            padding: 10px;
        }
        #commentForm .cf-head {
            font-size: 0.82rem;
            font-weight: 600;
            margin-bottom: 6px;
        }
        #commentForm .cf-meta {
            font-size: 0.72rem;
            color: var(--muted);
            margin-bottom: 6px;
            word-break: break-all;
        }
        #commentForm textarea {
            width: 100%;
            min-height: 58px;
            box-sizing: border-box;
            resize: vertical;
            background: var(--bg);
            color: var(--text);
            border: 1px solid var(--border-strong);
            border-radius: 4px;
            padding: 5px 7px;
            font-family: inherit;
            font-size: 0.8rem;
        }
        #commentForm .cf-btns { display: flex; gap: 6px; margin-top: 8px; }
        #commentForm button {
            font-size: 0.76rem;
            padding: 4px 12px;
            border-radius: 4px;
            cursor: pointer;
            border: 1px solid var(--border-strong);
        }
        #commentForm .cf-submit { background: var(--accent); color: #fff; border-color: transparent; }
        #commentForm .cf-cancel { background: var(--btn2); color: var(--text); }
        #commentForm .cf-err { margin-top: 7px; font-size: 0.72rem; color: #f87171; }

        /* Anchor flash after a resync. box-shadow (not background) so it layers
           over the .dl-del / .dl-add tints instead of replacing them. */
        .anchor-flash { animation: anchorFlash 1.2s ease-out; }
        @keyframes anchorFlash {
            0% { box-shadow: inset 0 0 0 3px var(--accent); }
            100% { box-shadow: inset 0 0 0 0 rgba(66, 133, 244, 0); }
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
        <div class="filterLabel">Name regex</div>
        <input type="text" id="filterInput" placeholder="regex, e.g. ^Parser|Generator$"
               onkeydown="if (event.key === 'Enter') applyFilter()">
        <div class="filterLabel">Namespace regex</div>
        <input type="text" id="nsFilterInput" placeholder="regex on the namespace, e.g. ^core|util\."
               onkeydown="if (event.key === 'Enter') applyFilter()">
        <button onclick="applyFilter()">Apply Filter</button>
        <button class="secondary" onclick="resetFilter()">Reset</button>
        <button class="secondary" onclick="resetView()">Reset View</button>
        <h3>Analysis</h3>
        <button id="callgraphBtn" class="secondary" onclick="toggleCallGraph()">Call Graph</button>
        <div class="filterLabel">Ctrl+wheel zooms the source / diagram / call-graph content</div>
        <div class="filterLabel">Max classes drawn (nearest first)</div>
        <input type="number" id="lodInput" min="10" step="50" value="500"
               onchange="setLodLimit(this.value)">
        <div id="diffWrap" style="display:none">
            <h3>Diff view</h3>
            <select id="diffSelect">
                <option value="changes" selected>Changes only</option>
                <option value="everything">Show everything</option>
            </select>
            <label style="display:block;margin-top:6px;cursor:pointer">
                <input type="checkbox" id="ignoreWs"> Ignore whitespace
            </label>
        </div>
        <h3>Layout</h3>
        <select id="layoutSelect">
            <option value="linear">Linear (hierarchy)</option>
            <option value="namespace">Grouped by namespace</option>
            <option value="circular">Circular</option>
            <option value="hierarchical">Hierarchical (inheritance)</option>
            <option value="cmake" style="display:none">CMake</option>
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
        <h3>Mouse</h3>
        <label style="display:block;margin-top:6px;cursor:pointer">
            <input type="checkbox" id="invertH"> Invert horizontal movement
        </label>
        <label style="display:block;margin-top:6px;cursor:pointer">
            <input type="checkbox" id="invertV"> Invert vertical movement
        </label>
        <h3>Colour scheme</h3>
        <select id="themeSelect">
            <option value="dark">Dark</option>
            <option value="light">Light</option>
            <option value="blue">Vim darkblue</option>
        </select>
        <div id="hint">drag: orbit &middot; wheel / ctrl+drag: zoom &middot; shift+drag: pan &middot; alt: fine &middot; double-click a class to focus, or double-click a member&rsquo;s type to jump to that class, or double-click an inheritance line to jump to its far end &middot; click a class name in the sidebar to focus &middot; click a type in the source pane to jump to it &middot; keys: n / &rarr; next class &middot; p / &larr; previous class &middot; c center view &middot; w / a / s / d move the view</div>
        <div id="stats"></div>
        <div id="diffStats" style="display:none"></div>
        </div>
    </div>
    <div id="touchpad" aria-label="touch controls">
        <div class="pad">
            <button data-act="prev"   title="Previous class" type="button">&lsaquo;</button>
            <button data-act="up"     title="Move up"    type="button">&#8593;</button>
            <button data-act="zoomin" title="Zoom in"    type="button">+</button>
            <button data-act="left"   title="Move left"  type="button">&#8592;</button>
            <button data-act="center" title="Center view" type="button">&#9678;</button>
            <button data-act="zoomout" title="Zoom out"  type="button">&minus;</button>
            <button data-act="right"  title="Move right" type="button">&#8594;</button>
            <button data-act="down"   title="Move down"  type="button">&#8595;</button>
            <button data-act="next"   title="Next class" type="button">&rsaquo;</button>
        </div>
    </div>
    <div id="sidebar"><h2>Classes</h2></div>

    <div id="file-list" style="display:none">
        <div id="file-list-head">
            <span>Files</span>
            <a id="exportBtn" href="/export/review" download="review.md"
               title="Export review comments as a Markdown file">Export</a>
        </div>
        <div id="file-list-body"></div>
    </div>

    <div id="source-pane">
        <div id="pane-handle" title="Drag to resize the source pane"></div>
        <div id="pane-head">
            <div id="pane-title">No class focused</div>
            <div id="pane-vars"></div>
        </div>
        <div id="pane-tabs"></div>
        <pre id="pane-code"><span class="syn-placeholder">double-click a class to show its source here</span></pre>
    </div>

    <div id="callgraph" class="hidden">
        <div id="callgraph-head">
            <div id="callgraph-title">Call Graph &middot; whole project</div>
            <button id="callgraphClose" class="secondary" onclick="toggleCallGraph()">Close</button>
        </div>
        <div id="callgraph-status"></div>
        <div id="callgraph-scroll">
            <div id="callgraph-inner"></div>
        </div>
    </div>

    <script>
        // Class data produced by the code analyzer.
        //   DIFF_MODE true  -> two files given (older newer): OLD_CLASSES is the
        //                      baseline, NEW_CLASSES the new state.
        //   DIFF_MODE false -> one file: NEW_CLASSES holds the combined set,
        //                      OLD_CLASSES is [].
        const DIFF_MODE = __DIFF_MODE__;
        const OLD_CLASSES = __OLD_CLASSES_JSON__;
        const NEW_CLASSES = __NEW_CLASSES_JSON__;
        // CMake targets from the analyzed directory (empty when the input has
        // none). Each: {name, kind, alias_of, sources[], links[]}.
        const CMAKE_TARGETS = __CMAKE_JSON__;

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

        // --- Diff file list (left panel) ---
        // Group the diffed classes by the (oldPath, newPath) pair — the thing a
        // file diff is keyed on. NOT c.file: that is the NEW path for added/
        // modified/unchanged classes but the OLD path for removed ones, so the
        // pair has to be resolved from oldMerged/newMerged by the same
        // namespace::name key computeDiff uses. A file's status is the worst of
        // its classes (added/removed beats modified beats unchanged).
        const FILE_RANK = { unchanged: 0, modified: 1, added: 2, removed: 2 };
        const fileKeyOf = (o, n) => (o || '') + '||' + (n || '');
        let fileEntries = [];    // [{oldPath, newPath, status, names:[...]}]
        let fileIndex = new Map(); // fileKey -> index into fileEntries
        if (DIFF_MODE) {
            const fileOf = list => {
                const m = new Map(list.map(c => [(c.namespace || '') + '::' + c.name, c]));
                return key => { const r = m.get(key); return (r && r.file) ? r.file : null; };
            };
            const oldFileOf = fileOf(oldMerged);
            const newFileOf = fileOf(newMerged);
            const groups = new Map();
            for (const c of classes) {
                const key = (c.namespace || '') + '::' + c.name;
                const oldPath = oldFileOf(key);
                const newPath = newFileOf(key);
                const fkey = fileKeyOf(oldPath, newPath);
                if (!groups.has(fkey)) {
                    groups.set(fkey, { oldPath, newPath, status: 'unchanged', names: [] });
                }
                const g = groups.get(fkey);
                if ((FILE_RANK[c.status] || 0) > (FILE_RANK[g.status] || 0)) g.status = c.status;
                g.names.push(c.name);
            }
            fileEntries = [...groups.values()];
            fileIndex = new Map(fileEntries.map((e, i) => [fileKeyOf(e.oldPath, e.newPath), i]));
        }

        // --- Visibility state (diff filter + name + namespace filters) ---
        // Default: in diff mode hide unchanged classes so only the changes show.
        let showOnlyChanges = DIFF_MODE;
        let currentRegex = null;      // name regex filter
        let currentNsRegex = null;    // namespace regex filter
        // name -> status (undefined in non-diff mode). Used to hide 'unchanged'
        // nodes when "Changes only" is active. (Name-keyed: same-named classes in
        // different namespaces are already merged into one node — pre-existing.)
        const statusByName = new Map();
        classes.forEach(c => { if (c.status) statusByName.set(c.name, c.status); });
        const diffVisible = status => !(DIFF_MODE && showOnlyChanges && status === 'unchanged');
        const regexVisible = name => !currentRegex || currentRegex.test(name);
        // A class matches the namespace filter on its own namespace.
        // (External stubs have none — they follow their children instead.)
        const nsVisible = name => {
            if (!currentNsRegex) return true;
            const n = nodes.get(name);
            return !!(n && n.cls) && currentNsRegex.test(n.cls.namespace || '');
        };
        const classVisible = name =>
            regexVisible(name) && nsVisible(name) && diffVisible(statusByName.get(name));

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

        // --- CMake targets -------------------------------------------------
        // Each target from the analyzed directory becomes a node in the same
        // nodes Map, flagged n.cmake so the class layouts, span and sidebar
        // leave it out; only the 'cmake' layout shows it. A synthetic cls
        // drives the box: the target name as the header (kind-prefixed) and
        // its source files as member rows. Aliases render as dashed stubs.
        // A real class of the same name wins (node already present).
        const cmakeTargets = (CMAKE_TARGETS || [])
            .filter(t => t && t.name);
        const cmakeKnown = new Set(cmakeTargets.map(t => t.name));
        for (const t of cmakeTargets) {
            if (nodes.has(t.name)) continue;
            const srcs = t.sources || [];
            nodes.set(t.name, {
                name: t.name,
                cls: {
                    name: t.name, namespace: '',
                    kind: (t.kind && t.kind !== 'class') ? t.kind : 'library',
                    file: null, methods: [],
                    variables: srcs.map(s => ({ type: '', name: s })),
                    __cmake: true,
                },
                external: t.kind === 'alias',
                cmake: true, cmakeTarget: t,
                parent: null, depth: 0, x: 0, y: 0, z: 0,
                w: 0, h: 0, mesh: null, sideMat: null,
            });
        }

        // CMake dependency edges from target_link_libraries: an arrow from a
        // target to each linked target that is itself a known target (external
        // libs like `z` and generator expressions have no node, so no edge).
        const edgesCmake = [];
        for (const t of cmakeTargets) {
            for (const dep of (t.links || [])) {
                if (dep && dep !== t.name && cmakeKnown.has(dep)) {
                    edgesCmake.push({ from: t.name, to: dep });
                }
            }
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
        let currentMode = 'linear';   // active layout mode (set by applyLayout)

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
            const roots = [...nodes.values()].filter(n => !n.cmake && !n.parent);
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
            const laid = [...nodes.values()].filter(n => !n.cmake);
            for (const n of laid) cz += n.z;
            cz /= laid.length;
            laid.forEach(n => { n.z -= cz; });
        }

        // Hierarchical: classes arranged in a dependency hierarchy based on inheritance
        // relationships, with base classes above derived classes.
        function layoutHierarchical() {
            const list = [...nodes.values()]
                .filter(n => !n.cmake)  // Only apply to regular classes, not CMake targets
                .sort((a, b) => a.name.localeCompare(b.name));
            if (!list.length) return;

            // Build inheritance graph for topological sorting
            const inDegree = new Map();  // class -> number of incoming dependencies (base classes)
            const outEdges = new Map();  // class -> list of outgoing dependent classes

            // Initialize all nodes with zero in-degree
            for (const n of list) {
                inDegree.set(n.name, 0);
                outEdges.set(n.name, []);
            }

            // Calculate in-degrees and build dependency map from inheritance edges
            for (const edge of edges) {
                const from = edge.from;
                const to = edge.to;

                // Only count dependencies that are within our known classes
                if (inDegree.has(from) && inDegree.has(to)) {
                    // Inheritance relationships are from child to parent, so we increment
                    // the in-degree of the parent (to) class
                    inDegree.set(to, inDegree.get(to) + 1);
                    outEdges.get(from).push(to);
                }
            }

            // Topological sort to determine layering
            const layers = [];
            const queue = [];

            // Find all nodes with no incoming dependencies (in-degree = 0)
            for (const [target, degree] of inDegree.entries()) {
                if (degree === 0) {
                    queue.push(target);
                }
            }

            // Process nodes in topological order to create layers
            while (queue.length > 0) {
                const layer = [];
                const size = queue.length;

                for (let i = 0; i < size; i++) {
                    const target = queue.shift();
                    layer.push(target);

                    // For each dependent of this node, reduce its in-degree
                    for (const dependent of outEdges.get(target)) {
                        const newDegree = inDegree.get(dependent) - 1;
                        inDegree.set(dependent, newDegree);

                        if (newDegree === 0) {
                            queue.push(dependent);
                        }
                    }
                }

                layers.push(layer);
            }

            // Position nodes in layers
            const layerSpacing = 25;
            const nodeSpacing = 15;
            let y = 0;

            for (const layer of layers) {
                // Sort nodes within each layer by name for consistent positioning
                layer.sort();

                // Calculate starting x position to center the layer
                const totalWidth = (layer.length - 1) * nodeSpacing;
                const startX = -totalWidth / 2;

                let x = startX;
                for (const className of layer) {
                    const node = nodes.get(className);
                    if (node) {
                        node.x = x;
                        node.y = y;
                        node.z = 0;
                        node.rotY = 0;
                        x += nodeSpacing;
                    }
                }

                y += layerSpacing;
            }
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
                if (n.cmake) continue;
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

        // Place `list` as one standing ring on the x-z plane (y = 0), each
        // node's content face turned toward the center (n.rotY, applied by
        // applyLayout). Radius is large enough that the chord between
        // adjacent boxes is >= maxW + 2*GAP; a lone node has no neighbours.
        function placeRing(list) {
            const N = list.length;
            const GAP = 3;
            const maxW = N ? Math.max(...list.map(n => n.w)) : 0;
            let R = maxW + GAP * 2;
            if (N > 1) R = Math.max(R, (maxW / 2 + GAP) / Math.sin(Math.PI / N));
            const TWO_PI = Math.PI * 2;
            list.forEach((n, i) => {
                const a = (i + 0.5) * TWO_PI / N;
                n.x = R * Math.cos(a);
                n.z = R * Math.sin(a);
                n.y = 0;
                n.rotY = a - Math.PI / 2;   // +z content face points at the origin
            });
        }

        // Circular ("Stonehenge"): one giant ring of every class, neighbours
        // side by side, content faces toward the center, seen from an
        // elevated camera (homeView lifts the pitch in this mode).
        function layoutCircular() {
            const list = [...nodes.values()]
                .filter(n => !n.cmake)
                .sort((a, b) =>
                    groupKey(a).localeCompare(groupKey(b)) || a.name.localeCompare(b.name));
            placeRing(list);
        }

        // CMake: the analyzed project's targets stand in a hierarchical layout
        // that minimizes dependency crossings. The targets are arranged in
        // layers based on their dependency relationships, with dependencies
        // flowing from top to bottom.
        function layoutCmake() {
            const list = [...nodes.values()]
                .filter(n => n.cmake)
                .sort((a, b) => a.name.localeCompare(b.name));
            if (!list.length) return;

            // Build dependency graph for topological sorting
            const inDegree = new Map();  // target -> number of incoming dependencies
            const outEdges = new Map();  // target -> list of outgoing dependent targets

            // Initialize all nodes with zero in-degree
            for (const n of list) {
                inDegree.set(n.name, 0);
                outEdges.set(n.name, []);
            }

            // Calculate in-degrees and build dependency map
            for (const edge of edgesCmake) {
                const from = edge.from;
                const to = edge.to;

                // Only count dependencies that are within our known targets
                if (inDegree.has(from) && inDegree.has(to)) {
                    inDegree.set(to, inDegree.get(to) + 1);
                    outEdges.get(from).push(to);
                }
            }

            // Topological sort to determine layering
            const layers = [];
            const queue = [];

            // Find all nodes with no incoming dependencies (in-degree = 0)
            for (const [target, degree] of inDegree.entries()) {
                if (degree === 0) {
                    queue.push(target);
                }
            }

            // Process nodes in topological order to create layers
            while (queue.length > 0) {
                const layer = [];
                const size = queue.length;

                for (let i = 0; i < size; i++) {
                    const target = queue.shift();
                    layer.push(target);

                    // For each dependent of this node, reduce its in-degree
                    for (const dependent of outEdges.get(target)) {
                        const newDegree = inDegree.get(dependent) - 1;
                        inDegree.set(dependent, newDegree);

                        if (newDegree === 0) {
                            queue.push(dependent);
                        }
                    }
                }

                layers.push(layer);
            }

            // Position nodes in layers
            const layerSpacing = 25;
            const nodeSpacing = 15;
            let y = 0;

            for (const layer of layers) {
                // Sort nodes within each layer by name for consistent positioning
                layer.sort();

                // Calculate starting x position to center the layer
                const totalWidth = (layer.length - 1) * nodeSpacing;
                const startX = -totalWidth / 2;

                let x = startX;
                for (const targetName of layer) {
                    const node = nodes.get(targetName);
                    if (node) {
                        node.x = x;
                        node.y = y;
                        node.z = 0;
                        node.rotY = 0;
                        x += nodeSpacing;
                    }
                }

                y += layerSpacing;
            }
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

        // The CMake layout draws the target graph instead of inheritance;
        // every other layout draws the inheritance edges.
        const activeEdges = () => (currentMode === 'cmake') ? edgesCmake : edges;
        let edgeObjs = [];
        function rebuildEdges() {
            for (const e of edgeObjs) {
                diagram.remove(e.line);
                e.line.geometry.dispose();   // edgeMat is shared — keep it
            }
            edgeObjs = activeEdges()
                .map(e => ({ from: e.from, to: e.to, line: makeEdge(nodes.get(e.from), nodes.get(e.to)) }))
                .filter(e => e.line);
            edgeObjs.forEach(e => diagram.add(e.line));
        }

        for (const n of nodes.values()) diagram.add(n.mesh);
        const nodeList = [...nodes.values()];   // stable list for picking / LOD

        // Faint floor grid as a depth cue (rebuilt when the layout or theme changes)
        let span = 100, gridY = 0;
        function computeSpan() {
            const list = [...nodes.values()].filter(n => n.cmake === (currentMode === 'cmake'));
            if (!list.length) { span = 100; gridY = 0; return; }
            span = Math.max(...list.map(n => Math.hypot(n.x, n.y, n.z) + Math.max(n.w, n.h)));
            gridY = Math.min(...list.map(n => n.y - n.h / 2)) - 3;
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
            currentMode = mode;
            if (mode === 'namespace') layoutNamespaces();
            else if (mode === 'circular') { currentGroups = []; layoutCircular(); }
            else if (mode === 'cmake') { currentGroups = []; layoutCmake(); }
            else if (mode === 'hierarchical') { currentGroups = []; layoutHierarchical(); }
            else { currentGroups = []; layoutLinear(); }
            for (const n of nodes.values()) {
                n.mesh.position.set(n.x, n.y, n.z);
                // Only the Stonehenge ring stands on edge, faces turned to
                // the center; every other layout is flat and front-facing
                n.mesh.rotation.y = (mode === 'circular' && !n.cmake) ? (n.rotY || 0) : 0;
            }
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
                       target: { x: 0, y: 0, z: 0 },
                       contentZoom: 1,   // Ctrl+wheel: scales pane/diagram/call-graph
                       invertH: false,   // "Invert horizontal movement" checkbox
                       invertV: false }; // "Invert vertical movement" checkbox

        // Wire up the invert toggles to change behavior
        document.getElementById('invertH').addEventListener('change', e => {
            view.invertH = e.target.checked;
        });
        document.getElementById('invertV').addEventListener('change', e => {
            view.invertV = e.target.checked;
        });

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
        // of both the initial setup and "Reset View". The Stonehenge ring
        // stands in the x-z plane, so its home view is elevated (+0.9 rad)
        // to look down on it.
        function homeView() {
            return { yaw: 0, pitch: currentMode === 'circular' ? 0.9 : 0,
                     dist: initDist, tx: 0, ty: 0, tz: 0 };
        }
        const focus = { active: false, ...homeView() };

        // --- Level of detail: only the nearest classes are drawn ---
        // Drawing every box every frame is what makes a 10k-class diagram
        // unresponsive. The filters decide which classes *exist*; LOD then draws
        // only the lodLimit nearest of those (plus any pinned ones), re-picking
        // whenever the camera moves. The sidebar keeps the full filtered list.
        let lodLimit = 500;
        const pinned = new Set();      // focused classes always stay drawn
        let lodPool = [];             // nodes that pass the filters (set by applyVisibility)
        let lastLodCam = null;        // camera position LOD was last computed from
        const heldKeys = new Set();   // w / a / s / d currently held

        function applyLod() {
            const p = camera.position;
            let drawn;
            if (lodPool.length <= lodLimit) {
                drawn = new Set(lodPool.map(n => n.name));
            } else {
                lodPool.sort((a, b) =>
                    (a.x - p.x) ** 2 + (a.y - p.y) ** 2 + (a.z - p.z) ** 2
                  - (b.x - p.x) ** 2 + (b.y - p.y) ** 2 + (b.z - p.z) ** 2);
                drawn = new Set(lodPool.slice(0, lodLimit).map(n => n.name));
            }
            pinned.forEach(n => drawn.add(n.name));
            for (const n of nodes.values()) n.mesh.visible = drawn.has(n.name);
            edgeObjs.forEach(e => {
                const a = nodes.get(e.from), b = nodes.get(e.to);
                e.line.visible = a.mesh.visible && b.mesh.visible;
            });
            // A namespace frame stays while any class inside it is drawn
            panels.forEach(pl => {
                pl.mesh.visible = pl.names.some(name => {
                    const n = nodes.get(name);
                    return n && !n.external && n.mesh.visible;
                });
            });
        }

        // True when the camera moved since the last applyLod (or it never ran)
        function lodMoved() {
            const p = camera.position;
            const moved = !lastLodCam ||
                (p.x - lastLodCam.x) ** 2 + (p.y - lastLodCam.y) ** 2
                + (p.z - lastLodCam.z) ** 2 > 1e-4;
            lastLodCam = { x: p.x, y: p.y, z: p.z };
            return moved;
        }

        function setLodLimit(value) {
            const n = parseInt(value, 10);
            if (!isNaN(n) && n >= 1) lodLimit = n;
            applyLod();
        }

        // --- Streaming / nearest-N (large projects) ------------------------
        // Building a three.js box (canvas + texture + geometry) for every class
        // is what eats the client's RAM on a 10k-class diagram. When the server
        // reports a large index we switch the scene to *streaming*: each node
        // keeps a cheap placeholder holder, and only the classes inside the
        // camera's window hold a live box. Boxes are materialized as they enter
        // the window and disposed (.dispose()) as they leave — the bounded live
        // set *is* the RAM fix. The server's /classes/index and /classes/near
        // endpoints drive which classes are near the view; a grid-cell cache of
        // the last few cells makes a pan-back instant (no round-trip).
        //
        // The existing eager build above stays the DEFAULT (small projects and
        // the no-server test render exactly as before). Streaming only engages
        // when /classes/index reports a project past the SMALL threshold, so
        // nothing regresses until it is warranted.
        const streaming = (function () {
            const SMALL = 2000;        // index count below this: render the whole set at once
            const WINDOW = 400;        // live boxes kept around the view target
            const CACHE_CELLS = 24;    // grid cells remembered for an instant pan-back
            let on = false;
            let index = null;          // { count, bounds, cellSize } from /classes/index
            let seq = 0;               // sequence token: drop stale responses
            let inFlight = false;      // one-in-flight guard
            let timer = null;          // debounce handle (camera settle)
            const gridCache = new Map(); // cellKey -> Set(names), LRU-capped

            const cellKey = (x, y, z) => {
                const c = (index && index.cellSize) || 1;
                return Math.round(x / c) + ':' + Math.round(y / c) + ':' + Math.round(z / c);
            };
            // Remember a cell; evict the oldest once past CACHE_CELLS.
            function rememberCell(key, names) {
                if (gridCache.has(key)) gridCache.delete(key);
                gridCache.set(key, names);
                while (gridCache.size > CACHE_CELLS) {
                    gridCache.delete(gridCache.keys().next().value);
                }
            }
            // Give a node a live box, rebuilding it from its inline class data
            // when it has none. The holder placeholder keeps n.mesh valid for
            // picking / layout / LOD whether or not a box is currently live.
            function materialize(n) {
                if (!n || n._box || !n.cls) return;   // already live / stub: no box
                const box = makeClassBox(n.cls, !n.external && isAbstract(n.cls), n.external);
                const holder = n._holder || n.mesh;
                box.mesh.position.set(0, 0, 0);       // child of the holder at its origin
                holder.add(box.mesh);
                box.mesh.userData.node = n;           // picking anchor (dblclick)
                n._box = box.mesh;
                n.sideMat = box.side;
                n.rows = box.rows; n.ch = box.ch; n.w = box.w; n.h = box.h;
            }
            // Free a node's live box; the (now empty) holder stays, so layout /
            // LOD / picking keep working and a later materialize re-attaches it.
            function dispose(n) {
                const b = n && n._box;
                if (!b) return;
                const holder = n._holder;
                (holder || b.parent).remove(b);
                b.geometry.dispose();
                const mats = Array.isArray(b.material) ? b.material : [b.material];
                for (const m of mats) {
                    if (m && m.map) m.map.dispose();
                    if (m) m.dispose();
                }
                n._box = null;
            }
            // Reconcile the live set to `names` (the classes in the window):
            // materialize the new ones, dispose the ones that left.
            function reconcile(names) {
                const want = new Set(names);
                for (const n of nodes.values()) {
                    if (n.external || !n.cls) continue;   // stubs never materialize
                    if (want.has(n.name)) materialize(n);
                    else dispose(n);
                }
                applyLod();   // re-run LOD visibility over the (possibly new) meshes
            }
            // Seed a fresh window around the current view target. The grid
            // cache makes a pan-back instant (reconcile from memory); otherwise
            // fetch the nearest-N from the server. One-in-flight + stale-ignore.
            function requestNear() {
                if (!on) return;
                const t = view.target;
                const key = cellKey(t.x, t.y, t.z);
                if (gridCache.has(key)) { reconcile(gridCache.get(key)); return; }
                if (inFlight) return;
                const my = ++seq;
                inFlight = true;
                const url = '/classes/near?x=' + t.x + '&y=' + t.y +
                            '&z=' + t.z + '&count=' + WINDOW;
                fetch(url)
                    .then(r => (r.ok ? r.json() : Promise.reject(new Error('near ' + r.status))))
                    .then(list => {
                        if (my !== seq) return;          // a newer request superseded it
                        const names = (list || []).map(o => o && o.name).filter(Boolean);
                        rememberCell(key, new Set(names));
                        reconcile(names);
                    })
                    .catch(() => { /* keep the current live set; retry on next move */ })
                    .finally(() => { if (my === seq) inFlight = false; });
            }
            // Camera moved: debounce, then refresh the window. Cheap no-op when
            // streaming is off (small project / no index).
            function onMove() {
                if (!on) return;
                if (timer) clearTimeout(timer);
                timer = setTimeout(requestNear, 120);
            }
            // Upgrade the already-rendered scene to streaming: wrap each real
            // box in a cheap placeholder holder (so n.mesh stays valid), then
            // dispose everything outside the initial window. A small project,
            // an offline page, or a missing index all leave the eager build in
            // place — the .catch below is the no-regression path.
            function maybeEnable() {
                if (typeof fetch !== 'function') return;
                fetch('/classes/index')
                    .then(r => (r.ok ? r.json() : Promise.reject(new Error('index ' + r.status))))
                    .then(ix => {
                        if (!ix || !ix.count || ix.count < SMALL) return;   // small: keep all
                        on = true;
                        index = ix;
                        for (const n of nodes.values()) {
                            if (!n.mesh) continue;
                            const holder = new THREE.Group();
                            holder.position.copy(n.mesh.position);
                            holder.rotation.copy(n.mesh.rotation);
                            holder.visible = n.mesh.visible;
                            n.mesh.userData.node = n;              // picking anchor
                            const parent = n.mesh.parent;
                            if (parent) parent.remove(n.mesh);
                            holder.add(n.mesh);
                            if (parent) parent.add(holder);
                            n._holder = holder;
                            n._box = n.mesh;                        // currently-live box
                            n.mesh = holder;                        // n.mesh stays valid
                        }
                        requestNear();                              // seed the first window
                    })
                    .catch(() => { /* no index / small / offline: the eager build stands */ });
            }
            return { maybeEnable, onMove };
        })();

        container.addEventListener('mousedown', e => {
            if (touchMode) return;  // touch devices use the touch model below
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
                // The invert toggles flip the sign of the corresponding
                // delta (see the "Mouse" checkboxes in the control panel).
                view.yaw += dx * (view.invertH ? -1 : 1) * 0.005 * fine;
                view.pitch = clamp(view.pitch + dy * (view.invertV ? -1 : 1) * 0.005 * fine, -1.4, 1.4);
            }
            streaming.onMove();   // refresh the streamed window (no-op when small)
        });
        container.addEventListener('wheel', e => {
            e.preventDefault();
            focus.active = false;
            if (e.ctrlKey) {
                // Ctrl+wheel zooms the content (source pane, diagram, call-graph)
                view.contentZoom = clamp(view.contentZoom * (1 - e.deltaY * 0.001), 0.1, 5);
                document.documentElement.style.setProperty('--cz', view.contentZoom);
            } else {
                // Regular wheel zooms the camera
                view.dist = clamp(view.dist * (1 + e.deltaY * 0.001), MIN_DIST, MAX_DIST);
            }
            streaming.onMove();   // zoom changes what's near the target
        }, { passive: false });

        // --- Touch / mobile controls -------------------------------------
        // On a coarse pointer (phone, tablet) there is no wheel and no
        // modifier keys, so the mouse model above does not apply. Detect it
        // once and switch to the native touch model:
        //   one finger        -> orbit  (same yaw/pitch maths as a left drag)
        //   two-finger pinch  -> zoom   (camera dist)
        //   two-finger drag   -> pan    (slide the target, like shift+drag)
        //   button pad        -> move / zoom / next / prev / center
        // PC browsers keep the mouse model; the pad stays hidden.
        const touchMode = (navigator.maxTouchPoints || 0) > 0 ||
            (window.matchMedia && matchMedia('(pointer: coarse)').matches);
        if (touchMode) {
            document.body.classList.add('touch');
            const hint = document.getElementById('hint');
            if (hint) hint.textContent =
                'one finger: orbit · two-finger pinch: zoom · ' +
                'two-finger drag: pan · double-tap a class to focus · ' +
                'long press: context menu · ' +
                'bottom pad: move, zoom, next/prev, center';
        }

        let touchPrev = null;    // {x, y, d} reference point of the last frame
        let longPressTimer = null;
        let longPressTriggered = false;

        container.addEventListener('touchstart', e => {
            if (e.touches.length === 0) return;
            autoRotate = false;
            focus.active = false;
            const t = e.touches;
            if (t.length === 1) {
                touchPrev = { x: t[0].clientX, y: t[0].clientY, d: null };
                // Set up long press timer
                longPressTimer = setTimeout(() => {
                    if (!longPressTriggered && touchPrev) {
                        longPressTriggered = true;
                        openCtxMenu(t[0].clientX, t[0].clientY, null);
                    }
                }, 500); // 500ms long press
            } else {
                touchPrev = {
                    x: (t[0].clientX + t[1].clientX) / 2,
                    y: (t[0].clientY + t[1].clientY) / 2,
                    d: Math.hypot(t[0].clientX - t[1].clientX,
                                 t[0].clientY - t[1].clientY)
                };
                // Clear long press timer when multi-touch starts
                if (longPressTimer) {
                    clearTimeout(longPressTimer);
                    longPressTimer = null;
                    longPressTriggered = false;
                }
            }
        }, { passive: true });

        container.addEventListener('touchmove', e => {
            if (!touchPrev) return;
            e.preventDefault();     // keep the page from scrolling / zooming
            const t = e.touches;

            // Cancel long press if finger moves significantly
            if (t.length === 1 && longPressTimer && !longPressTriggered) {
                const dx = Math.abs(t[0].clientX - touchPrev.x);
                const dy = Math.abs(t[0].clientY - touchPrev.y);
                if (dx > 10 || dy > 10) {
                    clearTimeout(longPressTimer);
                    longPressTimer = null;
                    longPressTriggered = false;
                }
            }

            if (t.length === 1) {
                // Finger went back to one after a two-finger gesture:
                // re-anchor instead of dragging from the old centroid.
                if (touchPrev.d !== null) {
                    touchPrev = { x: t[0].clientX, y: t[0].clientY, d: null };
                    return;
                }
                const dx = t[0].clientX - touchPrev.x,
                      dy = t[0].clientY - touchPrev.y;
                touchPrev.x = t[0].clientX; touchPrev.y = t[0].clientY;
                view.yaw += dx * (view.invertH ? -1 : 1) * 0.005;
                view.pitch = clamp(view.pitch + dy * (view.invertV ? -1 : 1) * 0.005,
                                   -1.4, 1.4);
            } else if (t.length === 2) {
                const cx = (t[0].clientX + t[1].clientX) / 2,
                      cy = (t[0].clientY + t[1].clientY) / 2,
                      d  = Math.hypot(t[0].clientX - t[1].clientX,
                                     t[0].clientY - t[1].clientY);
                if (touchPrev.d !== null && d > 0) {
                    view.dist = clamp(view.dist * (touchPrev.d / d),
                                      MIN_DIST, MAX_DIST);
                }
                if (touchPrev.d === null) {
                    // First two-finger frame: anchor, no delta yet.
                    touchPrev.x = cx; touchPrev.y = cy; touchPrev.d = d;
                    streaming.onMove();
                    return;
                }
                const dx = cx - touchPrev.x, dy = cy - touchPrev.y;
                const s = worldPerPixel();
                const cp = Math.cos(view.pitch), sp = Math.sin(view.pitch);
                const cyw = Math.cos(view.yaw),  syw = Math.sin(view.yaw);
                const rx = cyw,    ry = 0,   rz = -syw;        // camera right
                const ux = -sp*syw, uy = cp, uz = -sp*cyw;     // camera up
                view.target.x += (-dx * rx + dy * ux) * s;
                view.target.y += (-dx * ry + dy * uy) * s;
                view.target.z += (-dx * rz + dy * uz) * s;
                touchPrev.x = cx; touchPrev.y = cy; touchPrev.d = d;
            }
            streaming.onMove();   // refresh the streamed window (no-op when small)
        }, { passive: false });

        const touchEnd = e => {
            if (e.touches.length === 0) {
                touchPrev = null;
                // Clear long press timer on touch end
                if (longPressTimer) {
                    clearTimeout(longPressTimer);
                    longPressTimer = null;
                    longPressTriggered = false;
                }
            }
        };
        container.addEventListener('touchend', touchEnd);
        container.addEventListener('touchcancel', touchEnd);

        // On-screen button pad (visible only on touch devices): the four
        // arrows hold-to-move through the same heldKeys / moveView() path as
        // the w / a / s / d keys; the rest are one-shot actions.
        const pad = document.getElementById('touchpad');
        if (pad) {
            const hold = { up: 'w', down: 's', left: 'a', right: 'd' };
            const zoomStep = () => streaming.onMove();
            const oneShot = {
                zoomin:  () => { view.dist = clamp(view.dist / 1.25, MIN_DIST, MAX_DIST); zoomStep(); },
                zoomout: () => { view.dist = clamp(view.dist * 1.25, MIN_DIST, MAX_DIST); zoomStep(); },
                center:  () => navCenter(),
                prev:    () => navTo(-1),
                next:    () => navTo(1)
            };
            pad.querySelectorAll('button').forEach(btn => {
                const act = btn.dataset.act;
                if (hold[act]) {
                    btn.addEventListener('pointerdown', e => {
                        e.preventDefault();
                        heldKeys.add(hold[act]);
                    });
                    const release = () => heldKeys.delete(hold[act]);
                    btn.addEventListener('pointerup', release);
                    btn.addEventListener('pointercancel', release);
                    btn.addEventListener('pointerleave', release);
                } else if (oneShot[act]) {
                    btn.addEventListener('click', () => oneShot[act]());
                }
            });
        }

        // Double-click a class to point the camera at it; a double-click on
        // a member row jumps to the class named by that member's type
        // (e.g. a "Mutability" field flies to the Mutability class); a
        // double-click on an inheritance line jumps to its far end
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
            // Only pick among drawn classes — LOD culls the rest
            const hits = ray.intersectObjects(nodeList.filter(n => n.mesh.visible).map(n => n.mesh));
            if (hits.length) {
                // A streaming box is a child of a placeholder holder; resolve it
                // via userData.node, falling back to a direct-mesh match (eager).
                const hitObj = hits[0].object;
                const n = (hitObj.userData && hitObj.userData.node) ||
                          nodeList.find(nd => nd.mesh === hitObj);
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
                return;
            }
            // No class under the cursor: pick the inheritance line there and
            // jump to the end farthest from the camera ("the other end")
            ray.params.Line.threshold = 1.5;
            const edgeHits = ray.intersectObjects(
                edgeObjs.filter(x => x.line.visible).map(x => x.line));
            if (!edgeHits.length) return;
            const edge = edgeObjs.find(x => x.line === edgeHits[0].object);
            const a = nodes.get(edge.from), b = nodes.get(edge.to);
            const p = camera.position;
            const da = (a.x - p.x) ** 2 + (a.y - p.y) ** 2 + (a.z - p.z) ** 2;
            const db = (b.x - p.x) ** 2 + (b.y - p.y) ** 2 + (b.z - p.z) ** 2;
            focusOn(da > db ? a : b);
        });

        // Bring yaw onto [-pi, pi] so the focus animation takes the short way
        function wrapYaw() {
            view.yaw = ((view.yaw + Math.PI) % (2 * Math.PI) + 2 * Math.PI)
                       % (2 * Math.PI) - Math.PI;
        }

        function focusOnImpl(n) {
            autoRotate = false;
            pinned.clear();
            pinned.add(n);
            // Keep its parent drawn too, so the inheritance arrow stays in view
            if (n.parent) { const p = nodes.get(n.parent); if (p) pinned.add(p); }
            wrapYaw();
            // Head-on view: the class front faces the camera, centered
            focus.yaw = 0;
            focus.pitch = 0;
            focus.dist = Math.max(8, Math.max(n.w, n.h) * 3);
            focus.tx = n.x; focus.ty = n.y; focus.tz = n.z;
            focus.active = true;
        }

        function resetViewImpl() {
            pinned.clear();
            wrapYaw();
            Object.assign(focus, homeView());
            focus.active = true;
        }

        // w / a / s / d: slide the look-at target along the camera's forward
        // and right axes; the step scales with the zoom distance so the speed
        // feels the same far and near
        function moveView() {
            const step = view.dist * 0.02;
            const cp = Math.cos(view.pitch), sp = Math.sin(view.pitch);
            const cy = Math.cos(view.yaw), sy = Math.sin(view.yaw);
            const fx = -cp * sy, fy = -sp, fz = -cp * cy;   // camera forward
            const rx = cy,      ry = 0,    rz = -sy;        // camera right
            const fwd = (heldKeys.has('w') ? 1 : 0) - (heldKeys.has('s') ? 1 : 0);
            const side = (heldKeys.has('d') ? 1 : 0) - (heldKeys.has('a') ? 1 : 0);
            view.target.x += (fx * fwd + rx * side) * step;
            view.target.y += (fy * fwd + ry * side) * step;
            view.target.z += (fz * fwd + rz * side) * step;
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
            if (heldKeys.size) {
                autoRotate = false;
                focus.active = false;
                moveView();
            } else if (autoRotate) view.yaw += 0.002;
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
            if (lodMoved()) applyLod();
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

        // --- Call graph view ---
        let callGraphVisible = false;
        let callGraphData = null;

        function toggleCallGraph() {
            const callgraph = document.getElementById('callgraph');
            if (callGraphVisible) {
                callgraph.classList.add('hidden');
                callGraphVisible = false;
            } else {
                callgraph.classList.remove('hidden');
                renderCallGraph();
                callGraphVisible = true;
            }
        }

        function renderCallGraph() {
            const status = document.getElementById('callgraph-status');

            // For now, we'll show a simple placeholder since we don't have real data
            // In a real implementation, this would fetch from the server and render properly
            if (!callGraphData) {
                status.innerHTML = 'Call graph data loading...';
                // Simulate fetching data
                setTimeout(() => {
                    callGraphData = { methods: [] }; // Empty for now
                    drawCallGraph();
                }, 100);
            } else {
                drawCallGraph();
            }
        }

        function drawCallGraph() {
            const inner = document.getElementById('callgraph-inner');
            const status = document.getElementById('callgraph-status');

            if (!callGraphData) {
                status.innerHTML = '<span class="amb">No call graph data available</span>';
                return;
            }

            // Clear previous content
            inner.innerHTML = '';

            // Create a basic visualization showing the concept
            const width = 1000;
            const height = 800;
            const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
            svg.setAttribute('width', width);
            svg.setAttribute('height', height);
            svg.setAttribute('viewBox', `0 0 ${width} ${height}`);

            // Add basic styling
            const style = document.createElementNS('http://www.w3.org/2000/svg', 'style');
            style.textContent = `
                .cgnode rect { fill: var(--panel); stroke: var(--border-strong); }
                .cgedge { fill: none; stroke: var(--muted); stroke-width: 1.25; }
                .cgedge.amb { stroke: #b06000; stroke-dasharray: 4 3; }
                .cgtext { font-family: sans-serif; font-size: 12px; fill: var(--text); }
            `;
            svg.appendChild(style);

            // Create a placeholder showing the concept
            const centerX = width / 2;
            const centerY = height / 2;

            // Draw a central node to represent "whole project"
            const centerNode = document.createElementNS('http://www.w3.org/2000/svg', 'g');
            const rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
            rect.setAttribute('x', centerX - 100);
            rect.setAttribute('y', centerY - 25);
            rect.setAttribute('width', 200);
            rect.setAttribute('height', 50);
            rect.setAttribute('rx', 8);
            rect.setAttribute('class', 'cgnode');

            const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
            text.setAttribute('x', centerX);
            text.setAttribute('y', centerY + 5);
            text.setAttribute('text-anchor', 'middle');
            text.setAttribute('class', 'cgtext');
            text.textContent = 'Whole Project Call Graph';

            centerNode.appendChild(rect);
            centerNode.appendChild(text);
            svg.appendChild(centerNode);

            // Draw some sample method connections
            const methods = ['main()', 'processData()', 'parseInput()', 'validateOutput()'];
            const methodNodes = [];

            methods.forEach((method, i) => {
                const angle = (i / methods.length) * Math.PI * 2;
                const radius = 200;
                const x = centerX + Math.cos(angle) * radius;
                const y = centerY + Math.sin(angle) * radius;

                // Draw method node
                const node = document.createElementNS('http://www.w3.org/2000/svg', 'g');
                const rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
                rect.setAttribute('x', x - 80);
                rect.setAttribute('y', y - 15);
                rect.setAttribute('width', 160);
                rect.setAttribute('height', 30);
                rect.setAttribute('rx', 4);
                rect.setAttribute('class', 'cgnode');

                const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
                text.setAttribute('x', x);
                text.setAttribute('y', y + 5);
                text.setAttribute('text-anchor', 'middle');
                text.setAttribute('class', 'cgtext');
                text.textContent = method;

                node.appendChild(rect);
                node.appendChild(text);
                svg.appendChild(node);
                methodNodes.push({ x, y, name: method });
            });

            // Draw connections from center to methods
            methodNodes.forEach(node => {
                const edge = document.createElementNS('http://www.w3.org/2000/svg', 'line');
                edge.setAttribute('x1', centerX);
                edge.setAttribute('y1', centerY);
                edge.setAttribute('x2', node.x);
                edge.setAttribute('y2', node.y);
                edge.setAttribute('class', 'cgedge');

                svg.appendChild(edge);
            });

            inner.appendChild(svg);
            status.innerHTML = `Call graph showing sample method relationships`;
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
            if (currentMode === 'cmake') {
                const t = (CMAKE_TARGETS || []).filter(x => x && x.name).length;
                document.getElementById('stats').textContent
                    = t + ' CMake target' + (t === 1 ? '' : 's') + ' shown';
                return;
            }

            // Count actual classes that are visible in the 3D scene
            const shown = [...nodes.values()].filter(n => !n.cmake && n.mesh.visible).length;

            // If in namespace mode, also show per-namespace counts
            if (currentMode === 'namespace') {
                const namespaceCounts = new Map();
                for (const n of nodes.values()) {
                    if (!n.cmake && n.mesh.visible) {
                        const key = groupKey(n);
                        namespaceCounts.set(key, (namespaceCounts.get(key) || 0) + 1);
                    }
                }

                let statsText = shown + ' of ' + classes.length + ' classes shown';
                if (namespaceCounts.size > 0) {
                    statsText += ' - Namespaces: ';
                    const counts = [];
                    for (const [ns, count] of namespaceCounts.entries()) {
                        counts.push(`${ns || '(global)'}: ${count}`);
                    }
                    statsText += counts.join(', ');
                }

                document.getElementById('stats').textContent = statsText;
            } else {
                document.getElementById('stats').textContent
                    = shown + ' of ' + classes.length + ' classes shown';
            }
        }

        // --- Visibility: apply the diff, name and namespace filters together
        // --- (classes are hidden in both the 3D scene and the panel), then
        // --- narrow the drawn set to the nearest lodLimit via applyLod()
        function applyVisibility() {
            // CMake targets live in a separate world: in the cmake layout
            // they are the diagram (and the class boxes + class sidebar are
            // not), and in every other layout they are absent.
            const inCmakeMode = currentMode === 'cmake';
            const realVisible = new Map();
            nodes.forEach(n => {
                if (n.cmake !== inCmakeMode) return;
                if (!n.external) realVisible.set(n.name, classVisible(n.name));
            });

            // For namespace grouping, we want to ensure that when a class is visible,
            // all related classes (inheritance chain) are also visible
            const enhancedVisibility = new Map();
            nodes.forEach(n => {
                if (n.cmake !== inCmakeMode) return;
                if (!n.external) {
                    const isVisible = realVisible.get(n.name);
                    enhancedVisibility.set(n.name, isVisible);
                    if (isVisible) {
                        // If this class is visible, make sure its inheritance chain is also visible
                        const classObj = n.cls;
                        if (classObj && classObj.inheritance) {
                            classObj.inheritance.forEach(baseName => {
                                const baseNode = nodes.get(baseName);
                                if (baseNode && !enhancedVisibility.get(baseName)) {
                                    enhancedVisibility.set(baseName, true);
                                }
                            });
                        }
                    }
                } else {
                    // Unresolved base (or alias stub): shown while any of its
                    // dependents/children is shown
                    const isVisible = edgeObjs.some(e => e.to === n.name && realVisible.get(e.from));
                    enhancedVisibility.set(n.name, isVisible);
                }
            });

            lodPool = [];
            nodes.forEach(n => {
                if (n.cmake !== inCmakeMode) return;
                let visible;
                if (!n.external) {
                    visible = enhancedVisibility.get(n.name);
                } else {
                    // Unresolved base (or alias stub): shown while any of its
                    // dependents/children is shown
                    visible = edgeObjs.some(e => e.to === n.name && enhancedVisibility.get(e.from));
                }
                if (visible) lodPool.push(n);
            });
            applyLod();
            const hideCards = inCmakeMode;
            document.querySelectorAll('.classCard').forEach(card => {
                card.style.display = (hideCards || !classVisible(card.dataset.name)) ? 'none' : '';
            });
            // Hide a namespace group whose classes are all hidden
            document.querySelectorAll('.nsGroup').forEach(g => {
                const anyShown = !hideCards && [...g.querySelectorAll('.classCard')]
                    .some(card => card.style.display !== 'none');
                g.style.display = anyShown ? '' : 'none';
            });
            updateStats();
        }

        function applyFilter() {
            const nameVal = document.getElementById('filterInput').value.trim();
            const nsVal = document.getElementById('nsFilterInput').value.trim();
            let nameRe = null, nsRe = null;
            if (nameVal) {
                try { nameRe = new RegExp(nameVal); }
                catch (err) { alert('Invalid name regex: ' + err.message); return; }
            }
            if (nsVal) {
                try { nsRe = new RegExp(nsVal); }
                catch (err) { alert('Invalid namespace regex: ' + err.message); return; }
            }
            currentRegex = nameRe;
            currentNsRegex = nsRe;
            applyVisibility();
        }

        function resetFilter() {
            document.getElementById('filterInput').value = '';
            document.getElementById('nsFilterInput').value = '';
            currentRegex = null;
            currentNsRegex = null;
            applyVisibility();
        }

        buildSidebar();

        // --- Keyboard navigation: next / previous / center ---
        // n or → steps to the next class, p or ← to the previous one — both
        // wrap around the classes currently shown in the sidebar — and c (or
        // Home) returns to the home view, the same destination as the
        // "Reset View" button. w / a / s / d slide the view along the camera's
        // forward / right axes. Keys are ignored while typing in a field (e.g.
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
            // Ctrl-Z / Cmd-Z: step backwards through the focus history. Handled
            // first (before the modifier early-return below) so it always fires.
            if ((e.ctrlKey || e.metaKey) && (e.key === 'z' || e.key === 'Z')) {
                e.preventDefault();
                histBack();
                return;
            }
            // Escape dismisses the review UI (context menu / comment form).
            // Handled before the tag check below: cancelling a comment draft
            // must work even while the textarea has focus.
            if (e.key === 'Escape') {
                closeCtxMenu();
                hideCommentForm();
                return;
            }
            if (e.ctrlKey || e.metaKey || e.altKey) return;
            const tag = (e.target && e.target.tagName) || '';
            if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return;
            const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
            if (e.key === 'n' || e.key === 'N' || e.key === 'ArrowRight') { e.preventDefault(); navTo(1); }
            else if (e.key === 'p' || e.key === 'P' || e.key === 'ArrowLeft') { e.preventDefault(); navTo(-1); }
            else if (e.key === 'c' || e.key === 'C' || e.key === 'Home') { e.preventDefault(); navCenter(); }
            else if (k === 'w' || k === 'a' || k === 's' || k === 'd') { e.preventDefault(); heldKeys.add(k); }
            else if (e.key === '[') { e.preventDefault(); stepFile(-1); }   // previous file
            else if (e.key === ']') { e.preventDefault(); stepFile(1); }    // next file
        });
        window.addEventListener('keyup', e => {
            const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
            heldKeys.delete(k);
        });
        window.addEventListener('blur', () => heldKeys.clear());

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
        // The CMake layout only exists when the analyzed directory produced
        // targets; reveal it then.
        if ((CMAKE_TARGETS || []).some(t => t && t.name)) {
            document.querySelector('#layoutSelect option[value="cmake"]').style.display = '';
        }
        applyLayout(layoutSelect.value);

        // Large projects: upgrade to streaming (no-op for small ones / offline).
        streaming.maybeEnable();

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

            // Review UI: make room for the left file list, build its entries
            // from the fileEntries computed up top, and re-measure the 3D scene
            // for the narrower viewport (three.js re-sizes on 'resize').
            document.body.classList.add('diffActive');
            const fileListBody = document.getElementById('file-list-body');
            fileEntries.forEach((e, i) => {
                const el = document.createElement('div');
                el.className = 'fileEntry';
                el.dataset.idx = String(i);
                const name = document.createElement('span');
                name.className = 'fileEntry-name';
                const shown = String(e.newPath || e.oldPath || (e.names && e.names[0]) || 'file');
                name.textContent = shown.split('/').pop() || shown;
                name.title = shown;
                const badge = document.createElement('span');
                badge.className = 'fileEntry-badge st-' + e.status;
                badge.textContent = e.status;
                el.appendChild(name);
                el.appendChild(badge);
                el.addEventListener('click', () => {
                    setActiveFile(e.oldPath, e.newPath);
                    showFileDiff(e.oldPath, e.newPath);
                });
                fileListBody.appendChild(el);
            });
            // Inline display:none wins over the CSS, so reveal it in JS (the
            // #diffWrap pattern) after its content exists.
            document.getElementById('file-list').style.display = '';
            window.dispatchEvent(new Event('resize'));
        }

        // Minimize the controls panel to a compact bar, and back again
        const controls = document.getElementById('controls');
        const minimizeBtn = document.getElementById('minimizeBtn');
        minimizeBtn.addEventListener('click', () => {
            const minimized = controls.classList.toggle('minimized');
            minimizeBtn.innerHTML = minimized ? '+' : '&ndash;';
            minimizeBtn.title = minimized ? 'Expand panel' : 'Minimize panel';
        });

        // ------------------------------------------------------------------
        // Source pane
        // Shows the focused class and the source files that implement it, as
        // tabs. Source is fetched from the server on demand (GET /source?
        // path=...); a page opened from a file:// URL cannot fetch, so those
        // tabs fall back to a "source not available" placeholder.
        // ------------------------------------------------------------------
        const paneEl = {
            title:  document.getElementById('pane-title'),
            vars:   document.getElementById('pane-vars'),
            tabs:   document.getElementById('pane-tabs'),
            code:   document.getElementById('pane-code'),
        };
        let activePath = null;   // the tab whose fetch is in flight / shown
        let curFiles = [];       // tab order for the focused class
        let ignoreWhitespace = false;  // diff lines compared modulo whitespace

        // --- Diff-mode render state (single-file view) ---
        // diffSeq is a monotonic render token: every showFileDiff bumps it and a
        // fetch that resolves after a newer request has started simply compares
        // its captured seq against diffSeq and drops itself (replaces the old
        // lastFocus node-guard, which couldn't track resync re-renders).
        let diffSeq = 0;
        let curOldPath = null;   // the two revisions currently shown in the pane
        let curNewPath = null;
        let curAnchors = [];     // pinned resync rows: [{o, n, row}] (see lineDiff)
        let curOldLines = null;  // fetched source split into lines (or null)
        let curNewLines = null;
        let allComments = [];    // review comments loaded from GET /comments

        // --- Focus history: newest last, capped at 30; Ctrl-Z steps back ---
        const hist = [];
        function pushHistory(n) {
            hist.push(n);
            if (hist.length > 30) hist.shift();
        }
        // Public focusOn: record the target, focus it, refresh the pane. All
        // existing callers (double-click, navigation, sidebar) route through
        // this, so every focus is a history entry Ctrl-Z can walk back over.
        function focusOn(n) {
            pushHistory(n);
            focusOnImpl(n);
            updatePane(n);
        }
        // Step back one entry (never past the oldest), re-focus it, refresh.
        function histBack() {
            if (hist.length > 1) {
                hist.pop();
                const n = hist[hist.length - 1];
                focusOnImpl(n);
                updatePane(n);
            }
        }
        // Camera reset (button / "c" key) — leaves history and pane alone.
        function resetView() { resetViewImpl(); }

        // --- Tabs: own file first, then related classes' files, cap 8 -----
        // Candidate files are the focused class's own file, its base classes'
        // files, and the files of classes named by member/parameter/return
        // types. Each is counted by how many references point at it, so the
        // most-central files surface first.
        function relatedFiles(n) {
            const c = n.cls;
            const count = new Map();
            const add = p => { if (p) count.set(p, (count.get(p) || 0) + 1); };
            add(c.file);
            for (const b of (c.inheritance || [])) {
                const base = resolveBase(b);
                if (base && base !== c) add(base.file);
            }
            const refFile = t => {
                const id = classInType(t);
                if (!id) return;
                const nd = nodes.get(id);
                if (nd && nd.cls && nd.cls !== c) add(nd.cls.file);
            };
            for (const v of (c.variables || [])) refFile(v.type);
            for (const m of (c.methods || [])) {
                refFile(m.return_type);
                for (const p of (m.parameters || [])) refFile(p);
            }
            const own = c.file;
            return [...count.entries()]
                .filter(e => e[0])
                .sort((a, b) => {
                    const ao = (a[0] === own) ? 0 : 1;
                    const bo = (b[0] === own) ? 0 : 1;
                    if (ao !== bo) return ao - bo;   // own file always first
                    return b[1] - a[1];              // then most-referenced
                })
                .slice(0, 8)
                .map(e => e[0]);
        }

        // --- Highlighter: no CDN, colors from the theme's --syn-* tokens ---
        // C++/C#/Python keyword union; leading-uppercase identifiers read as
        // types; comment/string/number spans for the rest.
        const SYN_KEYWORDS = new Set(('abstract as auto bool break case catch char class const constexpr '
            + 'continue default def delete do double elif else enum except export extends extern '
            + 'final finally float for friend from function global if import inline int '
            + 'interface internal lambda let long match mutable namespace new noexcept '
            + 'nullptr operator override package pass private protected public readonly '
            + 'record return sealed short signed sizeof static struct string switch '
            + 'template this throw throws try typedef typename type union unsigned using '
            + 'var virtual void volatile where while with yield').split(' '));
        const SYN_RE = /(\/\/[^\n]*|\/\*[\s\S]*?\*\/|#[^\n]*)|("(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')|([A-Za-z_][A-Za-z0-9_]*)|(\d[\dxXa-fA-F'_.eE]*)/g;
        function highlight(code) {
            if (typeof code !== 'string') code = String(code);
            if (!code) return '';
            // Size guard: tokenizing a very large file would freeze the pane;
            // render the (escaped) plain text instead.
            if (code.length > 300000) return esc(code);
            let out = '';
            let last = 0;
            let m;
            SYN_RE.lastIndex = 0;
            while ((m = SYN_RE.exec(code)) !== null) {
                out += esc(code.slice(last, m.index));
                last = SYN_RE.lastIndex;
                if (m[1] !== undefined) out += '<span class="syn-comment">' + esc(m[1]) + '</span>';
                else if (m[2] !== undefined) out += '<span class="syn-string">' + esc(m[2]) + '</span>';
                else if (m[3] !== undefined) {
                    if (SYN_KEYWORDS.has(m[3])) out += '<span class="syn-keyword">' + esc(m[3]) + '</span>';
                    else {
                        // A name known to the model: a clickable link that
                        // focuses that class (delegated handler on #pane-code).
                        const target = classInType(m[3]);
                        if (target) {
                            const tn = nodes.get(target);
                            out += '<a class="syn-type syn-link" data-target="' + esc(target) +
                                   '" title="Focus ' + esc(tn.cls ? displayName(tn.cls) : target) + '">'
                                   + esc(m[3]) + '</a>';
                        }
                        else if (/^[A-Z]/.test(m[3])) out += '<span class="syn-type">' + esc(m[3]) + '</span>';
                        else out += esc(m[3]);
                    }
                } else if (m[4] !== undefined) out += '<span class="syn-number">' + esc(m[4]) + '</span>';
            }
            out += esc(code.slice(last));
            return out;
        }

        // --- Renderable non-code files ------------------------------------
        // Extension tests drive openTab / renderLine: markdown renders to
        // HTML, diagrams render server-side via /render (dot / drawio).
        function isMd(path) {
            const p = String(path || '');
            return p.endsWith('.md') || p.endsWith('.markdown');
        }
        function isDiagram(path) {
            const p = String(path || '').toLowerCase();
            return p.endsWith('.dot') || p.endsWith('.drawio') || p.endsWith('.draw.io');
        }
        // Per-path preference from the context menu: 'auto' (the extension
        // decides) is the default; 'source' / 'rendered' force a side.
        const renderModes = new Map();
        const renderMode = p => renderModes.get(p) || 'auto';

        // --- Inline markdown: escape-first, so it is XSS-safe -------------
        // `code`, **bold**, *em*, [text](url). The URL scheme allowlist keeps
        // javascript: links dead; esc() already neutralized any quotes.
        function mdInline(text) {
            let s = esc(text);
            s = s.replace(/`([^`]+)`/g, '<code class="md-code">$1</code>');
            s = s.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
            s = s.replace(/(^|[\s(])\*([^*\n]+)\*/g, '$1<em>$2</em>');
            s = s.replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, (m, label, url) =>
                /^(https?:|\/|\.)/i.test(url)
                    ? '<a class="md-link" href="' + url + '" target="_blank" rel="noopener">'
                      + label + '</a>'
                    : label);
            return s;
        }

        // Block-level markdown -> HTML. Compact on purpose: headings, fenced
        // code, lists, blockquotes, hr, paragraphs; inline markup via
        // mdInline. Every raw line passes through esc(), so the output is
        // safe to innerHTML.
        function mdToHtml(md) {
            if (typeof md !== 'string') md = String(md);
            if (!md.trim()) return '';
            if (md.length > 300000) return '<pre class="md-pre">' + esc(md) + '</pre>';
            const lines = md.split(/\r?\n/);
            const out = [];
            const para = [];
            const flush = () => {
                if (para.length) { out.push('<p>' + mdInline(para.join(' ')) + '</p>'); para.length = 0; }
            };
            let i = 0;
            while (i < lines.length) {
                const line = lines[i];
                let m;
                if (/^\s*$/.test(line)) { flush(); i++; continue; }
                if ((m = line.match(/^(#{1,6})\s+(.*)$/))) {
                    flush();
                    const lvl = m[1].length;
                    out.push('<h' + lvl + '>' + mdInline(m[2]) + '</h' + lvl + '>');
                    i++; continue;
                }
                if (/^\s*(```|~~~)/.test(line)) {
                    flush();
                    const buf = [];
                    i++;
                    while (i < lines.length && !/^\s*(```|~~~)/.test(lines[i])) { buf.push(lines[i]); i++; }
                    i++;   // skip the closing fence (or run to EOF)
                    out.push('<pre class="md-pre"><code>' + esc(buf.join('\n')) + '</code></pre>');
                    continue;
                }
                if (/^\s*([-*_])\1{2,}\s*$/.test(line)) { flush(); out.push('<hr/>'); i++; continue; }
                if (/^\s*>/.test(line)) {
                    flush();
                    const buf = [];
                    while (i < lines.length && /^\s*>/.test(lines[i])) {
                        buf.push(lines[i].replace(/^\s*>\s?/, '')); i++;
                    }
                    out.push('<blockquote>' + mdToHtml(buf.join('\n')) + '</blockquote>');
                    continue;
                }
                if (/^\s*[-*+]\s+/.test(line)) {
                    flush();
                    const buf = [];
                    while (i < lines.length && /^\s*[-*+]\s+/.test(lines[i])) {
                        buf.push(lines[i].replace(/^\s*[-*+]\s+/, '')); i++;
                    }
                    out.push('<ul>' + buf.map(li => '<li>' + mdInline(li) + '</li>').join('') + '</ul>');
                    continue;
                }
                if (/^\s*\d+[.)]\s+/.test(line)) {
                    flush();
                    const buf = [];
                    while (i < lines.length && /^\s*\d+[.)]\s+/.test(lines[i])) {
                        buf.push(lines[i].replace(/^\s*\d+[.)]\s+/, '')); i++;
                    }
                    out.push('<ol>' + buf.map(li => '<li>' + mdInline(li) + '</li>').join('') + '</ol>');
                    continue;
                }
                para.push(line);
                i++;
            }
            flush();
            return out.join('\n');
        }

        // --- Fetch + render a tab -----------------------------------------
        function setPlaceholder(msg) {
            paneEl.code.innerHTML = '<span class="syn-placeholder">' + esc(msg) + '</span>';
        }
        // Render one fetched file by preference: markdown -> HTML, diagrams
        // -> the server-rendered SVG (source fallback if the renderer is
        // missing), everything else the syntax highlighter.
        function renderTab(path, text) {
            if (isMd(path) && renderMode(path) !== 'source') {
                paneEl.code.innerHTML = '<div class="md-body">' + mdToHtml(text) + '</div>';
                return;
            }
            if (isDiagram(path) && renderMode(path) !== 'source') {
                renderDiagram(path, text);
                return;
            }
            paneEl.code.innerHTML = highlight(text);
        }
        // Diagrams render on the server (offline shell-out to dot / drawio).
        // Any failure — binary missing (503), bad body, empty SVG — falls
        // back to the highlighted source under an explanatory banner.
        function renderDiagram(path, text) {
            fetch('/render?path=' + encodeURIComponent(path))
                .then(r => { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
                .then(svg => {
                    if (activePath !== path) return;   // superseded by a newer tab
                    paneEl.code.innerHTML = '<div class="diagram-wrap">' + svg + '</div>';
                })
                .catch(() => {
                    if (activePath !== path) return;
                    paneEl.code.innerHTML =
                        '<div class="render-fallback">renderer unavailable — showing source</div>'
                        + highlight(text);
                });
        }
        function openTab(path) {
            activePath = path;
            const idx = curFiles.indexOf(path);
            [...paneEl.tabs.children].forEach((t, i) => t.classList.toggle('active', i === idx));
            paneEl.code.textContent = 'Loading ' + path + ' …';
            fetch('/source?path=' + encodeURIComponent(path))
                .then(r => { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
                .then(text => {
                    if (activePath !== path) return;   // superseded by a newer tab
                    renderTab(path, text);
                })
                .catch(() => {
                    if (activePath !== path) return;
                    setPlaceholder('source not available for ' + path);
                });
        }

        // --- Diff-mode source: OLD | NEW side by side, changes highlighted --
        // LCS alignment of two already-normalized line arrays -> ops list. Each
        // op is [oldIdx|null, newIdx|null]: same = [i,j], removed = [i,null],
        // added = [null,j]. LINE_DIFF_CELLS caps the O(n·m) table; over budget we
        // trim the common prefix/suffix and mark the whole middle changed
        // (coarse, but safe). Shared by the plain diff and each resync segment.
        const LINE_DIFF_CELLS = 4000000;  // budget for the O(n·m) LCS table
        // oBase/nBase shift the emitted indices to absolute positions. a/b are
        // either the full arrays (plain diff, base 0) or a segment slice that
        // lineDiff passes in; lineDiff supplies the slice's starting index so the
        // ops come out on the original coordinate system.
        function lcsOps(a, b, oBase, nBase) {
            const ob = oBase | 0, nb = nBase | 0;
            const n = a.length, m = b.length;
            const ops = [];
            if (n * m <= LINE_DIFF_CELLS) {
                // LCS, bottom-up; Uint32 keeps the table small
                const W = m + 1;
                const dp = new Uint32Array((n + 1) * W);
                for (let i = n - 1; i >= 0; i--)
                    for (let j = m - 1; j >= 0; j--)
                        dp[i * W + j] = a[i] === b[j]
                            ? dp[(i + 1) * W + j + 1] + 1
                            : Math.max(dp[(i + 1) * W + j], dp[i * W + j + 1]);
                let i = 0, j = 0;
                while (i < n && j < m) {
                    if (a[i] === b[j]) { ops.push([i + ob, j + nb]); i++; j++; }
                    else if (dp[(i + 1) * W + j] >= dp[i * W + j + 1]) { ops.push([i + ob, null]); i++; }
                    else { ops.push([null, j + nb]); j++; }
                }
                while (i < n) { ops.push([i + ob, null]); i++; }
                while (j < m) { ops.push([null, j + nb]); j++; }
            } else {
                // Too big for the table: trim the common prefix/suffix and
                // mark the whole middle as changed (coarse, but safe)
                let pre = 0;
                while (pre < n && pre < m && a[pre] === b[pre]) pre++;
                let suf = 0;
                while (suf < n - pre && suf < m - pre &&
                       a[n - 1 - suf] === b[m - 1 - suf]) suf++;
                for (let k = 0; k < pre; k++) ops.push([k + ob, k + nb]);
                for (let k = pre; k < n - suf; k++) ops.push([k + ob, null]);
                for (let k = pre; k < m - suf; k++) ops.push([null, k + nb]);
                for (let k = 0; k < suf; k++) ops.push([n - 1 - k + ob, m - 1 - k + nb]);
            }
            return ops;
        }

        // lineDiff: LCS alignment of two line arrays -> ops list (same op shape
        // as lcsOps). With ignoreWs on, lines are compared with all whitespace
        // stripped (like `git diff -w`) — `a = 1` and `a=1` are the same line —
        // while the original text is still rendered.
        //
        // `anchors` (default []) are resync pins: each is {op, np, row}, where
        // op/np are the counts of old/new lines ABOVE that row and row is one of
        // 'dl-same' | 'dl-del' | 'dl-add'. Each pin forces its row to appear at
        // exactly that position and lets the LCS run independently on the segment
        // above and below it, so a single pinned line can override a bad global
        // alignment. Pins are sorted by position and de-duplicated defensively.
        function lineDiff(oldLines, newLines, ignoreWs, anchors) {
            const norm = s => ignoreWs ? s.replace(/\s/g, '') : s;
            const a = oldLines.map(norm), b = newLines.map(norm);
            const n = a.length, m = b.length;

            const pins = (anchors || [])
                .filter(p => Number.isFinite(p.op) && Number.isFinite(p.np)
                             && p.op >= 0 && p.op <= n && p.np >= 0 && p.np <= m)
                .sort((x, y) => (x.op - y.op) || (x.np - y.np));
            const seen = new Set();
            const uniq = [];
            for (const p of pins) {
                const k = p.op + ',' + p.np;
                if (!seen.has(k)) { seen.add(k); uniq.push(p); }
            }

            const ops = [];
            let o0 = 0, n0 = 0;   // running segment start (old index, new index)
            for (const p of uniq) {
                // Clamp any out-of-order pin to the running position (a no-op
                // segment) so a stale/malformed anchor can't corrupt the diff.
                const op = Math.max(o0, p.op);
                const np = Math.max(n0, p.np);
                ops.push(...lcsOps(a.slice(o0, op), b.slice(n0, np), o0, n0));
                // The pinned row consumes an old line unless it is an added row,
                // and a new line unless it is a removed row.
                const consumesOld = p.row !== 'dl-add';
                const consumesNew = p.row !== 'dl-del';
                ops.push([consumesOld ? op : null, consumesNew ? np : null]);
                o0 = op + (consumesOld ? 1 : 0);
                n0 = np + (consumesNew ? 1 : 0);
            }
            if (o0 < n || n0 < m) {
                ops.push(...lcsOps(a.slice(o0), b.slice(n0), o0, n0));
            }
            return ops;
        }

        // One aligned row: line number + highlighted source, so keywords and
        // types keep their colors and known type names stay clickable (the
        // delegated #pane-code handler covers these rows too). data-o/data-n are
        // the counts of old/new lines ABOVE this row; together they uniquely
        // identify the row, so resync anchors and comment markers can find it.
        function renderLine(line, status, ln, dataO, dataN, path) {
            // Markdown rows render through the same mdToHtml so bold/code/
            // lists keep meaning in the diff; the .dl envelope (and its
            // data-o/data-n stamps) is untouched, so resync, scroll anchors
            // and comment markers still find the row.
            const src = isMd(path) && renderMode(path) !== 'source'
                ? '<span class="dl-src md-row">' + (mdToHtml(line) || '&nbsp;') + '</span>'
                : '<span class="dl-src">' + highlight(line) + '</span>';
            return '<div class="dl ' + status + '" data-o="' + dataO
                + '" data-n="' + dataN + '"><span class="dl-ln">' + ln
                + '</span>' + src + '</div>';
        }

        // Build the two-column view from the alignment ops. `lines` may be
        // null for a class absent from that revision (added / removed). Every
        // row — gaps included — stamps the running prefix that identifies it.
        function diffColumns(oldLines, newLines, oldPath, newPath, anchors) {
            const ops = lineDiff(oldLines || [], newLines || [], ignoreWhitespace, anchors);
            // Precompute each row's identity stamp, once. data-o / data-n are the
            // 1-based old/new line number the row lands on (old/new lines
            // consumed above it, plus one). They depend only on the op sequence,
            // so both columns stamp the same row identically — which is what lets
            // a resync pin or a comment marker find the row by (data-o, data-n).
            const oStamp = [], nStamp = [];
            let oPref = 0, nPref = 0;
            for (const [oi, ni] of ops) {
                oStamp.push(oPref + 1);
                nStamp.push(nPref + 1);
                if (oi !== null) oPref++;
                if (ni !== null) nPref++;
            }
            const col = (lines, path, side) => {
                const head = path ? esc(path)
                    : '<span class="diff-col-none">' + esc(side === 'old'
                        ? 'added — no file in old revision'
                        : 'removed — no file in new revision') + '</span>';
                let body = '';
                for (let k = 0; k < ops.length; k++) {
                    const [oi, ni] = ops[k];
                    const o = oStamp[k], n = nStamp[k];
                    if (side === 'old') {
                        if (oi === null) body +=
                            '<div class="dl dl-gap" data-o="' + o + '" data-n="' + n + '"></div>';
                        else body += renderLine(lines[oi],
                            ni === null ? 'dl-del' : 'dl-same', oi + 1, o, n, path);
                    } else {
                        if (ni === null) body +=
                            '<div class="dl dl-gap" data-o="' + o + '" data-n="' + n + '"></div>';
                        else body += renderLine(lines[ni],
                            oi === null ? 'dl-add' : 'dl-same', ni + 1, o, n, path);
                    }
                }
                return '<div class="diff-col"><div class="diff-col-head">' + head + '</div>'
                    + body + '</div>';
            };
            return '<div class="diff-split">'
                + col(oldLines, oldPath, 'old')
                + col(newLines, newPath, 'new') + '</div>';
        }

        // Whether review comments have already been fetched for this page.
        // Shared across files (GET /comments is global), so load at most once.
        let commentsLoaded = false;

        // Render the current file pair with its pinned anchors, then (re)draw the
        // review-comment badges over the fresh rows. Called after any state
        // change: a fetch landing, a resync, or the ignore-whitespace toggle.
        function renderDiffNow() {
            if (isDiagram(curOldPath) || isDiagram(curNewPath)) {
                renderDiagramDiff();   // side-by-side renders; no line diff
                renderCommentMarkers();
                return;
            }
            paneEl.code.innerHTML = diffColumns(
                curOldLines, curNewLines, curOldPath, curNewPath, curAnchors);
            renderCommentMarkers();
        }

        // Diagram revisions can't be diffed line-for-line: show both sides'
        // rendered SVG (or their source when the renderer is absent) in the
        // usual two-column shell.
        function diagramDiffCol(path, side) {
            const head = path ? esc(path) : esc(side === 'old'
                ? 'added — no file in old revision'
                : 'removed — no file in new revision');
            return '<div class="diff-col"><div class="diff-col-head">' + head + '</div>'
                + '<div class="diagram-fill"></div></div>';
        }
        function renderDiagramDiff() {
            const linesOf = side =>
                (side === 'old' ? curOldLines : curNewLines) || null;
            const textOf = side => {
                const l = linesOf(side);
                return l ? l.join('\n') : null;
            };
            paneEl.code.innerHTML = '<div class="diff-split">'
                + diagramDiffCol(curOldPath, 'old')
                + diagramDiffCol(curNewPath, 'new') + '</div>';
            const cols = paneEl.code.querySelectorAll('.diagram-fill');
            const fill = (idx, path) => {
                const el = cols[idx];
                const text = textOf(idx === 0 ? 'old' : 'new');
                if (!el) return;
                if (!path) {
                    el.innerHTML = '<span class="diff-col-none">no file in this revision</span>';
                    return;
                }
                if (renderMode(path) === 'source' || text === null) {
                    el.innerHTML = text !== null
                        ? highlight(text)
                        : '<span class="syn-placeholder">source not available</span>';
                    return;
                }
                fetch('/render?path=' + encodeURIComponent(path))
                    .then(r => { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
                    .then(svg => { el.innerHTML = '<div class="diagram-wrap">' + svg + '</div>'; })
                    .catch(() => {
                        el.innerHTML = '<div class="render-fallback">renderer unavailable — showing source</div>'
                            + (text !== null ? highlight(text) : '');
                    });
            };
            fill(0, curOldPath);
            fill(1, curNewPath);
        }

        // Fetch both revisions of `oldPath`/`newPath` (where they exist) and
        // render the side-by-side diff. A monotonic seq guards against a slow
        // fetch resolving after a newer request (resync / file switch) has
        // started — the old lastFocus node-guard could not track re-renders.
        function showFileDiff(oldPath, newPath, opts) {
            const seq = ++diffSeq;
            curOldPath = oldPath;
            curNewPath = newPath;
            curAnchors = (opts && opts.anchors) ? opts.anchors.slice() : [];
            loadComments();
            paneEl.code.innerHTML = '<span class="syn-placeholder">loading old and new source…</span>';
            const load = p => p
                ? fetch('/source?path=' + encodeURIComponent(p))
                      .then(r => { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
                : Promise.resolve(null);
            Promise.all([load(oldPath), load(newPath)]).then(([oldText, newText]) => {
                if (seq !== diffSeq) return;   // superseded by a newer request
                curOldLines = oldText === null ? null : oldText.split(/\r?\n/);
                curNewLines = newText === null ? null : newText.split(/\r?\n/);
                renderDiffNow();
            }).catch(() => {
                if (seq !== diffSeq) return;
                setPlaceholder('source not available for this class');
            });
        }

        // Focus entry point (called from updatePane). Resolves the class's file
        // pair and hands off to showFileDiff; keeps the no-file / external-stub
        // branches. Anchors reset on a new focus (the "cleared on file switch"
        // rule — a stale pin from another file would corrupt the alignment).
        function showDiff(n) {
            paneEl.tabs.style.display = 'none';
            const c = n.cls;
            if (!c) { setPlaceholder('external class — no source file in this model'); return; }
            const key = (c.namespace || '') + '::' + c.name;
            const fileOf = list => {
                const r = list.find(o => (o.namespace || '') + '::' + o.name === key);
                return (r && r.file) ? r.file : null;
            };
            const oldPath = fileOf(oldMerged);
            const newPath = fileOf(newMerged);
            if (!oldPath && !newPath) { setPlaceholder('no source file recorded for this class'); return; }
            setActiveFile(oldPath, newPath);
            showFileDiff(oldPath, newPath);
        }

        // Highlight the file-list entry matching the current file pair (or clear
        // the highlight if none). The .fileEntry elements are built in the same
        // order as fileEntries, so the fileIndex value is their array position.
        function setActiveFile(oldPath, newPath) {
            if (!DIFF_MODE) return;
            const idx = fileIndex.get(fileKeyOf(oldPath, newPath));
            document.querySelectorAll('#file-list-body .fileEntry').forEach((el, i) =>
                el.classList.toggle('active', i === idx));
        }

        // Pin the clicked row as an alignment anchor and re-render. The row's
        // data-o/data-n are 1-based line numbers; the anchor stores the 0-based
        // counts above it (op/np) plus the row kind, matching lineDiff's pin.
        function resyncHere(rowEl) {
            if (!rowEl || rowEl.classList.contains('dl-gap')) return;
            const dataO = parseInt(rowEl.getAttribute('data-o'), 10) || 0;
            const dataN = parseInt(rowEl.getAttribute('data-n'), 10) || 0;
            let cls = 'dl-same';
            if (rowEl.classList.contains('dl-add')) cls = 'dl-add';
            else if (rowEl.classList.contains('dl-del')) cls = 'dl-del';
            curAnchors.push({ op: dataO - 1, np: dataN - 1, row: cls });
            renderDiffNow();
            scrollToAnchor(dataO, dataN);
        }

        function clearResync() {
            if (!curAnchors.length) return;
            curAnchors = [];
            renderDiffNow();
        }

        // After a resync, center the newly pinned row and flash it so the user
        // can see where the alignment re-anchored. It re-queries the fresh DOM
        // by (data-o, data-n): the row we clicked is gone after the re-render.
        function scrollToAnchor(o, n) {
            for (const r of paneEl.code.querySelectorAll('.dl')) {
                if (parseInt(r.getAttribute('data-o'), 10) === o
                    && parseInt(r.getAttribute('data-n'), 10) === n) {
                    r.scrollIntoView({ block: 'center' });
                    r.classList.add('anchor-flash');
                    const clear = () => r.classList.remove('anchor-flash');
                    r.addEventListener('animationend', clear, { once: true });
                    setTimeout(clear, 1400);   // fallback if animationend never fires
                    return;
                }
            }
        }

        // Re-diff the current focus when the ignore-whitespace toggle flips.
        // `.diff-split` is present exactly when a diff is on screen, so this is
        // a no-op on a placeholder (external class / no recorded source).
        document.getElementById('ignoreWs').addEventListener('change', e => {
            ignoreWhitespace = e.target.checked;
            if (DIFF_MODE && paneEl.code.querySelector('.diff-split')) renderDiffNow();
        });

        // --- Focused class: title, clickable member types, tabs ------------
        function updatePane(n) {
            const c = n.cls;
            if (!c) {
                // External stub: no class record, hence no source file.
                paneEl.title.textContent = n.name;
                paneEl.vars.textContent = '';
                paneEl.tabs.innerHTML = '';
                curFiles = [];
                activePath = null;
                setPlaceholder('external class — no source file in this model');
                return;
            }
            paneEl.title.textContent = displayName(c);

            // Member variables; a type that names a class in the model is a
            // clickable link that focuses that class (and records it in the
            // history, so Ctrl-Z comes straight back here).
            const vars = c.variables || [];
            if (vars.length) {
                paneEl.vars.innerHTML = vars.map(v => {
                    const type = String(v.type || '').trim();
                    const target = classInType(type);
                    const tn = target ? nodes.get(target) : null;
                    const label = esc(type);
                    const inner = target
                        ? '<a data-target="' + esc(target) + '" title="Focus ' + esc(tn.cls ? displayName(tn.cls) : target) + '">' + label + '</a>'
                        : label;
                    return '<span class="pane-var">' + inner + ' ' + esc(v.name) + '</span>';
                }).join('');
                paneEl.vars.querySelectorAll('a[data-target]').forEach(a => {
                    a.addEventListener('click', () => {
                        const t = a.getAttribute('data-target');
                        if (t && nodes.has(t)) focusOn(nodes.get(t));
                    });
                });
            } else {
                paneEl.vars.textContent = '';
            }

            // Diff mode: OLD | NEW side-by-side source instead of the tabbed
            // single-file view (showDiff hides the tabs and fetches both).
            // A CMake target is not part of the old/new class merge, so it has
            // no revision pair to diff.
            if (DIFF_MODE) {
                if (n.cmake) { setPlaceholder('CMake target — no revision pair to diff'); return; }
                showDiff(n); return;
            }

            // Tabs: a CMake target's "source" is the files its build rules
            // attached to it; a class's are the related files, own first.
            curFiles = n.cmake
                ? ((n.cmakeTarget && n.cmakeTarget.sources) || [])
                : relatedFiles(n);
            activePath = null;
            paneEl.tabs.innerHTML = '';
            for (const path of curFiles) {
                const tab = document.createElement('span');
                tab.className = 'pane-tab';
                const segs = String(path).split('/');
                tab.textContent = segs[segs.length - 1] || path;
                tab.title = path;
                tab.addEventListener('click', () => openTab(path));
                paneEl.tabs.appendChild(tab);
            }
            if (curFiles.length) openTab(curFiles[0]);
            else setPlaceholder(n.cmake
                ? 'no source files recorded for this target'
                : 'no source file recorded for this class');
        }

        // One delegated click handler on the stable #pane-code element: any
        // known type name rendered by highlight() (tabbed view or diff split)
        // focuses that class and records it in the history. Delegation on the
        // container survives the innerHTML replacement on every re-render.
        paneEl.code.addEventListener('click', e => {
            const a = (e.target && e.target.closest)
                ? e.target.closest('a[data-target]') : null;
            if (!a) return;
            const t = a.getAttribute('data-target');
            if (t && nodes.has(t)) focusOn(nodes.get(t));
        });

        // =====================================================================
        // Diff-mode review UI: context menu, comment badges / form, file stepping
        // (diff mode only — guarded so a single-input page never wires these).
        // #ctxMenu and #commentForm live after this <script>, so they are read
        // lazily inside the functions below, never cached at load time.
        // =====================================================================

        // --- Review comments ------------------------------------------------
        // Load the shared store once; badges and the export button read allComments.
        function loadComments() {
            if (commentsLoaded) return;
            commentsLoaded = true;
            fetch('/comments')
                .then(r => r.ok ? r.json() : null)
                .then(j => {
                    allComments = (j && Array.isArray(j.comments)) ? j.comments : [];
                    renderCommentMarkers();
                })
                .catch(() => { commentsLoaded = false; });   // retry on a later focus
        }

        // Comments that apply to the file pair currently on screen: a comment's
        // file matches whichever revision it was anchored to (a removed line
        // carries the old path, an added line the new path).
        function commentsForFile() {
            if (!curOldPath && !curNewPath) return [];
            return allComments.filter(c =>
                c.file === curOldPath || c.file === curNewPath);
        }

        // Draw a badge on the row each comment points at. A comment's (oldLine,
        // newLine) maps to a unique (data-o, data-n) row: newLine>0 matches the
        // new-column line (optionally cross-checked with oldLine); a removed
        // line (newLine==0) matches the old-column line. Several comments on the
        // same row share one badge (its text is the count, its title the list).
        function renderCommentMarkers() {
            const list = commentsForFile();
            // Idempotent: clear any badges from an earlier pass first. This
            // function runs after every re-render AND when the comment fetch
            // lands (which may follow a render with no DOM reset between), so
            // without this the same row would accumulate a second badge.
            paneEl.code.querySelectorAll('.cm-badge').forEach(b => b.remove());
            if (!list.length) return;
            const rows = Array.from(paneEl.code.querySelectorAll('.dl:not(.dl-gap)'));
            const resolve = c => {
                for (const r of rows) {
                    const o = parseInt(r.getAttribute('data-o'), 10);
                    const n = parseInt(r.getAttribute('data-n'), 10);
                    if (c.newLine > 0 && n === c.newLine
                        && (c.oldLine === 0 || o === c.oldLine)) return r;
                    if (c.newLine === 0 && c.oldLine > 0 && o === c.oldLine) return r;
                }
                return null;
            };
            const groups = new Map();
            for (const c of list) {
                const row = resolve(c);
                if (!row) continue;                 // alignment moved it; don't guess
                if (!groups.has(row)) groups.set(row, []);
                groups.get(row).push(c);
            }
            for (const [row, cs] of groups) {
                const b = document.createElement('span');
                b.className = 'cm-badge';
                b.textContent = String(cs.length);
                b.title = cs.map(c => (c.text || '') + (c.created ? ' — ' + c.created : ''))
                    .join('\n');
                row.appendChild(b);
            }
        }

        // The comment payload for a row, by its column: an added line anchors to
        // the new path (oldLine 0), a removed one to the old path (newLine 0), a
        // matched line to the new path with both 1-based line numbers.
        function commentPayload(rowEl) {
            const o = parseInt(rowEl.getAttribute('data-o'), 10) || 0;
            const n = parseInt(rowEl.getAttribute('data-n'), 10) || 0;
            if (rowEl.classList.contains('dl-add')) {
                return { file: curNewPath || curOldPath, oldLine: 0, newLine: n };
            }
            if (rowEl.classList.contains('dl-del')) {
                return { file: curOldPath || curNewPath, oldLine: o, newLine: 0 };
            }
            return { file: curNewPath || curOldPath, oldLine: o, newLine: n };
        }

        let pendingComment = null;   // the {file, oldLine, newLine} for the open form

        // Position and show the floating comment form beside the row (clamped to
        // the viewport) and remember which line it will be anchored to.
        function addCommentHere(rowEl) {
            if (!rowEl || rowEl.classList.contains('dl-gap')) return;
            wireCommentForm();
            const form = document.getElementById('commentForm');
            if (!form) return;
            const p = commentPayload(rowEl);
            pendingComment = p;
            document.getElementById('cfMeta').textContent =
                (p.file ? p.file : '(no file)')
                + ' — line ' + (p.newLine > 0 ? p.newLine : p.oldLine)
                + (p.newLine > 0 && p.oldLine > 0 ? ' (old L' + p.oldLine + ')' : '');
            document.getElementById('cfText').value = '';
            document.getElementById('cfErr').textContent = '';
            form.style.left = '0px';
            form.style.top = '0px';
            form.style.display = 'block';
            const vw = window.innerWidth, vh = window.innerHeight;
            const fw = form.offsetWidth || 340, fh = form.offsetHeight || 200;
            const rc = rowEl.getBoundingClientRect();
            let left = rc.right + 8;
            if (left + fw > vw - 8) left = Math.max(8, rc.left - fw - 8);
            const top = Math.max(8, Math.min(rc.top, vh - fh - 8));
            form.style.left = left + 'px';
            form.style.top = top + 'px';
            setTimeout(() => document.getElementById('cfText').focus(), 0);
        }

        function hideCommentForm() {
            const form = document.getElementById('commentForm');
            if (form) form.style.display = 'none';
            pendingComment = null;
        }

        let commentFormWired = false;
        // #cfSubmit / #cfCancel are after this <script>, so attach their handlers
        // the first time the form is used, not at load.
        function wireCommentForm() {
            if (commentFormWired) return;
            commentFormWired = true;
            document.getElementById('cfSubmit').addEventListener('click', submitComment);
            document.getElementById('cfCancel').addEventListener('click', hideCommentForm);
            document.getElementById('cfText').addEventListener('keydown', e => {
                if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) submitComment();
            });
        }

        function submitComment() {
            const textEl = document.getElementById('cfText');
            const errEl = document.getElementById('cfErr');
            const val = textEl.value;
            if (!val.trim()) { errEl.textContent = 'Enter a comment before submitting.'; return; }
            if (!pendingComment) { hideCommentForm(); return; }
            errEl.textContent = '';
            const body = {
                file: pendingComment.file || '',
                oldLine: pendingComment.oldLine,
                newLine: pendingComment.newLine,
                text: val
            };
            fetch('/comments', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(body)
            }).then(async r => {
                let j = null;
                try { j = await r.json(); } catch (e) { /* non-JSON body */ }
                if (r.status === 201) {
                    allComments.push(j || body);
                    hideCommentForm();
                    renderCommentMarkers();
                } else {
                    errEl.textContent = (j && j.error)
                        ? j.error : 'submit failed (HTTP ' + r.status + ')';
                }
            }).catch(() => { errEl.textContent = 'Could not reach the server.'; });
        }

        // --- Context menu ---------------------------------------------------
        const ctxMenuEl = () => document.getElementById('ctxMenu');
        let ctxTargetRow = null;    // the .dl row the menu was opened on (or null)

        function closeCtxMenu() {
            const menu = ctxMenuEl();
            if (menu) menu.style.display = 'none';
            ctxTargetRow = null;
        }

        function ctxItem(label, enabled, fn) {
            const d = document.createElement('div');
            d.className = 'ctxItem' + (enabled ? '' : ' disabled');
            d.textContent = label;
            if (enabled) d.addEventListener('click', () => { closeCtxMenu(); fn(); });
            return d;
        }
        const ctxSep = () => {
            const d = document.createElement('div');
            d.className = 'ctxSep';
            return d;
        };

        // View source / View rendered: flips the active file's preference in
        // renderModes and re-renders the tab through the usual openTab path.
        function toggleRender() {
            const p = activePath;
            if (!p) return;
            const rendering = (isMd(p) || isDiagram(p)) && renderMode(p) !== 'source';
            renderModes.set(p, rendering ? 'source' : 'auto');
            openTab(p);
        }

        // Generate a URL that represents the current view state
        function generateCurrentUrl() {
            const url = new URL(window.location.href);

            // Add current focus class if there's one focused
            if (focus.active && pinned.size > 0) {
                const focusedNode = [...pinned.values()][0];
                if (focusedNode && focusedNode.name) {
                    url.searchParams.set('focus', focusedNode.name);
                }
            }

            // Add current layout mode
            url.searchParams.set('layout', layoutSelect.value);

            // Add current zoom level
            url.searchParams.set('zoom', view.dist.toFixed(2));

            // Add current theme
            url.searchParams.set('theme', themeSelect.value);

            // Add current filters if any
            const nameFilter = document.getElementById('filterInput').value;
            const nsFilter = document.getElementById('nsFilterInput').value;
            if (nameFilter) url.searchParams.set('filter', nameFilter);
            if (nsFilter) url.searchParams.set('nsfilter', nsFilter);

            return url.toString();
        }

        // Copy the current URL to clipboard
        function copyCurrentUrl() {
            const url = generateCurrentUrl();
            navigator.clipboard.writeText(url).then(() => {
                // Show user feedback - could be a toast or similar
                console.log('URL copied to clipboard:', url);
            }).catch(err => {
                console.error('Failed to copy URL: ', err);
                // Fallback for browsers that don't support clipboard API
                const textArea = document.createElement('textarea');
                textArea.value = url;
                document.body.appendChild(textArea);
                textArea.select();
                try {
                    document.execCommand('copy');
                } catch (err) {
                    console.error('Failed to copy URL via execCommand: ', err);
                }
                document.body.removeChild(textArea);
            });
        }

        // Build and show the menu at (x, y), clamped to the viewport. `rowEl` is
        // the diff row (or null when right-clicking a gap / non-row), which
        // enables the row-specific items.
        function openCtxMenu(x, y, rowEl) {
            const menu = ctxMenuEl();
            if (!menu) return;
            ctxTargetRow = rowEl;
            const hasRow = !!rowEl;
            const many = fileEntries.length >= 2;
            menu.innerHTML = '';
            // Render/source toggle: enabled when the flip would actually do
            // something (the file is renderable); a plain-code file only shows
            // the disabled hint.
            const p = activePath;
            const renderable = !!p && (isMd(p) || isDiagram(p));
            const rendering = renderable && renderMode(p) !== 'source';
            menu.appendChild(ctxItem(
                rendering ? 'View source' : 'View rendered',
                rendering || renderable, toggleRender));
            menu.appendChild(ctxSep());
            menu.appendChild(ctxItem('Resync here', hasRow, () => resyncHere(ctxTargetRow)));
            menu.appendChild(ctxItem('Clear resync', curAnchors.length > 0, () => clearResync()));
            menu.appendChild(ctxSep());
            menu.appendChild(ctxItem('Previous file  [', many, () => stepFile(-1)));
            menu.appendChild(ctxItem('Next file  ]', many, () => stepFile(1)));
            menu.appendChild(ctxItem('Add comment here', hasRow, () => addCommentHere(ctxTargetRow)));
            menu.appendChild(ctxSep());
            // Add copy link to clipboard option
            menu.appendChild(ctxItem('Copy link to clipboard', true, copyCurrentUrl));
            menu.appendChild(ctxSep());
            const exp = document.createElement('a');
            exp.className = 'ctxItem';
            exp.href = '/export/review';
            exp.download = 'review.md';
            exp.textContent = 'Export review…';
            menu.appendChild(exp);
            menu.style.display = 'block';
            const mw = menu.offsetWidth, mh = menu.offsetHeight;
            let left = x, top = y;
            if (left + mw > window.innerWidth - 4) left = Math.max(4, window.innerWidth - mw - 4);
            if (top + mh > window.innerHeight - 4) top = Math.max(4, window.innerHeight - mh - 4);
            menu.style.left = left + 'px';
            menu.style.top = top + 'px';
            wireCtxDismiss();
        }

        let ctxDismissWired = false;
        function wireCtxDismiss() {
            if (ctxDismissWired) return;
            ctxDismissWired = true;
            // Capture-phase mousedown outside the menu dismisses it (beats the
            // click that would otherwise activate an item). Escape is handled in
            // the global keydown listener.
            document.addEventListener('mousedown', e => {
                const menu = ctxMenuEl();
                if (menu && menu.style.display !== 'none' && !menu.contains(e.target)) {
                    closeCtxMenu();
                }
            }, true);
            window.addEventListener('resize', closeCtxMenu);
            window.addEventListener('blur', closeCtxMenu);
            window.addEventListener('scroll', closeCtxMenu, true);
        }

        // --- File stepping (next / previous) --------------------------------
        // Cycle to the adjacent file in the list (wrapping). `dir` is -1 for
        // previous, +1 for next. Resets anchors (a pin belongs to one file).
        function stepFile(dir) {
            if (fileEntries.length < 2) return;
            const cur = fileIndex.get(fileKeyOf(curOldPath, curNewPath));
            let i;
            if (cur === undefined) i = (dir > 0) ? 0 : fileEntries.length - 1;
            else i = (cur + dir + fileEntries.length) % fileEntries.length;
            const e = fileEntries[i];
            setActiveFile(e.oldPath, e.newPath);
            showFileDiff(e.oldPath, e.newPath);
        }

        // Right-click the source pane for the context menu. Diff mode adds the
        // review items (row-specific when a row was clicked); single-file mode
        // shows the same menu with the review items disabled — the
        // render/source toggle is what matters there. Delegated on the stable
        // #pane-code element, like the click handler, so it survives every
        // innerHTML re-render.
        paneEl.code.addEventListener('contextmenu', e => {
            if (DIFF_MODE && !paneEl.code.querySelector('.diff-split')) return;
            e.preventDefault();
            const row = (e.target && e.target.closest) ? e.target.closest('.dl') : null;
            openCtxMenu(e.clientX, e.clientY,
                row && !row.classList.contains('dl-gap') ? row : null);
        });

        // --- Drag the top handle to resize the pane (clamped 15%–80%) ------
        let paneDragging = false;
        document.getElementById('pane-handle').addEventListener('mousedown', e => {
            paneDragging = true;
            e.preventDefault();
            document.body.style.userSelect = 'none';
            document.body.style.cursor = 'ns-resize';
        });

        // --- Drag the sidebar resize handle to resize the sidebar ------
        let sidebarResizing = false;
        const sidebar = document.getElementById('sidebar');
        const sidebarResizeHandle = document.createElement('div');
        sidebarResizeHandle.id = 'sidebar-resize-handle';
        sidebar.appendChild(sidebarResizeHandle);

        sidebarResizeHandle.addEventListener('mousedown', e => {
            e.preventDefault();
            sidebarResizing = true;
            document.body.style.userSelect = 'none';
            document.body.style.cursor = 'col-resize';
        });

        document.addEventListener('mousemove', e => {
            if (sidebarResizing) {
                const newWidth = window.innerWidth - e.clientX;
                // Clamp to reasonable values: 200px minimum, 80% maximum of window width
                const clampedWidth = Math.max(200, Math.min(newWidth, window.innerWidth * 0.8));
                sidebar.style.width = clampedWidth + 'px';
            }
        });

        document.addEventListener('mouseup', () => {
            if (sidebarResizing) {
                sidebarResizing = false;
                document.body.style.userSelect = '';
                document.body.style.cursor = '';
            }
        });
        window.addEventListener('mousemove', e => {
            if (!paneDragging) return;
            const h = window.innerHeight;
            if (!h) return;
            let pct = ((h - e.clientY) / h) * 100;
            pct = Math.max(15, Math.min(80, pct));
            document.documentElement.style.setProperty('--pane-h', pct + 'vh');
            window.dispatchEvent(new Event('resize'));   // three.js re-measures
        });
        window.addEventListener('mouseup', () => {
            if (!paneDragging) return;
            paneDragging = false;
            document.body.style.userSelect = '';
            document.body.style.cursor = '';
        });
    </script>

    <div id="ctxMenu" role="menu" style="display:none"></div>

    <div id="commentForm" role="dialog" style="display:none">
        <div class="cf-head">New review comment</div>
        <div class="cf-meta" id="cfMeta"></div>
        <textarea id="cfText" placeholder="Describe your review comment&hellip;"></textarea>
        <div class="cf-btns">
            <button type="button" id="cfSubmit" class="cf-submit">Submit</button>
            <button type="button" id="cfCancel" class="cf-cancel">Cancel</button>
        </div>
        <div class="cf-err" id="cfErr"></div>
    </div>
</body>
</html>)HTMLDOC";

}  // namespace uml
