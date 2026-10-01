#include "core/provider_registry.h"

#include "core/commit_provider.h"
#include "core/compile_commands_provider.h"
#include "core/directory_provider.h"

void ProviderRegistry::register_provider(const std::string& kind, ProviderFactory factory) {
    providers_[kind] = std::move(factory);
}

std::unique_ptr<SourceFileProvider>
ProviderRegistry::create(const std::string& kind, const ProviderOptions& opts) const {
    auto it = providers_.find(kind);
    if (it == providers_.end()) return nullptr;
    return it->second(opts);
}

bool ProviderRegistry::has_provider(const std::string& kind) const {
    return providers_.find(kind) != providers_.end();
}

ProviderRegistry ProviderRegistry::standard(const std::vector<std::string>& skip_dirs) {
    ProviderRegistry registry;

    // A recursive directory walk, pruning the configured skip directories
    registry.register_provider("directory", [skip_dirs](const ProviderOptions& opts) {
        return std::make_unique<DirectoryFileProvider>(opts.path, skip_dirs);
    });

    // A compile_commands.json file list
    registry.register_provider("compile-commands", [](const ProviderOptions& opts) {
        return std::make_unique<CompileCommandsFileProvider>(opts.path);
    });

    // A repo at a commit: files read via `git show`, submodules at their pinned SHAs
    registry.register_provider("commit", [](const ProviderOptions& opts) {
        return std::make_unique<CommitFileProvider>(
            opts.path, opts.secondary, opts.staging);
    });

    return registry;
}
