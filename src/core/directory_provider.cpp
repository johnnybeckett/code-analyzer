#include "core/directory_provider.h"

#include <algorithm>
#include <filesystem>

DirectoryFileProvider::DirectoryFileProvider(std::string root,
                                             std::vector<std::string> skip_dirs)
    : root_(std::move(root)), skip_dirs_(std::move(skip_dirs)) {}

std::vector<std::string> DirectoryFileProvider::files() const {
    std::vector<std::string> files;
    const std::vector<std::string>& skip_dirs = skip_dirs_;

    std::filesystem::recursive_directory_iterator it(
        root_, std::filesystem::directory_options::skip_permission_denied);
    std::filesystem::recursive_directory_iterator end;

    for (; it != end; ++it) {
        const std::filesystem::directory_entry entry = *it;

        // Prune a skip-listed directory before descending into it
        if (entry.is_directory() &&
            std::find(skip_dirs.begin(), skip_dirs.end(),
                      entry.path().filename().string()) != skip_dirs.end()) {
            it.disable_recursion_pending();
            continue;
        }

        if (!entry.is_regular_file()) continue;

        files.push_back(entry.path().string());
    }

    return files;
}

std::string DirectoryFileProvider::banner() const {
    return "Analyzing project: " + root_;
}
