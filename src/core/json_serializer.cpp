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
    return json::value(std::move(root));
}
