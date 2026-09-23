#include "core/analyzer.h"
#include <iostream>
#include <string>

/**
 * @brief Print usage information
 */
void print_usage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_path>\n";
    std::cout << "Options:\n";
    std::cout << "  --compile-commands <path>   Specify path to compile_commands.json\n";
    std::cout << "  -h, --help                  Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " /path/to/project\n";
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

    // Parse command line arguments
    for (int i = 1; i < argc - 1; ++i) {
        std::string arg = argv[i];
        if (arg == "--compile-commands" && i + 1 < argc) {
            input_path = argv[++i];
            use_compile_commands = true;
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

        // Output the analysis results (in JSON format)
        std::cout << "Analysis complete. Results would be output here.\n";
        std::cout << "Classes found: " << result.classes.size() << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error during analysis: " << e.what() << "\n";
        return 1;
    }
}