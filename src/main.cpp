#include "core/analyzer.h"
#include <boost/json.hpp>
#include <iostream>
#include <fstream>
#include <string>

/**
 * @brief Convert a Visibility enum to its string form
 */
const char* visibility_name(Visibility v) {
    switch (v) {
        case Visibility::PUBLIC: return "public";
        case Visibility::PRIVATE: return "private";
        case Visibility::PROTECTED: return "protected";
    }
    return "public";
}

/**
 * @brief Convert a Mutability enum to its string form
 */
const char* mutability_name(Mutability m) {
    switch (m) {
        case Mutability::READ_WRITE: return "read_write";
        case Mutability::READ_ONLY: return "read_only";
        case Mutability::CONST: return "const";
    }
    return "read_write";
}

/**
 * @brief Serialize an AnalysisResult to a boost::json::value
 */
boost::json::value to_json(const AnalysisResult& result, const std::string& input_path) {
    namespace json = boost::json;
    json::object root;
    root["generator"] = "CodeAnalyzer";
    root["project_path"] = input_path;
    root["class_count"] = result.classes.size();

    json::array classes;
    for (const auto& class_obj : result.classes) {
        json::object cj;
        cj["name"] = class_obj->name;
        cj["namespace"] = class_obj->full_namespace;
        cj["visibility"] = visibility_name(class_obj->visibility);
        cj["static"] = class_obj->is_static;

        json::array inheritance;
        for (const auto& base : class_obj->inheritance_list) {
            inheritance.emplace_back(base);
        }
        cj["inheritance"] = std::move(inheritance);

        json::array methods;
        for (const auto& method : class_obj->methods) {
            json::object mj;
            mj["name"] = method->name;
            mj["return_type"] = method->return_type;
            mj["visibility"] = visibility_name(method->visibility);
            mj["static"] = method->is_static;
            json::array params;
            for (const auto& param : method->parameters) {
                params.emplace_back(param);
            }
            mj["parameters"] = std::move(params);
            methods.emplace_back(std::move(mj));
        }
        cj["methods"] = std::move(methods);

        json::array variables;
        for (const auto& variable : class_obj->variables) {
            json::object vj;
            vj["name"] = variable->name;
            vj["type"] = variable->type;
            vj["mutability"] = mutability_name(variable->mutability);
            vj["visibility"] = visibility_name(variable->visibility);
            variables.emplace_back(std::move(vj));
        }
        cj["variables"] = std::move(variables);

        classes.emplace_back(std::move(cj));
    }
    root["classes"] = std::move(classes);
    return json::value(std::move(root));
}

/**
 * @brief Print usage information
 */
void print_usage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_path>\n";
    std::cout << "Options:\n";
    std::cout << "  --compile-commands <path>   Analyze only the files listed in\n";
    std::cout << "                              compile_commands.json (the .cpp translation\n";
    std::cout << "                              units). Does NOT follow #include'd headers\n";
    std::cout << "                              or .tpp template files.\n";
    std::cout << "  --json <file>               Write analysis results to a JSON file\n";
    std::cout << "  -h, --help                  Show this help message\n";
    std::cout << "\n";
    std::cout << "Recommended: pass a project DIRECTORY as <input_path>. This walks the\n";
    std::cout << "whole tree and parses every C++ source and header it finds (.cpp, .h,\n";
    std::cout << ".hpp, .hh, .tpp, .tcc, ...), so classes declared in headers and\n";
    std::cout << "template definitions in .tpp files are included. Use\n";
    std::cout << "--compile-commands only when you specifically want the compile database.\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " /path/to/project\n";
    std::cout << "  " << program_name << " --json out.json /path/to/project\n";
    std::cout << "  " << program_name << " --compile-commands /path/to/compile_commands.json\n";
}

/**
 * @brief Main entry point for the code analyzer
 * @param argc Number of command line arguments
 * @param argv Command line arguments
 * @return Exit status
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Error: No input specified\n";
        print_usage(argv[0]);
        return 1;
    }

    std::string input_path = argv[argc - 1];
    bool use_compile_commands = false;
    std::string json_output;

    // Parse command line arguments
    for (int i = 1; i < argc - 1; ++i) {
        std::string arg = argv[i];
        if (arg == "--compile-commands" && i + 1 < argc) {
            input_path = argv[++i];
            use_compile_commands = true;
        } else if (arg == "--json" && i + 1 < argc) {
            json_output = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    try {
        AnalysisResult result;

        if (use_compile_commands) {
            result = Analyzer::analyze_compile_commands(input_path);
        } else {
            result = Analyzer::analyze_project(input_path);
        }

        // Optionally write the full analysis to a JSON file
        if (!json_output.empty()) {
            std::ofstream out(json_output);
            if (!out.is_open()) {
                std::cerr << "Error: could not open " << json_output << " for writing\n";
                return 1;
            }
            out << boost::json::serialize(to_json(result, input_path));
            out.close();
            std::cout << "JSON written to: " << json_output << "\n";
        }

        // Output the analysis results (in JSON format)
        std::cout << "Analysis complete.\n";
        std::cout << "Classes found: " << result.classes.size() << "\n";

        if (result.classes.empty()) {
            std::cout << "No classes were found in the analyzed files.\n";
        } else {
            for (const auto& class_obj : result.classes) {
                std::cout << "Found class: " << class_obj->name << "\n";
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error during analysis: " << e.what() << "\n";
        return 1;
    }
}