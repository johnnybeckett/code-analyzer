#include "parser/python_parser.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>

/**
 * @brief Parse a Python file and extract class information
 * @param file_path Path to the Python source file
 * @return Unique pointer to parsed Class object or nullptr if parsing fails
 */
std::unique_ptr<Class> PythonParser::parse_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << file_path << std::endl;
        return nullptr;
    }

    // Read the entire file content
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Find class declarations using regex
    std::regex class_regex(R"(class\s+(\w+)(?:\s*\(([^)]*)\))?\s*:)");
    std::smatch matches;
    std::string::const_iterator search_start(content.cbegin());

    // Simple approach - extract first class found in file
    if (std::regex_search(search_start, content.cend(), matches, class_regex)) {
        std::string class_name = matches[1].str();

        // Create the class with proper namespace handling
        auto parsed_class = std::make_unique<Class>(class_name, "");
        parsed_class->kind = "class";  // Python types are always classes
        parsed_class->file = file_path;

        // Extract inheritance information if present. Each comma-separated
        // token is trimmed, and keyword arguments such as
        // `metaclass=ABCMeta` are skipped — only plain base class names are
        // recorded as inheritance.
        if (matches.size() > 2 && !matches[2].str().empty()) {
            std::string inheritance_list = matches[2].str();
            size_t start = 0;
            while (true) {
                size_t pos = inheritance_list.find(',', start);
                std::string token = (pos == std::string::npos)
                    ? inheritance_list.substr(start)
                    : inheritance_list.substr(start, pos - start);

                size_t b = token.find_first_not_of(" \t");
                size_t e = token.find_last_not_of(" \t");
                token = (b == std::string::npos) ? "" : token.substr(b, e - b + 1);

                if (!token.empty() && token.find('=') == std::string::npos) {
                    parsed_class->add_inheritance(token);
                }
                if (pos == std::string::npos) break;
                start = pos + 1;
            }
        }

        std::cout << "Found Python class: " << class_name << " in file: " << file_path << std::endl;
        return parsed_class;
    }

    return nullptr;
}

/**
 * @brief Parse a Python project directory
 * @param project_path Path to the Python project directory
 * @return AnalysisResult containing parsed information
 */
AnalysisResult PythonParser::parse_project(const std::string& project_path) {
    AnalysisResult result;

    // In a real implementation, this would identify Python source files in the project
    // and parse each one using parse_file()
    std::cout << "Parsing Python project: " << project_path << std::endl;

    return result;
}

// These methods are declared but not implemented for now (minimal implementation)
void PythonParser::parse_class_content(const std::string& content, size_t start_pos, Class* class_obj) {
    // Placeholder implementation
}

void PythonParser::parse_methods(const std::string& content, Class* class_obj) {
    // Placeholder implementation
}

void PythonParser::parse_fields(const std::string& content, Class* class_obj) {
    // Placeholder implementation
}