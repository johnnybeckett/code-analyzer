#ifndef PROVIDER_REGISTRY_H
#define PROVIDER_REGISTRY_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/source_file_provider.h"

/**
 * @brief Maps a provider kind to the factory that builds it (Factory pattern).
 *
 * Mirrors ParserRegistry: the single place that knows which input source
 * produces the file list, so adding a provider (a manifest, a VCS file list,
 * a VCPKG lock, ...) is a new SourceFileProvider subclass plus one
 * register_provider() call — neither main.cpp nor the Analyzer changes (OCP).
 * Callers work through the SourceFileProvider abstraction only (DIP).
 */
/**
 * @brief Everything a provider kind needs to know where its files come from.
 *
 * One struct instead of a growing string-argument list so each kind reads
 * exactly the fields it cares about (ISP): "directory" and "compile-commands"
 * use `path` only; "commit" adds `secondary` (the ref) and `staging` (where
 * the materialized files go; empty = auto temp dir).
 */
struct ProviderOptions {
    std::string path;       // primary input: root dir, DB, or repo
    std::string secondary;  // kind-specific (commit ref); empty for other kinds
    std::string staging;    // commit source only; empty = auto temp dir
};

class ProviderRegistry {
public:
    /**
     * @brief Builds a fresh provider from the inputs that define its file set:
     *        a root directory, a compile database, a repo at a commit, ...
     */
    using ProviderFactory =
        std::function<std::unique_ptr<SourceFileProvider>(const ProviderOptions&)>;

    void register_provider(const std::string& kind, ProviderFactory factory);

    std::unique_ptr<SourceFileProvider> create(const std::string& kind,
                                               const ProviderOptions& opts) const;

    bool has_provider(const std::string& kind) const;

    /**
     * @brief A registry pre-loaded with the standard providers:
     *        "directory" (recursive walk, pruning the given skip directories),
     *        "compile-commands" (a compile_commands.json file list), and
     *        "commit" (a repo at a commit, read via `git show`).
     *
     * Kept in one auditable place so the composition root (main) and the tests
     * register the identical set.
     */
    static ProviderRegistry standard(const std::vector<std::string>& skip_dirs);

private:
    std::map<std::string, ProviderFactory> providers_;
};

#endif // PROVIDER_REGISTRY_H
