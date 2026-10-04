#ifndef RENDERABLE_SOURCES_H
#define RENDERABLE_SOURCES_H

#include <string>
#include <vector>

/**
 * @brief Discover renderable non-code files (Markdown, Graphviz, Draw.io)
 *        under a root directory, recursively.
 *
 * The result feeds the analyzer JSON's root-level "sources" array, which the
 * UML viewer uses to offer a *rendered* view of docs and diagrams (instead of
 * raw text) in the source pane. Only regular files with a known renderable
 * extension are collected; full paths are returned, sorted for a
 * deterministic order across runs and hosts.
 *
 * @param root Root directory to walk
 * @param skip_dirs Directory names to prune (matches the analyzer's skip_dirs)
 * @return Sorted list of renderable file paths (empty when none found)
 */
std::vector<std::string> discover_renderable_sources(const std::string& root,
                                                     const std::vector<std::string>& skip_dirs = {});

#endif // RENDERABLE_SOURCES_H
