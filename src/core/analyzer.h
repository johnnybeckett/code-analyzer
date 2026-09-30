#ifndef ANALYZER_H
#define ANALYZER_H

#include "core/config.h"
#include "core/model.h"
#include "core/parser_registry.h"
#include "observers/analysis_observer.h"
#include <string>

/**
 * @brief Drives the analysis of a project directory or a compile database.
 *
 * The analyzer is agnostic about *how* a file is parsed: it is handed a
 * ParserRegistry and asks it for a parser per file extension. This removes the
 * per-language if/else that used to live here (OCP) and lets the composition
 * root decide which languages are supported (DIP). Walk behavior (which
 * directories are pruned) comes from the injected Config. Progress and
 * completion are broadcast as events to an optional EventDispatcher (Observer).
 */
class Analyzer {
public:
    /**
     * @param config     Supplies walk behavior (skip directories). It must
     *                   outlive this analyzer.
     * @param registry   Supplies a parser for each recognised file extension.
     *                   It must outlive this analyzer.
     * @param dispatcher Optional sink for analysis events. `nullptr` (the
     *                   default) means events are dropped. The analyzer does
     *                   not take ownership; the caller keeps the dispatcher
     *                   alive for as long as it uses this analyzer.
     */
    Analyzer(const Config& config, const ParserRegistry& registry,
             EventDispatcher* dispatcher = nullptr);

    /**
     * @brief Walk a directory tree and parse every recognised source file.
     * @param project_path Path to the project directory
     * @return AnalysisResult containing the classes found across all files
     */
    AnalysisResult analyze_project(const std::string& project_path);

    /**
     * @brief Analyze the sources listed in a compile_commands.json file.
     *
     * A compile database names translation units, so this stays a C++
     * specialization (it calls the C++ parser directly) rather than routing
     * through the per-file IParser abstraction.
     * @param compile_commands_path Path to compile_commands.json
     * @return AnalysisResult containing the classes found in the listed sources
     */
    AnalysisResult analyze_compile_commands(const std::string& compile_commands_path);

private:
    /**
     * @brief Forward an event to the dispatcher, if one was injected.
     */
    void emit(const AnalysisEvent& event) const;

    const Config& config_;
    const ParserRegistry& registry_;
    EventDispatcher* dispatcher_;  // non-owning; may be null
};

#endif // ANALYZER_H
