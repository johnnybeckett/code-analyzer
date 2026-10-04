# Code Analyzer Project Structure

## Overview
This is a C++ code analysis tool that generates UML class diagrams from source code. The system supports multiple programming languages including C++, C#, and Python.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        CODE ANALYZER TOOL                               │
└─────────────────────────────────────────────────────────────────────────┘
                                │
        ┌─────────────────────────┼─────────────────────────┐
        │                         │                         │
┌──────────────────┐     ┌──────────────────┐    ┌──────────────────┐
│   ANALYZER       │     │   PARSERS        │    │   UML SERVER     │
│                  │     │                  │    │                  │
│  - analyze_project│     │  - C++ Parser    │    │  - 3D Visualization│
│  - analyze_compile_commands│ │  - C# Parser     │    │  - JSON Processing │
│  - factory pattern│     │  - Python Parser │    │  - HTTP routes     │
│  - language detection│   │                  │    │  - Three.js       │
│                  │     │                  │    │                  │
│  ┌─────────────┐ │     │  ┌─────────────┐ │    │  ┌─────────────┐ │
│  │  Model      │ │     │  │  Model      │ │    │  │  Model      │ │
│  │  Class      │ │     │  │  Class      │ │    │  │  Class      │ │
│  │  Method     │ │     │  │  Method     │ │    │  │  Method     │ │
│  │  Variable   │ │     │  │  Variable   │ │    │  │  Variable   │ │
│  └─────────────┘ │     │  └─────────────┘ │    │  └─────────────┘ │
│                  │     │                  │    │                  │
└──────────────────┘     └──────────────────┘    └──────────────────┘
        │                         │                         │
        └─────────────────────────┼─────────────────────────┘
                                  │
                    ┌─────────────────────────────────────┐
                    │           MAIN APPLICATION          │
                    │                                     │
                    │  - main()                           │
                    │  - Command line parsing            │
                    │  - Output generation               │
                    │  - Error handling                  │
                    └─────────────────────────────────────┘
                                  │
                    ┌─────────────────────────────────────┐
                    │           BUILD SYSTEM              │
                    │                                     │
                    │  - CMakeLists.txt                   │
                    │  - JSON library integration         │
                    │  - Compiler flags                   │
                    │  - Test configuration               │
                    └─────────────────────────────────────┘
                                  │
                    ┌─────────────────────────────────────┐
                    │           TESTS                     │
                    │                                     │
                    │  - Unit tests                       │
                    │  - Integration tests                │
                    │  - UML model tests                  │
                    │  - Parser tests                     │
                    └─────────────────────────────────────┘
```

## Key Components

### 1. Analyzer (src/core/analyzer.cpp)
- Main entry point for code analysis
- Determines which parser to use based on input
- Orchestrates the parsing process

### 2. Parsers (src/parser/)
- **C++ Parser**: Parses C++ source files using AST
- **C# Parser**: Parses C# source files with class/method/variable extraction  
- **Python Parser**: Parses Python source files using AST

### 3. UML Model + Server (src/uml/, src/server/)
- **UmlModel** (`src/uml/uml_model.cpp`): reads the analyzer JSON, loads the classes,
  and splices the class data into the embedded Three.js viewer page
- **UmlServer** (`src/server/uml_server.cpp`): serves that page over HTTP behind a
  router-based REST API (`/source`, `/render`, `/comments`, `/export/review`,
  `/openapi.json`, `/api`), with an allowlist-guarded source route

### 4. Build System (CMakeLists.txt)
- Manages dependencies including jsoncpp library
- Configures proper linking for all components
- Sets up test suite compilation

## Data Flow

```
Source Code Files ──┐
                    ├─── Analyzer ────┐
                    │                  ├─── JSON Output ───┐
C++/C#/Python Files ──┤                  │                     │
                    │                  ├─── UML Model ────┤
                    └─── Parser ──────┘                     │
                                                             │
                                                             └─── Served page (UmlServer, over HTTP)
```

## Key Improvements

### Before the Fix:
- UML viewer page was a placeholder template with only console logs
- No actual class data processing from JSON files
- Missing 3D visualization implementation
- Only basic UI controls without functionality

### After the Fix:
- **Proper Data Processing**: Reads and parses JSON data from analysis results
- **3D Visualization**: Implements interactive Three.js scene showing classes as blue cubes
- **Interactive Features**: Supports rotation, zooming, panning, and filtering
- **Build Integration**: Properly links against required libraries (jsoncpp)
- **Test Coverage**: Added unit tests for the UML model splicing

## File Structure

```
src/
├── main.cpp                  # Main application entry point
├── core/
│   ├── analyzer.h/cpp        # Main analysis orchestrator
│   ├── model.h/cpp           # Data models (Class, Method, Variable)
│   ├── factory.h/cpp         # Parser factory pattern
│   └── config.h/cpp          # Configuration management
├── parser/
│   ├── cpp_parser.h/cpp      # C++ source code parsing
│   ├── csharp_parser.h/cpp   # C# source code parsing  
│   └── python_parser.h/cpp   # Python source code parsing
├── uml/                      # UML model: class loading + viewer page splicing
└── server/                   # UmlServer: router-based HTTP server for the viewer

tests/
├── gtest/
│   ├── main_test.cpp         # Main test suite
│   ├── csharp_test.cpp       # C# parser tests
│   └── uml_test.cpp          # UML template splice tests
```

## Usage Flow

1. User runs the analyzer on a project directory or compile_commands.json
2. Analyzer parses source files using appropriate parsers for each language
3. Results are stored in JSON format
4. UmlServer reads the JSON data, splices the viewer page, and serves it over HTTP
5. Open the printed URL in a browser to explore the interactive 3D diagram