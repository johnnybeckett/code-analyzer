#ifndef PARSER_REGISTRY_H
#define PARSER_REGISTRY_H

#include "core/iparser.h"
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <vector>

/**
 * @brief Maps a file extension to the factory that builds its parser (Factory).
 *
 * This is the single place that knows "which extension is parsed by which
 * language." Adding a language means registering a new (extension, adapter)
 * pair; no existing call site changes (OCP). Consumers call create() and work
 * entirely through the IParser abstraction, so they never name a concrete
 * parser (DIP).
 */
class ParserRegistry {
public:
    /**
     * @brief Builds a fresh parser instance.
     *
     * Held as a factory (rather than a pre-built IParser) so the registry can
     * hand out an independent object per call, and so adapters with state
     * could be supported without reworking the interface.
     */
    using ParserFactory = std::function<std::unique_ptr<IParser>()>;

    /**
     * @brief Register the parser factory for a file extension.
     * @param extension Extension including the leading dot (e.g. ".cpp")
     * @param factory  A callable returning a fresh IParser for the extension
     */
    void register_parser(const std::string& extension, ParserFactory factory);

    /**
     * @brief Build the parser registered for an extension.
     * @param extension Extension including the leading dot
     * @return A fresh parser, or nullptr if the extension is not registered
     */
    std::unique_ptr<IParser> create(const std::string& extension) const;

    /** @brief True if a parser is registered for the extension. */
    bool has_parser(const std::string& extension) const;

    /**
     * @brief A registry pre-loaded with the analyzer's standard extensions:
     *        C++ (.cpp .cc .cxx .c .h .hpp .hxx .hh .tpp .tcc), C# (.cs), and
     *        Python (.py).
     *
     * Kept in one auditable place so the composition root (main) and the tests
     * register the identical set. Note ".hxx": it is part of the set the
     * analyzer has always parsed and must not be dropped.
     */
    static ParserRegistry standard();

    /**
     * @brief A standard registry restricted to the given extensions.
     * @param extensions Extensions to keep (leading dot), e.g. from
     *        Config::source_extensions. Extensions unknown to the standard
     *        set are ignored — the registry only knows the bundled languages.
     *
     * Lets a configuration opt *down* from the full set without main naming
     * concrete adapters; behavior for unregistered extensions is unchanged
     * (the file is skipped, exactly as today).
     */
    static ParserRegistry standard(const std::vector<std::string>& extensions);

private:
    std::map<std::string, ParserFactory> parsers_;
};

#endif // PARSER_REGISTRY_H
