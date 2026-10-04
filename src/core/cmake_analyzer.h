#ifndef CMAKE_ANALYZER_H
#define CMAKE_ANALYZER_H

#include <string>
#include <vector>

#include "core/cmake_model.h"

/**
 * @brief Parses CMake build files into a CMakeGraph (Strategy).
 *
 * A pure, dependency-free reader of CMakeLists.txt / *.cmake: it discovers
 * those files under a directory (or accepts raw text) and extracts targets
 * (add_library incl. ALIAS and INTERFACE, add_executable), each target's
 * sources (its add_* file list plus any target_sources), and its
 * target_link_libraries dependencies. It understands just enough CMake to
 * build the library dependency graph — it is not a full CMake interpreter.
 *
 * `parse_text()` is exposed deliberately: the extraction logic can be
 * unit-tested against in-memory CMake blobs without writing fixture files to
 * disk (white-box), while `parse_directory()` is the black-box path driven by
 * the composition root for directory-mode analysis.
 */
class CMakeAnalyzer {
public:
    /**
     * @brief Discover and parse every CMakeLists.txt / *.cmake under `root`.
     * @param root      The directory to walk.
     * @param skip_dirs Directory names to prune (not descended into).
     */
    CMakeGraph parse_directory(const std::string& root,
                               const std::vector<std::string>& skip_dirs = {}) const;

    /** @brief Parse a single CMake text blob (used by parse_directory and tests). */
    CMakeGraph parse_text(const std::string& cmake_text) const;
};

#endif // CMAKE_ANALYZER_H
