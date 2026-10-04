#ifndef CMAKE_MODEL_H
#define CMAKE_MODEL_H

#include <string>
#include <vector>

/**
 * @brief One CMake target as discovered in the analyzed directory.
 *
 * Pure data describing a single build entry. `kind` is one of:
 *   "library"    — add_library (STATIC / SHARED / MODULE / OBJECT)
 *   "executable" — add_executable
 *   "interface"  — add_library(... INTERFACE)
 *   "alias"      — add_library(<name> ALIAS <real>); `alias_of` names the real
 *                  target, and `sources` / `links` are left empty
 *
 * `sources` lists the files the target compiles (from its add_* file list and
 * any target_sources). `links` holds the dependencies named by
 * target_link_libraries (keyword tags and generator expressions already
 * stripped); the viewer draws an edge only when the name is itself a target.
 */
struct CMakeTarget {
    std::string name;
    std::string kind;
    std::string alias_of;
    std::vector<std::string> sources;
    std::vector<std::string> links;
};

/**
 * @brief The full set of CMake targets found in a directory.
 *
 * Edges are derived from each target's `links` (a link is an edge iff the
 * named target exists here), so the graph is simply the ordered target list.
 */
struct CMakeGraph {
    std::vector<CMakeTarget> targets;

    bool empty() const { return targets.empty(); }
};

#endif // CMAKE_MODEL_H
