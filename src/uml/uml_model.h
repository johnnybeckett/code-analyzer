#ifndef UML_MODEL_H
#define UML_MODEL_H

#include <string>
#include <vector>

#include "uml/template_renderer.h"

namespace uml {

/**
 * @brief The "model" the UML tools serve: analyzer JSON files -> a fully-spliced
 *        self-contained viewer page.
 *
 * Single responsibility: turn a set of analyzer JSON files into the HTML page
 * (including the old/new/diff assembly), independent of *how* it is consumed.
 * Both the file-based UML generator and the HTTP server build the page through
 * this facade, so the splicing logic lives in exactly one place.
 */
class UmlModel {
public:
    /**
     * @brief Construct a model from analyzer JSON files.
     * @param files One file -> a single combined diagram; two files -> diff
     *              mode (files[0] the older baseline, files[1] the newer state).
     * @param hide_patterns Regex patterns accepted for CLI parity with the
     *              generator; NOT applied here (the page filters in-browser).
     */
    UmlModel(std::vector<std::string> files, std::vector<std::string> hide_patterns);

    /**
     * @brief True when two files are present (diff mode).
     */
    bool is_diff() const;

    /**
     * @brief Build the complete, self-contained UML viewer page.
     * @return The template with all class data spliced in.
     * @throws std::runtime_error if any input file is missing, malformed, or
     *         has no "classes" array — a bad input must never be served as an
     *         empty page.
     */
    std::string build_html() const;

    /**
     * @brief The distinct source files the classes in this model were parsed
     *        from, in first-seen (document) order.
     *
     * The union over every input file (files[0] before files[1] in diff mode),
     * deduped. This is the allowlist a server preloads so it can serve the
     * real source of any class on demand. Fail-soft: a missing or malformed
     * input file contributes nothing rather than throwing.
     */
    std::vector<std::string> source_files() const;

private:
    std::vector<std::string> files_;
    // Accepted for CLI parity with the generator, but deliberately not applied
    // here: the page does its own name/regex filtering in the browser.
    std::vector<std::string> hide_patterns_;
    TemplateRenderer renderer_;
};

}  // namespace uml

#endif  // UML_MODEL_H
