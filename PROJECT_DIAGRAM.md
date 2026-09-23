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
│   ANALYZER       │     │   PARSERS        │    │   UML GENERATOR  │
│                  │     │                  │    │                  │
│  - analyze_project│     │  - C++ Parser    │    │  - 3D Visualization│
│  - analyze_compile_commands│ │  - C# Parser     │    │  - JSON Processing │
│  - factory pattern│     │  - Python Parser │    │  - HTML Generation │
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
                    │  - UML generator tests              │
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

### 3. UML Generator (src/uml_generator.cpp)
- **Before**: Placeholder HTML template showing only console logs
- **After**: Full implementation that:
  - Reads JSON data from analysis results
  - Creates interactive 3D visualization using Three.js
  - Displays classes as blue cubes in a grid arrangement
  - Supports filtering and interaction controls

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
                    │                  ├─── UML Generator ──┤
                    └─── Parser ──────┘                     │
                                                             │
                                                             └─── HTML File (uml_diagram.html)
```

## Key Improvements

### Before the Fix:
- UML generator had placeholder HTML with only console logs
- No actual class data processing from JSON files
- Missing 3D visualization implementation
- Only basic UI controls without functionality

### After the Fix:
- **Proper Data Processing**: Reads and parses JSON data from analysis results
- **3D Visualization**: Implements interactive Three.js scene showing classes as blue cubes
- **Interactive Features**: Supports rotation, zooming, panning, and filtering
- **Build Integration**: Properly links against required libraries (jsoncpp)
- **Test Coverage**: Added unit tests for the UML generator functionality

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
└── uml_generator.cpp         # UML diagram generation with 3D visualization

tests/
├── gtest/
│   ├── main_test.cpp         # Main test suite
│   ├── csharp_test.cpp       # C# parser tests
│   └── uml_test.cpp          # UML generator tests
```

## Usage Flow

1. User runs the analyzer on a project directory or compile_commands.json
2. Analyzer parses source files using appropriate parsers for each language
3. Results are stored in JSON format
4. UML Generator reads the JSON data and creates 3D visualization
5. Output is saved as `uml_diagram.html` with interactive 3D diagram