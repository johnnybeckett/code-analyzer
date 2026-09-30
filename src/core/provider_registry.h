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
class ProviderRegistry {
public:
    /**
     * @brief Builds a fresh provider for a given input path.
     *
     * Carries the path (unlike ParserFactory) because a provider's file set is
     * defined by where it looks: a root directory, a compile database, ...
     */
    using ProviderFactory =
        std::function<std::unique_ptr<SourceFileProvider>(const std::string&)>;

    void register_provider(const std::string& kind, ProviderFactory factory);

    std::unique_ptr<SourceFileProvider> create(const std::string& kind,
                                               const std::string& path) const;

    bool has_provider(const std::string& kind) const;

    /**
     * @brief A registry pre-loaded with the standard providers:
     *        "directory" (recursive walk, pruning the given skip directories)
     *        and "compile-commands" (a compile_commands.json file list).
     *
     * Kept in one auditable place so the composition root (main) and the tests
     * register the identical set.
     */
    static ProviderRegistry standard(const std::vector<std::string>& skip_dirs);

private:
    std::map<std::string, ProviderFactory> providers_;
};

#endif // PROVIDER_REGISTRY_H
