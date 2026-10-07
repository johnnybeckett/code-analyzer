# UML Model Viewer

An interactive 3D UML class-diagram viewer built from the code analyzer's JSON output.
The page is assembled by the `code_analyzer_uml` library (class loading + template
splicing) and served over HTTP by **`UmlServer`** — see
[README.server.md](README.server.md) for the full server reference (routes, CLI,
source-access allowlist).

## Features

- **Interactive 3D Visualization**: Explore class relationships in a 3D space with rotation, zooming, panning, and keyboard movement
- **Filtering Capabilities**: Hide classes by name regex and/or namespace regex
- **Responsive at Scale**: Only the classes closest to the camera are drawn, so diagrams with thousands of classes stay smooth
- **Served, not written to disk**: `UmlServer` serves the complete page (embedded
  three.js, no external assets) over HTTP
- **Source Pane**: A resizable bottom pane shows the focused class's real source code,
  fetched on demand from `UmlServer`'s `/source` route, with theme-matched syntax
  highlighting and tabs for the class's own file and the files of the classes it references

## Building

The viewer page is served by the UML model server, built as part of the main project.
To build everything:

```bash
mkdir build && cd build
cmake ..
make
```

This creates the code analyzer (`CodeAnalyzer`) and the UML model server (`UmlServer`).

## Usage

### Basic Usage

```bash
# Serve a single analyzer JSON file on the default port (8000)
./UmlServer data.json

# Choose a port
./UmlServer --port 9000 data.json

# Serve with class filtering (diff mode: older newer)
./UmlServer --port 9000 --hide "^(std::|Test$)" older.json newer.json
```

### Options

- `--port <n>`: TCP port to listen on (default `8000`)
- `--hide <regex>`: Hide classes matching the specified regex pattern
- `-h, --help`: Show help message

## Visualization Features

1. **3D Navigation** — the page detects the browser and picks the model:
   *Mouse (desktop)*
   - **Click and drag** rotates the view (orbit; **Alt** for fine control)
   - **Shift + drag** pans the look-at target
   - **Ctrl + drag** or **scroll** zooms the camera in/out
   - **Ctrl + scroll** zooms the content (source pane, diagram, call graph)
   - **Double-click** a class to focus it; a member's type or an inheritance
     line jumps to the class at its far end
   - **Invert horizontal / vertical movement** checkboxes in the panel flip
     the drag direction per axis
   *Touch (phones, tablets)*
   - **One-finger drag** orbits · **two-finger pinch** zooms · **two-finger drag** pans
   - **Double-tap** a class to focus it
   - An on-screen **button pad** (bottom of the screen): ‹ / › previous/next
     class, ↑ ↓ ← → hold to move the view, + / − zoom, ⌖ center the view

2. **Keyboard Navigation**:
   - **n** / **→** next class, **p** / **←** previous class, **c** / **Home** center the view
   - **w / a / s / d** move the view forward, left, backward, and right (speed scales with zoom distance)
   - **Ctrl-Z** / **⌘-Z** steps backwards through the focus history (up to 30 steps)

3. **Filtering**:
   - Filter classes by **name** with a regex
   - Filter classes by **namespace** with a second regex (matched against the class namespace)
   - Both filters apply together; click "Reset" to clear them and show all classes again

4. **Performance (level of detail)**:
   - Only the classes closest to the camera are drawn (default the **nearest 500**), so a diagram with thousands of classes stays responsive
   - Adjust the cap with the "Max classes drawn (nearest first)" input
   - Focused classes and their parent stay drawn even if they fall outside the nearest set

5. **Double-click Focus & Jump**:
   - Double-click a class to focus on it
   - Double-click a member's type to jump to that class
   - Double-click an **inheritance line** to jump to the far end of that relationship

6. **Source Pane**:
   - A resizable pane (drag the top handle; defaults to the bottom **33%** of the window,
     clamped to 15–80%) shows the **currently focused class** and the files that implement it
   - **Tabs**: the focused class's own source file first, then the files of directly related
     classes (base classes and classes named by member/parameter/return types), most-referenced
     first, capped at 8
   - **Syntax highlighting** without any CDN asset — keywords, types, strings, comments, and
     numbers colored from the active theme's tokens, so switching themes recolors the code for free
   - **Click a member variable's type** in the pane to jump focus to that class (and back with
     **Ctrl-Z**); clicking a tab opens that file
   - The code is fetched on demand via `GET /source?path=...` from `UmlServer` (allowlist-guarded)

7. **Legend**:
   - Blue boxes: Classes
   - Red lines: Inheritance relationships
   - Green lines: Method/variable connections

## Example

```bash
# 1. Run the code analyzer to produce JSON output
./CodeAnalyzer --json analysis.json /path/to/project

# 2. Serve the viewer page over HTTP
./UmlServer --port 8000 analysis.json

# 3. Open http://localhost:8000/ in a browser
```

The served page contains an interactive 3D visualization of your project's class structure.
