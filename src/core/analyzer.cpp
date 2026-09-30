#include "core/analyzer.h"

#include <filesystem>
#include <iostream>

#include "core/compile_commands_provider.h"
#include "core/directory_provider.h"

Analyzer::Analyzer(const Config& config, const ParserRegistry& registry,
                   EventDispatcher* dispatcher)
    : config_(config), registry_(registry), dispatcher_(dispatcher) {}

AnalysisResult Analyzer::analyze(const SourceFileProvider& provider) {
    AnalysisResult result;

    std::cout << provider.banner() << std::endl;

    for (const std::string& file_path : provider.files()) {
        const std::string extension =
            std::filesystem::path(file_path).extension().string();

        // A single unreadable or malformed file must not abort the run
        try {
            // Broadcast the progress event; registered observers (e.g. the
            // console observer) decide what, if anything, to print.
            emit(AnalysisEvent{AnalysisEvent::Kind::FileParsed, file_path, 0});

            // Ask the registry for the parser that handles this extension; an
            // unregistered extension simply yields no parser (and no classes).
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

AnalysisResult Analyzer::analyze_project(const std::string& project_path) {
    // The directory walk (and its pruning of the configured skip directories)
    // is a provider; the analysis itself is shared with every input source.
    DirectoryFileProvider provider(project_path, config_.skip_dirs);
    return analyze(provider);
}

AnalysisResult Analyzer::analyze_compile_commands(const std::string& compile_commands_path) {
    // A compile database is just another file source: read the listed
    // translation units, then parse them through the standard pipeline.
    CompileCommandsFileProvider provider(compile_commands_path);
    return analyze(provider);
}

void Analyzer::emit(const AnalysisEvent& event) const {
    if (dispatcher_) {
        dispatcher_->notify(event);
    }
}
