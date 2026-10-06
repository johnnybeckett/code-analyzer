#include "server/source_resolver.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

namespace server {

std::optional<std::string> resolve_source(const std::string& recorded,
                                          const std::filesystem::path& json_dir) {
    namespace fs = std::filesystem;

    // Priority: the recorded string as-is (absolute, or valid against the
    // server's CWD); then, for a relative path, the same path under the input
    // JSON's directory. The first readable candidate wins.
    std::vector<std::string> candidates{recorded};
    if (!recorded.empty() && !fs::path(recorded).is_absolute()) {
        candidates.push_back((json_dir / recorded).string());
    }

    for (const auto& candidate : candidates) {
        std::ifstream in(candidate, std::ios::binary);
        if (!in) {
            continue;
        }
        std::ostringstream data;
        data << in.rdbuf();
        return data.str();
    }

    return std::nullopt;
}

}  // namespace server
