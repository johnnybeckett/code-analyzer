#include "core/json_serializer.h"

namespace {

/**
 * @brief Convert a Visibility enum to its string form
 */
const char* visibility_name(Visibility v) {
    switch (v) {
        case Visibility::PUBLIC: return "public";
        case Visibility::PRIVATE: return "private";
        case Visibility::PROTECTED: return "protected";
    }
    return "public";
}

/**
 * @brief Convert a Mutability enum to its string form
 */
const char* mutability_name(Mutability m) {
    switch (m) {
        case Mutability::READ_WRITE: return "read_write";
        case Mutability::READ_ONLY: return "read_only";
        case Mutability::CONST: return "const";
    }
    return "read_write";
}

} // namespace

boost::json::value serialize(const AnalysisResult& result, const std::string& input_path) {
    namespace json = boost::json;
    json::object root;
    root["generator"] = "CodeAnalyzer";
    root["project_path"] = input_path;
    root["class_count"] = result.classes.size();

    json::array classes;
    for (const auto& class_obj : result.classes) {
        json::object cj;
        cj["name"] = class_obj->name;
        cj["kind"] = class_obj->kind;
        cj["namespace"] = class_obj->full_namespace;
        cj["visibility"] = visibility_name(class_obj->visibility);
        cj["static"] = class_obj->is_static;
        cj["file"] = class_obj->file;

        json::array inheritance;
        for (const auto& base : class_obj->inheritance_list) {
            inheritance.emplace_back(base);
        }
        cj["inheritance"] = std::move(inheritance);

        json::array methods;
        for (const auto& method : class_obj->methods) {
            json::object mj;
            mj["name"] = method->name;
            mj["return_type"] = method->return_type;
            mj["visibility"] = visibility_name(method->visibility);
            mj["static"] = method->is_static;
            json::array params;
            for (const auto& param : method->parameters) {
                params.emplace_back(param);
            }
            mj["parameters"] = std::move(params);

            // Callee names referenced in the method body (call-graph edges);
            // empty when the parser could not capture a body.
            json::array calls;
            for (const auto& callee : method->called_methods) {
                calls.emplace_back(callee);
            }
            mj["called_methods"] = std::move(calls);

            methods.emplace_back(std::move(mj));
        }
        cj["methods"] = std::move(methods);

        json::array variables;
        for (const auto& variable : class_obj->variables) {
            json::object vj;
            vj["name"] = variable->name;
            vj["type"] = variable->type;
            vj["mutability"] = mutability_name(variable->mutability);
            vj["visibility"] = visibility_name(variable->visibility);
            variables.emplace_back(std::move(vj));
        }
        cj["variables"] = std::move(variables);

        classes.emplace_back(std::move(cj));
    }
    root["classes"] = std::move(classes);

    // Optional, backward-compatible keys: emitted only when present so a
    // directory without CMake files (or without renderable non-code files)
    // serializes exactly as before.
    if (!result.cmake.empty()) {
        json::array cmake;
        for (const auto& target : result.cmake.targets) {
            json::object tj;
            tj["name"] = target.name;
            tj["kind"] = target.kind;
            if (!target.alias_of.empty()) {
                tj["alias_of"] = target.alias_of;
            }
            json::array sources;
            for (const auto& s : target.sources) {
                sources.emplace_back(s);
            }
            tj["sources"] = std::move(sources);
            json::array links;
            for (const auto& l : target.links) {
                links.emplace_back(l);
            }
            tj["links"] = std::move(links);
            cmake.emplace_back(std::move(tj));
        }
        root["cmake"] = std::move(cmake);
    }

    if (!result.sources.empty()) {
        json::array src;
        for (const auto& s : result.sources) {
            src.emplace_back(s);
        }
        root["sources"] = std::move(src);
    }

    return json::value(std::move(root));
}
