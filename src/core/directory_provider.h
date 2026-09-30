#ifndef DIRECTORY_PROVIDER_H
#define DIRECTORY_PROVIDER_H

#include <string>
#include <vector>

#include "core/source_file_provider.h"

/**
 * @brief Discovers source files by recursively walking a project root directory.
 *
 * This is the provider behind the positional <input_path> and the --directory
 * flag. Sub-directories named in skip_dirs are pruned (not descended into);
 * only regular files are returned, in filesystem-walk order — the same order
 * the Analyzer always produced.
 */
class DirectoryFileProvider final : public SourceFileProvider {
public:
    DirectoryFileProvider(std::string root, std::vector<std::string> skip_dirs);

    std::vector<std::string> files() const override;
    std::string banner() const override;

private:
    std::string root_;
    std::vector<std::string> skip_dirs_;
};

#endif // DIRECTORY_PROVIDER_H
