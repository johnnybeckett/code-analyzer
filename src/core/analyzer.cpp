#include "analyzer.h"
#include "../parser/cpp_parser.h"
#include "../parser/csharp_parser.h"
#include "../parser/python_parser.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>

AnalysisResult Analyzer::analyze_project(const std::string& project_path) {
    AnalysisResult result;

    // Walk through the project directory and find source files of different types
    std::cout << "Analyzing project: " << project_path << std::endl;

    // Directories that are never part of a source tree (VCS metadata, build
    // artifacts, tooling state) — skipping them keeps self-analysis clean
    static const std::set<std::string> skip_dirs = {
        ".git", "build", "cmake-build", "out", "node_modules", ".claude"
    };

    std::filesystem::recursive_directory_iterator it(
        project_path, std::filesystem::directory_options::skip_permission_denied);
    std::filesystem::recursive_directory_iterator end;

    for (; it != end; ++it) {
        const std::filesystem::directory_entry entry = *it;

        // Prune skip-listed directories before descending into them
        if (entry.is_directory() && skip_dirs.count(entry.path().filename().string())) {
            it.disable_recursion_pending();
            continue;
        }

        if (!entry.is_regular_file()) continue;

        std::string file_path = entry.path().string();
        std::string extension = entry.path().extension().string();

        // A single bad file must not abort the whole walk
        try {
            std::cout << "Processing file: " << file_path << std::endl;

            // Check for different source file types and parse accordingly
            // (C++ classes are usually declared in headers, so parse those too)
            if (extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".c"
                || extension == ".h" || extension == ".hpp" || extension == ".hxx") {
                // Parse the file using the C++ parser (may yield several classes)
                for (auto& parsed_class : CppParser::parse_file(file_path)) {
                    result.add_class(std::move(parsed_class));
                }
            } else if (extension == ".cs") {
                // Parse the file using the C# parser
                auto parsed_class = CSharpParser::parse_file(file_path);
                if (parsed_class) {
                    result.add_class(std::move(parsed_class));
                }
            } else if (extension == ".py") {
                // Parse the file using the Python parser
                auto parsed_class = PythonParser::parse_file(file_path);
                if (parsed_class) {
                    result.add_class(std::move(parsed_class));
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: skipping file " << file_path
                      << " (" << e.what() << ")" << std::endl;
        } catch (...) {
            std::cerr << "Warning: skipping file " << file_path
                      << " (unknown error)" << std::endl;
        }
    }

    return result;
}

AnalysisResult Analyzer::analyze_compile_commands(const std::string& compile_commands_path) {
    AnalysisResult result;

    // Parse the compile_commands.json file and analyze source files
    std::cout << "Analyzing compile_commands.json: " << compile_commands_path << std::endl;

    // For now, call the C++ parser's compile commands function
    // In a real implementation this would parse the JSON and process each source file
    result = CppParser::parse_compile_commands(compile_commands_path);

    return result;
}