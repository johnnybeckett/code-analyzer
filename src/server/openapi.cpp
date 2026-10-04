#include "server/openapi.h"

#include <boost/json.hpp>
#include <cctype>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace json = boost::json;

namespace server {

namespace {

// The OpenAPI version this document conforms to.
const char kOpenApiVersion[] = "3.0.3";
// Our own implementation version reported in info.version.
const char kApiVersion[] = "1.0.0";
// A fallback document title for single-input mode (no real title).
const char kDefaultTitle[] = "UML Server API";

// The HTTP verb lower-cased, as the key an operation sits under in OpenAPI
// (e.g. "GET" -> "get"). Only the verbs this server uses appear.
std::string verb_key(const std::string& verb) {
    std::string v = verb;
    for (char& c : v) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return v;
}

// The OpenAPI object describing one parameter (query/path param).
json::object parameter_object(const RestParam& p) {
    json::object param;
    param["name"] = p.name;
    param["in"] = p.in;
    param["required"] = p.required;
    json::object schema;
    schema["type"] = p.type;
    if (!p.description.empty()) {
        schema["description"] = p.description;
    }
    param["schema"] = std::move(schema);
    return param;
}

// The requestBody object for an operation that takes body parameters.
json::object request_body_object(const std::vector<RestParam>& body_params) {
    json::object properties;
    json::array required;
    for (const auto& p : body_params) {
        json::object prop;
        prop["type"] = p.type;
        if (!p.description.empty()) {
            prop["description"] = p.description;
        }
        properties[p.name] = std::move(prop);
        if (p.required) {
            required.push_back(json::value(p.name));
        }
    }

    json::object schema;
    schema["type"] = "object";
    schema["properties"] = std::move(properties);
    if (!required.empty()) {
        schema["required"] = std::move(required);
    }

    json::object media_type;
    media_type["schema"] = std::move(schema);
    json::object content;
    content["application/json"] = std::move(media_type);

    json::object body;
    body["content"] = std::move(content);
    body["required"] = true;
    return body;
}

// The OpenAPI operation object (summary, parameters/requestBody, responses)
// for one (verb, path).
json::object operation_object(const RestEndpoint& e) {
    json::object op;
    if (!e.summary.empty()) {
        op["summary"] = e.summary;
    }

    // Query/path parameters become parameters[].
    json::array params;
    // Body parameters become a single JSON requestBody.
    bool has_body = false;
    for (const auto& p : e.params) {
        if (p.in == "body") {
            has_body = true;
        } else {
            params.push_back(parameter_object(p));
        }
    }
    if (!params.empty()) {
        op["parameters"] = std::move(params);
    }

    if (has_body) {
        std::vector<RestParam> body_params;
        for (const auto& p : e.params) {
            if (p.in == "body") {
                body_params.push_back(p);
            }
        }
        op["requestBody"] = request_body_object(body_params);
    }

    // Responses: map of "code" -> {description}, in declared order.
    json::object responses;
    for (const auto& [code, description] : e.responses) {
        json::object r;
        r["description"] = description;
        responses[std::to_string(code)] = std::move(r);
    }
    op["responses"] = std::move(responses);

    return op;
}

// Group endpoints by path, keeping each path's operations keyed by verb.
json::object paths_object(const std::vector<RestEndpoint>& endpoints) {
    json::object paths;
    for (const auto& e : endpoints) {
        // paths[path] is an object keyed by lower-cased verb.
        if (!paths.contains(e.path)) {
            paths[e.path] = json::object{};
        }
        auto& path_ops = paths[e.path].as_object();
        path_ops[verb_key(e.method)] = operation_object(e);
    }
    return paths;
}

}  // namespace

std::string build_spec(const std::vector<RestEndpoint>& endpoints,
                       const std::string& title) {
    json::object spec;
    spec["openapi"] = kOpenApiVersion;

    json::object info;
    info["title"] = title.empty() ? kDefaultTitle : title;
    info["version"] = kApiVersion;
    spec["info"] = std::move(info);

    spec["paths"] = paths_object(endpoints);

    return json::serialize(spec);
}

std::string build_reflection(const std::vector<RestEndpoint>& endpoints) {
    json::array arr;
    for (const auto& e : endpoints) {
        json::object entry;
        entry["method"] = e.method;
        entry["path"] = e.path;
        entry["summary"] = e.summary;

        json::array params;
        for (const auto& p : e.params) {
            json::object po;
            po["name"] = p.name;
            po["in"] = p.in;
            po["type"] = p.type;
            po["required"] = p.required;
            if (!p.description.empty()) {
                po["description"] = p.description;
            }
            params.push_back(std::move(po));
        }
        entry["params"] = std::move(params);

        json::array responses;
        for (const auto& [code, description] : e.responses) {
            json::object ro;
            ro["code"] = code;
            ro["description"] = description;
            responses.push_back(std::move(ro));
        }
        entry["responses"] = std::move(responses);

        arr.push_back(std::move(entry));
    }
    return json::serialize(arr);
}

OpenApiHandler::OpenApiHandler(Router* router, std::string title)
    : router_(router), title_(std::move(title)) {}

RestResponse OpenApiHandler::handle(const Request&) const {
    RestResponse r;
    r.status = 200;
    r.content_type = "application/json; charset=utf-8";
    // Read the live endpoint set: the document always matches what is
    // registered, and it includes /openapi.json and /api themselves.
    r.body = build_spec(router_->endpoints(), title_);
    return r;
}

RestEndpoint OpenApiHandler::describe() const {
    return RestEndpoint{
        "GET", "/openapi.json", "The OpenAPI 3.0.3 document describing this API",
        {},
        {{200, "The OpenAPI specification"}}};
}

ReflectionHandler::ReflectionHandler(Router* router) : router_(router) {}

RestResponse ReflectionHandler::handle(const Request&) const {
    RestResponse r;
    r.status = 200;
    r.content_type = "application/json; charset=utf-8";
    r.body = build_reflection(router_->endpoints());
    return r;
}

RestEndpoint ReflectionHandler::describe() const {
    return RestEndpoint{
        "GET", "/api", "A compact description of every registered endpoint",
        {},
        {{200, "The endpoint list"}}};
}

void register_meta_handlers(Router& router, const std::string& title) {
    // Both meta handlers point at this same router so they document the full
    // API (themselves included). The router outlives the handlers it owns.
    router.register_handler(std::make_shared<OpenApiHandler>(&router, title));
    router.register_handler(std::make_shared<ReflectionHandler>(&router));
}

}  // namespace server
