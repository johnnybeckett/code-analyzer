#include "uml/uml_model.h"

#include "uml/class_loader.h"
#include <set>
#include <utility>

namespace uml {

UmlModel::UmlModel(std::vector<std::string> files,
                   std::vector<std::string> hide_patterns)
    : files_(std::move(files)), hide_patterns_(std::move(hide_patterns)) {}

bool UmlModel::is_diff() const {
    return files_.size() >= 2;
}

std::string UmlModel::build_html() const {
    // Splice the real class data into the template.
    //   - single file: one merged class set, diff off
    //   - two files:   old (files[0]) and new (files[1]), diff on
    std::string old_json, new_json;
    const bool diff = is_diff();
    if (diff) {
        old_json = JsonClassLoader::getClassesJSON(
            JsonClassLoader::parseFileClassesStrict(files_[0]));
        new_json = JsonClassLoader::getClassesJSON(
            JsonClassLoader::parseFileClassesStrict(files_[1]));
    } else {
        std::vector<std::string> all;
        for (const auto& f : files_) {
            const auto c = JsonClassLoader::parseFileClassesStrict(f);
            all.insert(all.end(), c.begin(), c.end());
        }
        old_json = "[]";
        new_json = JsonClassLoader::getClassesJSON(all);
    }

    // CMake targets feed the "CMake" layout. Fail-soft: an analyzer JSON with
    // no "cmake" key (old files, or a tree without CMake) splices in "[]",
    // which the page treats as "layout unavailable" and hides the option.
    const std::string cmake_json = JsonClassLoader::parseCmakeMerged(files_);

    return renderer_.render({
        { "__DIFF_MODE__",        diff ? "true" : "false" },
        { "__OLD_CLASSES_JSON__", old_json },
        { "__NEW_CLASSES_JSON__", new_json },
        { "__CMAKE_JSON__",       cmake_json },
    });
}

std::vector<std::string> UmlModel::source_files() const {
    std::vector<std::string> out;
    std::set<std::string> seen;
    for (const auto& f : files_) {
        // Class source files first (the long-standing allowlist), then the
        // renderable non-code files from the "sources" array (Markdown,
        // Graphviz, Draw.io) so the server can serve and render them too.
        for (const auto& src : JsonClassLoader::parseFileSources(f)) {
            if (seen.insert(src).second) {
                out.push_back(src);
            }
        }
        for (const auto& src : JsonClassLoader::parseFileSourcesList(f)) {
            if (seen.insert(src).second) {
                out.push_back(src);
            }
        }
    }
    return out;
}

}  // namespace uml
