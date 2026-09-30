#include "core/provider_registry.h"

#include "core/compile_commands_provider.h"
#include "core/directory_provider.h"

void ProviderRegistry::register_provider(const std::string& kind, ProviderFactory factory) {
    providers_[kind] = std::move(factory);
}

std::unique_ptr<SourceFileProvider>
ProviderRegistry::create(const std::string& kind, const std::string& path) const {
    auto it = providers_.find(kind);
    if (it == providers_.end()) return nullptr;
    return it->second(path);
}

bool ProviderRegistry::has_provider(const std::string& kind) const {
    return providers_.find(kind) != providers_.end();
}

ProviderRegistry ProviderRegistry::standard(const std::vector<std::string>& skip_dirs) {
    ProviderRegistry registry;

    // A recursive directory walk, pruning the configured skip directories
    registry.register_provider("directory", [skip_dirs](const std::string& root) {
        return std::make_unique<DirectoryFileProvider>(root, skip_dirs);
    });

    // A compile_commands.json file list
    registry.register_provider("compile-commands", [](const std::string& path) {
        return std::make_unique<CompileCommandsFileProvider>(path);
    });

    return registry;
}
