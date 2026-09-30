#ifndef ANALYZER_H
#define ANALYZER_H

#include <string>

#include "core/config.h"
#include "core/model.h"
#include "core/parser_registry.h"
#include "core/source_file_provider.h"
#include "observers/analysis_observer.h"

class Analyzer {
public:
    Analyzer(const Config& config, const ParserRegistry& registry,
             EventDispatcher* dispatcher = nullptr);

    /**
     * @brief Analyze every file a provider yields, in the provider's order.
     *
     * The provider supplies the file list (a directory walk, a compile
     * database, ...) and nothing else: per-file parsing still goes through
     * the IParser/ParserRegistry pipeline, and progress/completion are still
     * broadcast as events (Observer). This one entry point serves every input
     * source, so adding a provider changes nothing here (OCP/DIP).
     */
    AnalysisResult analyze(const SourceFileProvider& provider);

    /**
     * @brief Walk a directory tree and parse every recognised source file.
     *
     * Convenience wrapper over analyze(): builds a DirectoryFileProvider
     * (pruning the configured skip directories) and runs it.
     */
    AnalysisResult analyze_project(const std::string& project_path);

    /**
     * @brief Analyze the sources listed in a compile_commands.json file.
     *
     * Convenience wrapper over analyze(): builds a CompileCommandsFileProvider
     * and runs it, so compile-database sources flow through the same parser
     * pipeline as a directory walk.
     */
    AnalysisResult analyze_compile_commands(const std::string& compile_commands_path);

private:
    void emit(const AnalysisEvent& event) const;

    const Config& config_;
    const ParserRegistry& registry_;
    EventDispatcher* dispatcher_;  // non-owning; may be null
};

#endif // ANALYZER_H
