#ifndef UML_TEMPLATE_RENDERER_H
#define UML_TEMPLATE_RENDERER_H

#include <string>
#include <utility>
#include <vector>

namespace uml {

/**
 * @brief Splices placeholder tokens into the embedded UML page template
 */
class TemplateRenderer {
public:
    using Substitutions = std::vector<std::pair<std::string, std::string>>;

    /**
     * @brief Render the complete UML page
     * @param subs Placeholder token → replacement value pairs
     * @return The template with every token spliced in
     */
    std::string render(const Substitutions& subs) const;

    /**
     * @brief Replace all placeholders with their values in a single pass
     *
     * Scans the template left to right. When a placeholder token is hit, its
     * value is emitted verbatim and the scan continues *past* the token — the
     * emitted value is never re-scanned. This makes the splice independent of
     * substitution order and immune to a class's name/type literally
     * containing a placeholder token (e.g. a method named
     * "__NEW_CLASSES_JSON__"), which the old find/replace-once sequence could
     * corrupt by matching the token inside already-injected data.
     *
     * @param html   Document to modify in place
     * @param subs   Placeholder token → replacement value pairs
     */
    static void spliceAll(std::string& html, const Substitutions& subs);
};

}  // namespace uml

#endif  // UML_TEMPLATE_RENDERER_H
