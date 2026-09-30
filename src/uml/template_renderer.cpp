#include "uml/template_renderer.h"

#include "uml/template.h"
#include <utility>

namespace uml {

std::string TemplateRenderer::render(const Substitutions& subs) const {
    std::string html(kTemplate);
    spliceAll(html, subs);
    return html;
}

void TemplateRenderer::spliceAll(std::string& html, const Substitutions& subs) {
    std::string out;
    out.reserve(html.size());
    size_t i = 0;
    while (i < html.size()) {
        bool matched = false;
        for (const auto& kv : subs) {
            if (html.compare(i, kv.first.size(), kv.first) == 0) {
                out += kv.second;
                i += kv.first.size();
                matched = true;
                break;
            }
        }
        if (!matched) {
            out += html[i];
            ++i;
        }
    }
    html = std::move(out);
}

}  // namespace uml
