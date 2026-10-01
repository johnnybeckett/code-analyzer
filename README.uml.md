# UML Generator Tool

A tool that converts JSON output from the code analyzer into interactive 3D UML class diagrams.

## Features

- **Interactive 3D Visualization**: Explore class relationships in a 3D space with rotation, zooming, panning, and keyboard movement
- **Filtering Capabilities**: Hide classes by name regex and/or namespace regex
- **Responsive at Scale**: Only the classes closest to the camera are drawn, so diagrams with thousands of classes stay smooth
- **Self-contained HTML Output**: Generates a complete HTML file with embedded visualization
- **Multiple Input Support**: Can process multiple JSON input files
- **Source Pane**: A resizable bottom pane shows the focused class's real source code (fetched
  from `UmlServer` when served over HTTP) with theme-matched syntax highlighting, plus tabs for
  the class's own file and the files of the classes it references

## Building

The UML generator is built as part of the main project. To build everything:

```bash
mkdir build && cd build
cmake ..
make
```

This will create both the main code analyzer (`CodeAnalyzer`) and the UML generator (`UMLGenerator`).

## Usage

### Basic Usage

```bash
# Generate UML diagram from JSON files
./UMLGenerator data1.json data2.json

# Generate UML diagram with class filtering
./UMLGenerator --hide "^(std::|Test$)" data.json
```

### Options

- `--hide <regex>`: Hide classes matching the specified regex pattern
- `-h, --help`: Show help message

## Output

The tool generates a file called `uml_diagram.html` that contains the interactive 3D visualization. Open this file in any modern web browser to explore the class diagrams.

## Visualization Features

1. **3D Navigation**:
   - Click and drag to rotate the view
   - Scroll to zoom in/out (Ctrl+drag for fine control)
   - Right-click and drag to pan

2. **Keyboard Navigation**:
   - **n** / **→** next class, **p** / **←** previous class, **c** / **Home** center the view
   - **w / a / s / d** move the view forward, left, backward, and right (speed scales with zoom distance)
   - **Ctrl-Z** / **⌘-Z** steps backwards through the focus history (up to 30 steps)

3. **Filtering**:
   - Filter classes by **name** with a regex
   - Filter classes by **namespace** with a second regex (matched against the class namespace)
   - Both filters apply together; click "Reset Filter" to clear them and show all classes again

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
   - When served by `UmlServer` the code is fetched on demand via `GET /source?path=...`; in a
     page opened directly from a file (`file://`) the tabs show a "source not available"
     placeholder instead

7. **Legend**:
   - Blue boxes: Classes
   - Red lines: Inheritance relationships
   - Green lines: Method/variable connections

## Example

```bash
# First run the code analyzer to generate JSON output
./CodeAnalyzer --json analysis.json /path/to/project

# Then convert it to UML diagram
./UMLGenerator analysis.json
```

The resulting `uml_diagram.html` file will contain an interactive 3D visualization of your project's class structure.