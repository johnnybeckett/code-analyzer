#include "core/analyzer.h"

#include "parser/cpp_parser.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>

Analyzer::Analyzer(const Config& config, const ParserRegistry& registry,
                   EventDispatcher* dispatcher)
    : config_(config), registry_(registry), dispatcher_(dispatcher) {}

AnalysisResult Analyzer::analyze_project(const std::string& project_path) {
    AnalysisResult result;

    // Walk the project directory, finding source files of every registered kind
    std::cout << "Analyzing project: " << project_path << std::endl;

    // Directories to prune, per the active configuration (VCS metadata, build
    // artifacts, tooling state by default) — skipping them keeps self-analysis
    // clean
    const std::vector<std::string>& skip_dirs = config_.skip_dirs;

    std::filesystem::recursive_directory_iterator it(
        project_path, std::filesystem::directory_options::skip_permission_denied);
    std::filesystem::recursive_directory_iterator end;

    for (; it != end; ++it) {
        const std::filesystem::directory_entry entry = *it;

        // Prune skip-listed directories before descending into them
        if (entry.is_directory() &&
            std::find(skip_dirs.begin(), skip_dirs.end(),
                      entry.path().filename().string()) != skip_dirs.end()) {
            it.disable_recursion_pending();
            continue;
        }

        if (!entry.is_regular_file()) continue;

        const std::string file_path = entry.path().string();
        const std::string extension = entry.path().extension().string();

        // A single unreadable or malformed file must not abort the whole walk
        try {
            // Broadcast the progress event; registered observers (e.g. the
            // console observer) decide what, if anything, to print.
            emit(AnalysisEvent{AnalysisEvent::Kind::FileParsed, file_path, 0});

            // Ask the registry for the parser that handles this extension; an
            // unregistered extension simply yields no parser (and no classes),
            // exactly as the previous if/else did for unknown extensions.
            if (auto parser = registry_.create(extension); parser) {
                for (auto& parsed_class : parser->parse_file(file_path)) {
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

    AnalysisEvent complete;
    complete.kind = AnalysisEvent::Kind::AnalysisComplete;
    complete.class_count = result.classes.size();
    emit(complete);

    return result;
}

AnalysisResult Analyzer::analyze_compile_commands(const std::string& compile_commands_path) {
    AnalysisResult result;

    // Parse the compile_commands.json file and analyze the sources it lists
    std::cout << "Analyzing compile_commands.json: " << compile_commands_path << std::endl;

    // A compile database names C++ translation units, so route straight to the
    // C++ parser's compile-commands entry point.
    result = CppParser::parse_compile_commands(compile_commands_path);

    AnalysisEvent complete;
    complete.kind = AnalysisEvent::Kind::AnalysisComplete;
    complete.class_count = result.classes.size();
    emit(complete);

    return result;
}

void Analyzer::emit(const AnalysisEvent& event) const {
    if (dispatcher_) {
        dispatcher_->notify(event);
    }
}
