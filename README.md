# Code Analyzer

A powerful tool for analyzing C++ codebases and generating comprehensive reports.

## Features

- Static code analysis with detailed metrics
- Call graph visualization
- Dependency tracking
- UML class diagram generation (3D interactive)

## Installation

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, or MSVC 2017+)
- CMake 3.10 or higher
- Doxygen (for documentation, optional)

### Building

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Code Analyzer

The main code analyzer can be run with:

```bash
./bin/CodeAnalyzer [options] <input_directory>
```

**Options:**
- `-h, --help`          Show help message
- `--callgraph`         Generate call graph (default: true)
- `--dependencies`      Generate dependency analysis (default: true)
- `--output-dir <dir>`  Output directory for reports (default: ./reports)
- `--format <format>`   Output format: json, html, or xml (default: html)

### UML Generator

The UML generator creates interactive 3D class diagrams from JSON analysis output:

```bash
./bin/UMLGenerator [options] <input_json_file>...
```

**Options:**
- `--hide <regex>`      Hide classes matching regex pattern
- `-h, --help`          Show help message

**Example:**
```bash
# Generate UML diagram from JSON files
./bin/UMLGenerator analysis_output.json

# Generate UML diagram and hide standard library classes
./bin/UMLGenerator --hide "^std::|Test$" analysis_output.json
```

## Generated Output

The tool generates several types of reports in the specified output directory:

1. **HTML Reports**: Interactive visualizations with metrics and call graphs
2. **JSON Files**: Raw data for further processing
3. **UML Diagrams**: 3D interactive class diagrams (when using UMLGenerator)

## Example Output

The generated HTML report includes:
- Code metrics dashboard
- Call graph visualization
- Class dependency analysis
- Interactive 3D UML diagrams

## License

This project is licensed under the MIT License - see the LICENSE file for details.