#include "core/renderable_sources.h"

#include <algorithm>
#include <filesystem>

namespace {

/** @brief True if `name` ends with the extension (including its dot). */
bool has_extension(const std::string& name, const std::string& ext) {
    return name.size() > ext.size() &&
           name.compare(name.size() - ext.size(), ext.size(), ext) == 0;
}

/**
 * @brief True if a filename is a format the viewer can render:
 * Markdown (.md, .markdown), Graphviz (.dot), Draw.io (.drawio, .draw.io).
 */
bool is_renderable(const std::string& name) {
    return has_extension(name, ".md") ||
           has_extension(name, ".markdown") ||
           has_extension(name, ".dot") ||
           has_extension(name, ".drawio") ||
           has_extension(name, ".draw.io");
}

}  // namespace

std::vector<std::string> discover_renderable_sources(const std::string& root,
                                                     const std::vector<std::string>& skip_dirs) {
    std::vector<std::string> files;
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec) || ec) {
        return files;
    }

    std::filesystem::recursive_directory_iterator it(root,
        std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        return files;
    }
    std::filesystem::recursive_directory_iterator end;

    for (; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        std::error_code e2;
        const std::filesystem::directory_entry entry = *it;
        if (entry.is_directory(e2)) {
            const std::string name = entry.path().filename().string();
            if (std::find(skip_dirs.begin(), skip_dirs.end(), name) != skip_dirs.end()) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!entry.is_regular_file(e2)) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (is_renderable(name)) {
            files.push_back(entry.path().string());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}
