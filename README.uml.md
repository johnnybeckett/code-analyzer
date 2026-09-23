# UML Generator Tool

A tool that converts JSON output from the code analyzer into interactive 3D UML class diagrams.

## Features

- **Interactive 3D Visualization**: Explore class relationships in a 3D space with rotation, zooming, and panning
- **Filtering Capabilities**: Hide classes by name or regex pattern
- **Self-contained HTML Output**: Generates a complete HTML file with embedded visualization
- **Multiple Input Support**: Can process multiple JSON input files

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
   - Scroll to zoom in/out
   - Right-click and drag to pan

2. **Filtering**:
   - Use the input field to enter regex patterns
   - Classes matching the pattern will be hidden
   - Click "Reset Filter" to show all classes again

3. **Legend**:
   - Blue boxes: Classes
   - Red lines: Inheritance relationships
   - Green lines: Method/variable connections

## Example

```bash
# First run the code analyzer to generate JSON output
./CodeAnalyzer /path/to/project > analysis.json

# Then convert it to UML diagram
./UMLGenerator analysis.json
```

The resulting `uml_diagram.html` file will contain an interactive 3D visualization of your project's class structure.