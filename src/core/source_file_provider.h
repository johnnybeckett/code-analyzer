#ifndef SOURCE_FILE_PROVIDER_H
#define SOURCE_FILE_PROVIDER_H

#include <string>
#include <vector>

/**
 * @brief A pluggable strategy for discovering the set of source files to analyze.
 *
 * A provider answers the single question "where do the files come from?" and
 * returns them in a stable order. It does no parsing — that stays with the
 * per-file IParser/ParserRegistry pipeline. This keeps the two concerns
 * separate (SRP): a provider is a pure discovery strategy, and the Analyzer
 * depends only on this abstract type (DIP), never on a concrete provider or
 * the registry that builds them.
 *
 * New input sources (a directory walk, a compile database, a manifest, a VCS
 * file list, ...) are each a new SourceFileProvider subclass registered in the
 * ProviderRegistry — neither the Analyzer nor main.cpp changes (OCP).
 */
class SourceFileProvider {
public:
    virtual ~SourceFileProvider() = default;

    /**
     * @brief The ordered list of file paths to parse.
     *
     * Discovery only: no parsing, no per-file console output. The Analyzer is
     * responsible for the per-file events and progress reporting. A provider
     * that cannot read its input reports the error on std::cerr and returns
     * an empty list — mirroring the graceful degradation the compile-database
     * path has always had.
     */
    virtual std::vector<std::string> files() const = 0;

    /**
     * @brief A one-line banner describing this input, printed before the run.
     *
     * E.g. "Analyzing project: /path" or "Analyzing compile_commands.json: /path".
     */
    virtual std::string banner() const = 0;
};

#endif // SOURCE_FILE_PROVIDER_H
