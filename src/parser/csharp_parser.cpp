#include "csharp_parser.h"
#include "../core/model.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>
#include <filesystem>

/**
 * @brief Parse a C# file and extract class information
 * @param file_path Path to the C# file
 * @return Parsed Class object or nullptr if error
 */
std::unique_ptr<Class> CSharpParser::parse_file(const std::string& file_path) {
    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "Error: File does not exist - " << file_path << std::endl;
        return nullptr;
    }

    // Read the entire file
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file - " << file_path << std::endl;
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Find class declarations
    std::regex class_regex(R"(class\s+(\w+)(?:\s*:\s*(.+?))?\s*\{)");
    std::smatch matches;
    std::string::const_iterator search_start(content.cbegin());

    std::unique_ptr<Class> parsed_class = nullptr;

    while (std::regex_search(search_start, content.cend(), matches, class_regex)) {
        std::string class_name = matches[1];
        std::string base_classes = matches[2];

        // Create the class object
        parsed_class = std::make_unique<Class>(class_name, "");

        // Parse inheritance if exists
        if (!base_classes.empty()) {
            // Split by comma for multiple inheritance
            std::regex base_regex(R"(\s*(\w+)\s*)");
            std::smatch base_matches;
            std::string::const_iterator base_start(base_classes.cbegin());

            while (std::regex_search(base_start, base_classes.cend(), base_matches, base_regex)) {
                parsed_class->add_inheritance(base_matches[1]);
                base_start = base_matches.suffix().first;
            }
        }

        // Parse methods and fields within the class
        parse_class_content(content, matches.position(0), parsed_class.get());

        break; // For now, we only process the first class found in a file
    }

    return parsed_class;
}

/**
 * @brief Parse the content of a class to extract methods and variables
 * @param content Full file content
 * @param start_pos Position where the class starts
 * @param class_obj Class object to populate with extracted data
 */
void CSharpParser::parse_class_content(const std::string& content, size_t start_pos, Class* class_obj) {
    if (!class_obj) return;

    // Find the end of the class definition
    size_t brace_count = 0;
    size_t class_end_pos = start_pos;
    bool in_class = false;

    for (size_t i = start_pos; i < content.length(); ++i) {
        if (content[i] == '{') {
            if (!in_class) in_class = true;
            brace_count++;
        } else if (content[i] == '}') {
            brace_count--;
            if (brace_count == 0 && in_class) {
                class_end_pos = i;
                break;
            }
        }
    }

    // Extract content within the class
    std::string class_content = content.substr(start_pos, class_end_pos - start_pos + 1);

    // Parse methods (both regular and constructor)
    parse_methods(class_content, class_obj);

    // Parse fields/variables
    parse_fields(class_content, class_obj);
}

/**
 * @brief Parse methods from class content
 * @param content Content of the class
 * @param class_obj Class object to populate with methods
 */
void CSharpParser::parse_methods(const std::string& content, Class* class_obj) {
    if (!class_obj) return;

    // Match method signatures: [access] [static] [virtual] [override] [return_type] method_name(params)
    // This regex handles various C# method patterns including constructors
    std::regex method_regex(R"((?:public|private|protected|internal)\s+(?:static\s+)?(?:virtual\s+)?(?:override\s+)?(?:\w+)\s+(\w+)\s*\(([^)]*)\))");
    std::smatch matches;
    std::string::const_iterator search_start(content.cbegin());

    while (std::regex_search(search_start, content.cend(), matches, method_regex)) {
        std::string method_name = matches[1];
        std::string parameters = matches[2];

        // Create method object - use a generic return type for now
        auto method = std::make_unique<Method>(method_name, "");
        method->return_type = "void";  // Default to void

        // Parse parameters (simplified)
        if (!parameters.empty()) {
            // Match parameter types and names
            std::regex param_regex(R"(\w+\s+(\w+))");
            std::smatch param_matches;
            std::string::const_iterator param_start(parameters.cbegin());

            while (std::regex_search(param_start, parameters.cend(), param_matches, param_regex)) {
                method->parameters.push_back(param_matches[1]);
                param_start = param_matches.suffix().first;
            }
        }

        class_obj->add_method(std::move(method));
        search_start = matches.suffix().first;
    }
}

/**
 * @brief Parse fields/variables from class content
 * @param content Content of the class
 * @param class_obj Class object to populate with variables
 */
void CSharpParser::parse_fields(const std::string& content, Class* class_obj) {
    if (!class_obj) return;

    // Match field declarations: [access] [static] [type] variable_name;
    std::regex field_regex(R"((?:public|private|protected|internal)\s+(?:static\s+)?(\w+)\s+(\w+)\s*;)");
    std::smatch matches;
    std::string::const_iterator search_start(content.cbegin());

    while (std::regex_search(search_start, content.cend(), matches, field_regex)) {
        std::string type = matches[1];
        std::string variable_name = matches[2];

        // Create variable object
        auto variable = std::make_unique<Variable>(variable_name, "", type, Mutability::READ_WRITE);
        class_obj->add_variable(std::move(variable));
        search_start = matches.suffix().first;
    }
}

/**
 * @brief Parse C# project files to analyze multiple source files
 * @param project_path Path to the C# project directory
 * @return AnalysisResult containing parsed information
 */
AnalysisResult CSharpParser::parse_project(const std::string& project_path) {
    AnalysisResult result;

    // In a real implementation, this would:
    // 1. Look for .csproj files or other project configuration
    // 2. Identify source files in the project
    // 3. Parse each source file using parse_file()
    // 4. Return an AnalysisResult with parsed data

    std::cout << "Parsing C# project: " << project_path << std::endl;

    // For now, we'll just walk through the directory and parse .cs files
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(project_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".cs") {
                auto parsed_class = parse_file(entry.path().string());
                if (parsed_class) {
                    result.add_class(std::move(parsed_class));
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing project: " << e.what() << std::endl;
    }

    return result;
}