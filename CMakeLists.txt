cmake_minimum_required(VERSION 3.22)
project(CodeAnalyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost 1.71 REQUIRED COMPONENTS json)

# Core static library: shared by the production binary and the test suite.
add_library(code_analyzer_core STATIC
    src/core/model.cpp
    src/core/analyzer.cpp
    src/core/parser_registry.cpp
    src/core/config.cpp
    src/core/report_visitor.cpp
    src/core/json_serializer.cpp
    src/parser/cpp_parser.cpp
    src/parser/csharp_parser.cpp
    src/parser/python_parser.cpp
    src/observers/analysis_observer.cpp
    src/observers/console_observer.cpp
)

# Add executable (thin entry point over the core library)
add_executable(${PROJECT_NAME}
    src/main.cpp
)

# Add UML generator executable (template.h carries the embedded HTML/JS page)
add_executable(UMLGenerator
    src/uml/main.cpp
    src/uml/uml_generator.cpp
    src/uml/class_loader.cpp
    src/uml/template_renderer.cpp
)

# Add documentation generator executable
add_executable(DocumentationGenerator
    tools/self_documenter.cpp
)

# Core library: single include root (src/) so headers are addressed as
# core/model.h, parser/cpp_parser.h, etc. Dependencies propagate to consumers.
target_include_directories(code_analyzer_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(code_analyzer_core PUBLIC
    Boost::json
    Threads::Threads
)

target_include_directories(UMLGenerator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Production binary: links only the core library (no test framework).
target_link_libraries(${PROJECT_NAME} PRIVATE
    code_analyzer_core
)

# Link libraries for UML generator
target_link_libraries(UMLGenerator
    Threads::Threads
    Boost::json
)

# Enable testing
enable_testing()
add_subdirectory(tests)

# Doxygen documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()
