#include "src/core/model.h"
#include "src/core/analyzer.h"
#include "src/parser/cpp_parser.h"
#include "src/parser/csharp_parser.h"
#include "src/parser/python_parser.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "Testing code analyzer implementation..." << std::endl;

    // Test the C++ parser with a sample file
    auto cpp_class = CppParser::parse_file("test_files/SampleClass.cs");
    if (cpp_class) {
        std::cout << "Successfully parsed C++ class: " << cpp_class->name << std::endl;
    } else {
        std::cout << "Failed to parse C++ class" << std::endl;
    }

    // Test the C# parser with sample files
    auto csharp_class = CSharpParser::parse_file("test_files/SampleClass.cs");
    if (csharp_class) {
        std::cout << "Successfully parsed C# class: " << csharp_class->name << std::endl;
    } else {
        std::cout << "Failed to parse C# class" << std::endl;
    }

    // Test the Python parser
    auto python_class = PythonParser::parse_file("test_files/sample.py");
    if (python_class) {
        std::cout << "Successfully parsed Python class: " << python_class->name << std::endl;
    } else {
        std::cout << "Failed to parse Python class" << std::endl;
    }

    std::cout << "Test completed." << std::endl;
    return 0;
}