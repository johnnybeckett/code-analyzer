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

# Add executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/model.cpp
    src/core/analyzer.cpp
    src/core/visitor.cpp
    src/core/factory.cpp
    src/core/config.cpp
    src/parser/cpp_parser.cpp
    src/parser/csharp_parser.cpp
    src/parser/python_parser.cpp
    src/observers/analysis_observer.cpp
)

# Add UML generator executable
add_executable(UMLGenerator
    src/uml_generator.cpp
)

# Add documentation generator executable
add_executable(DocumentationGenerator
    tools/self_documenter.cpp
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/observers
    ${Boost_INCLUDE_DIRS}
)

target_include_directories(UMLGenerator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    Boost::json
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
