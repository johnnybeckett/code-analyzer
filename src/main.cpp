#include "core/analyzer.h"
#include "core/config.h"
#include "core/json_serializer.h"
#include "core/parser_registry.h"
#include "core/provider_registry.h"
#include "core/report_visitor.h"
#include "observers/analysis_observer.h"
#include "observers/console_observer.h"
#include <boost/json.hpp>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>

/**
 * @brief Print usage information
 */
void print_usage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_path>\n";
    std::cout << "Options:\n";
    std::cout << "  --directory <root>          Recursively analyze the project rooted at\n";
    std::cout << "                              <root>: every C++ source and header it\n";
    std::cout << "                              finds. Same as passing <root> as\n";
    std::cout << "                              <input_path>; listed here so the input\n";
    std::cout << "                              modes are explicit as more are added.\n";
    std::cout << "  --compile-commands <path>   Analyze only the files listed in\n";
    std::cout << "                              compile_commands.json (the .cpp translation\n";
    std::cout << "                              units). Does NOT follow #include'd headers\n";
    std::cout << "                              or .tpp template files.\n";
    std::cout << "  --commit <repo[@ref]>       Analyze the repository at <repo> as of\n";
    std::cout << "                              <ref> (default HEAD) from git objects — no\n";
    std::cout << "                              checkout needed. Superproject/submodule\n";
    std::cout << "                              layouts are detected: each submodule's\n";
    std::cout << "                              files are read at the commit SHA the\n";
    std::cout << "                              parent repo records for it.\n";
    std::cout << "  --staging <dir>             With --commit: materialize the commit's\n";
    std::cout << "                              files into <dir> and keep it (by default\n";
    std::cout << "                              a temp dir is used and removed after the\n";
    std::cout << "                              run).\n";
    std::cout << "  --json <file>               Write analysis results to a JSON file\n";
    std::cout << "  --config <file>             Read options (source_extensions, skip_dirs)\n";
    std::cout << "                              from a JSON configuration file\n";
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
    std::cout << "  " << program_name << " --directory /path/to/project\n";
    std::cout << "  " << program_name << " --json out.json /path/to/project\n";
    std::cout << "  " << program_name << " --compile-commands /path/to/compile_commands.json\n";
    std::cout << "  " << program_name << " --commit /path/to/repo@v1.2.0\n";
    std::cout << "  " << program_name << " --staging /tmp/keep --commit /path/to/repo@HEAD\n";
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
    bool use_commit = false;
    std::string staging_dir;
    std::string json_output;
    std::string config_path;

    // Parse command line arguments. The last argument is pre-seeded above as
    // the positional input path, so it is scanned here too: a trailing -h or
    // --help must still trigger the help path. Option flags landing at the
    // final position simply fail their i + 1 < argc guard and fall through.
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--compile-commands" && i + 1 < argc) {
            input_path = argv[++i];
            use_compile_commands = true;
            use_commit = false;
        } else if (arg == "--directory" && i + 1 < argc) {
            input_path = argv[++i];
            use_compile_commands = false;
            use_commit = false;
        } else if (arg == "--commit" && i + 1 < argc) {
            input_path = argv[++i];
            use_commit = true;
            use_compile_commands = false;
        } else if (arg == "--staging" && i + 1 < argc) {
            staging_dir = argv[++i];
        } else if (arg == "--json" && i + 1 < argc) {
            json_output = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    // Options: an explicit --config file wins; otherwise the built-in defaults
    Config config;
    if (!config_path.empty()) {
        auto loaded = Config::load(config_path);
        if (!loaded) {
            std::cerr << "Error: could not load configuration from " << config_path << "\n";
            return 1;
        }
        config = std::move(*loaded);
    }

    try {
        // Composition root: assemble the parser registry (which extension is
        // parsed by which language, per config), the provider registry (which
        // input mode supplies the file list), the observer pipeline (what
        // happens to events), and the analyzer that drives them all.
        ParserRegistry registry = ParserRegistry::standard(config.source_extensions);
        ProviderRegistry providers = ProviderRegistry::standard(config.skip_dirs);
        EventDispatcher dispatcher;
        dispatcher.add_observer(std::make_unique<ConsoleObserver>());
        Analyzer analyzer(config, registry, &dispatcher);

        // The input mode selects the file provider; the analyzer itself is
        // agnostic about where the files come from.
        ProviderOptions opts;
        std::string kind;
        if (use_commit) {
            // <repo[@ref]> splits on the LAST '@' (a path may legally contain
            // one); with no '@' the ref defaults to HEAD inside the provider.
            const auto at = input_path.rfind('@');
            if (at > 0) {
                opts.path = input_path.substr(0, at);
                opts.secondary = input_path.substr(at + 1);
            } else {
                opts.path = input_path;
            }
            opts.staging = staging_dir;
            kind = "commit";
        } else {
            opts.path = input_path;
            kind = use_compile_commands ? "compile-commands" : "directory";
        }
        auto provider = providers.create(kind, opts);
        if (!provider) {
            std::cerr << "Error: unknown input mode \"" << kind << "\"\n";
            return 1;
        }

        AnalysisResult result = analyzer.analyze(*provider);

        // Optionally write the full analysis to a JSON file
        if (!json_output.empty()) {
            std::ofstream out(json_output);
            if (!out.is_open()) {
                std::cerr << "Error: could not open " << json_output << " for writing\n";
                return 1;
            }
            out << boost::json::serialize(serialize(result, input_path));
            out.close();
            std::cout << "JSON written to: " << json_output << "\n";
        }

        // Output the analysis summary
        SummaryVisitor reporter;
        reporter.render(result);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error during analysis: " << e.what() << "\n";
        return 1;
    }
}
