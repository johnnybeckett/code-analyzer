# Working with the Code Analysis Project

This document provides guidance on how to work with this C++23 code analysis project, which converts C++, C#, and Python projects into structured JSON output.

## Project Overview

This is a command-line application built with C++23 that analyzes source code from multiple programming languages and produces detailed structural information in JSON format. The tool tracks class inheritance, method calls, variable access patterns, visibility modifiers, and other code characteristics.

## Design Patterns and Architecture

The project follows modern C++ design patterns to ensure maintainability and extensibility:

### Core Design Patterns
1. **Strategy Pattern** - Language-specific parsing implementations are handled through strategy pattern for easy extension
2. **Visitor Pattern** - AST traversal for collecting analysis information 
3. **Factory Pattern** - Parser creation based on file type using factory pattern
4. **Observer Pattern** - Notification system for analysis events
5. **Composite Pattern** - Hierarchical representation of code structures
6. **Singleton Pattern** - Global configuration and state management

### Boost Library Integration
The project leverages Boost libraries to enhance functionality:
1. **Boost.Asio** - For asynchronous file I/O operations
2. **Boost.Filesystem** - Cross-platform file and directory operations
3. **Boost.PropertyTree** - Configuration and JSON output handling
4. **Boost.Spirit** - Grammar parsing in C++ parser
5. **Boost.Graph** - Code relationship visualization for UML
6. **Boost.Algorithm** - String manipulation utilities

## Development Guidelines

### Code Quality Standards
- Follow C++23 standards and best practices
- Use Doxygen comments for all public interfaces with examples
- Maintain consistent naming conventions
- Write comprehensive tests for new functionality
- Implement proper error handling with meaningful messages
- Ensure thread safety where required

### Architecture Principles
1. **Modularity** - Clear separation of concerns between components
2. **Extensibility** - Easy addition of new languages or analysis features
3. **Maintainability** - Clean code structure with minimal coupling
4. **Performance** - Optimized algorithms and memory usage
5. **Testability** - Components designed for unit testing

## Getting Started

### Prerequisites
- C++23 compatible compiler (GCC 13+, Clang 16+, or MSVC 2022+)
- CMake 3.22 or higher
- Boost libraries (1.74 or higher recommended)
- Doxygen for documentation generation
- Google Test framework for testing

### Building the Project
```bash
mkdir build && cd build
cmake ..
make
```

## Getting Started

### Prerequisites
- C++23 compatible compiler (GCC 13+, Clang 16+, or MSVC 2022+)
- CMake 3.22 or higher
- Doxygen for documentation generation
- Google Test framework for testing

### Building the Project
```bash
mkdir build && cd build
cmake ..
make
```

## Usage

### Command Line Interface
The tool accepts a path to a project directory or compile_commands.json file:
```bash
./code_analyzer /path/to/project
# or
./code_analyzer --compile-commands /path/to/compile_commands.json
```

### Output Format
The tool generates a JSON file containing detailed code analysis information, including:
- Class hierarchies with inheritance relationships
- Method signatures and call graphs
- Variable access patterns
- Visibility modifiers (public/private/protected)
- Const/read-only property tracking

## Development Workflow

### Adding New Language Support
1. Create a new parser in `src/parser/` directory
2. Implement the language-specific parsing logic
3. Add corresponding tests in `tests/gtest/`
4. Update the main analysis logic to integrate the new parser

### Extending Analysis Features
1. Modify the core data model in `src/core/model.cpp/h`
2. Update the analysis logic in `src/core/analyzer.cpp/h`
3. Add appropriate tests for new functionality

### Testing
- All new features must include both unit and integration tests
- Sample projects are included in `tests/sample_projects/`
- Run tests with: `make test` or `ctest`

## Code Structure

### Core Components
- **Parser**: Language-specific code parsers
- **Analyzer**: Main analysis logic that processes parsed code
- **Model**: Data structures representing code elements
- **Schema**: JSON schema definition for output format

### Key Files
- `src/main.cpp`: Entry point and command line argument parsing
- `src/core/analyzer.cpp/h`: Main analysis logic
- `src/parser/cpp_parser.cpp/h`: C++ parser implementation
- `src/core/schema.cpp/h`: Output format definitions

## Contributing

### Code Style
- Follow C++23 standards and best practices
- Use Doxygen comments for all public interfaces
- Maintain consistent naming conventions
- Write comprehensive tests for new functionality

### Testing
- All code changes must include relevant tests
- Tests should cover edge cases and error conditions
- Ensure existing tests continue to pass after modifications

## Troubleshooting

### Common Issues
1. **Build failures**: Ensure C++23 compiler is properly configured
2. **Parser errors**: Verify input files are valid for the target language
3. **Memory issues**: The tool handles large projects but may require more RAM for very large codebases

### Getting Help
For questions or issues, please check:
- This documentation file
- Doxygen-generated API documentation
- GitHub issues tracker