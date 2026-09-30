#include "uml/uml_generator.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

/**
 * @brief Print usage information for the UML generator
 */
void print_usage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_json_file>...\n";
    std::cout << "Options:\n";
    std::cout << "  --hide <regex>             Hide classes matching regex pattern\n";
    std::cout << "  -h, --help                 Show this help message\n";
    std::cout << "\n";
    std::cout << "  One file     -> a single combined diagram\n";
    std::cout << "  Two files    -> diff mode, in 'older newer' order:\n";
    std::cout << "                    added (green), removed (red) at the\n";
    std::cout << "                    class, method and member level\n";
    std::cout << "  More than two -> diff uses the first two; the rest ignored\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " data.json\n";
    std::cout << "  " << program_name << " older.json newer.json\n";
    std::cout << "  " << program_name << " --hide \"^std::|Test$\" data.json\n";
}

}  // namespace

/**
 * @brief Main entry point for the UML generator
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

    std::vector<std::string> input_files;
    std::vector<std::string> hide_patterns;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--hide" && i + 1 < argc) {
            hide_patterns.push_back(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            input_files.push_back(arg);
        }
    }

    if (input_files.size() > 2) {
        std::cerr << "Warning: diff mode uses the first two files; ignoring the rest\n";
    }

    // Create and run the UML generator
    uml::UmlGenerator generator(input_files, hide_patterns);

    if (generator.generate()) {
        std::cout << "UML diagram generated successfully!\n";
        return 0;
    } else {
        std::cerr << "Failed to generate UML diagram.\n";
        return 1;
    }
}
