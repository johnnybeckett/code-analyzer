#ifndef REST_HANDLER_H
#define REST_HANDLER_H

#include <cctype>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace server {

/**
 * @brief One parameter an endpoint expects (drives the OpenAPI spec).
 *
 * `in` is where the parameter arrives: "query", "body", or "path". `type` is
 * the OpenAPI schema type ("string", "integer", …). `required` mirrors the
 * endpoint's validation (a missing one 400s).
 */
struct RestParam {
    std::string name;
    std::string in;
    std::string type;
    bool required = false;
    std::string description;
};

/**
 * @brief The description of one registered endpoint: how to call it, what it
 * expects, and what it can return.
 *
 * `method` is the HTTP verb (stored uppercase: "GET", "POST", …). `path` is
 * the exact route (e.g. "/source", "/export/review"). `responses` lists the
 * status codes the endpoint can return, with a human-readable reason for each.
 */
struct RestEndpoint {
    std::string method;
    std::string path;
    std::string summary;
    std::vector<RestParam> params;
    std::vector<std::pair<int, std::string>> responses;
};

/**
 * @brief A normalized incoming request: the route path, its (raw, not
 * percent-decoded) query pairs in order, and the raw body.
 *
 * The query pairs come from parse_query(): each (name, value) split on the
 * segment's first '='. Handlers percent-decode the individual value they need.
 */
struct Request {
    std::string path;
    std::vector<std::pair<std::string, std::string>> query;
    std::string body;
};

/**
 * @brief The (fully staged) answer a handler returns.
 *
 * `content_disposition` is set only where a download is offered
 * (/export/review); leave it unset otherwise.
 */
struct RestResponse {
    int status = 200;
    std::string content_type;
    std::optional<std::string> content_disposition;
    std::string body;
};

/**
 * @brief A generic REST endpoint: convert a normalized Request into a
 * RestResponse, and describe itself (for the spec/reflection endpoints).
 *
 * Handlers are pure `Request → RestResponse` — no socket, no Session, no
 * Asio — so they are unit-testable with no server running and can live in any
 * translation unit. Shared state (the source allowlist, the review store, the
 * document title) is injected by reference/pointer at construction and outlives
 * the handler (owned by the UmlServer that registers it).
 */
class RestHandler {
public:
    virtual ~RestHandler() = default;

    /** @brief Handle the request and return the complete response to send. */
    virtual RestResponse handle(const Request& req) const = 0;

    /** @brief Describe this endpoint (method, path, params, responses). */
    virtual RestEndpoint describe() const = 0;
};

/**
 * @brief The registry of endpoints: a map of request type (verb) to a
 * sub-map of endpoint (path) to handler — the plugin/multiplexer core.
 *
 * Adding a route is now "write a RestHandler, register it" — dispatch
 * (UmlServer::route) and the OpenAPI/reflection output pick it up with no
 * further wiring. Order is deterministic (maps sorted by verb, then path),
 * which is the order the spec and reflection emit.
 */
class Router {
public:
    /**
     * @brief Register a handler under the (verb, path) it declares in
     * describe(). A second handler for the same key replaces the first.
     */
    void register_handler(std::shared_ptr<RestHandler> handler) {
        const RestEndpoint e = handler->describe();
        routes_[upper_verb(e.method)][e.path] = std::move(handler);
    }

    /**
     * @brief The handler for (verb, path), or nullptr when no route matches.
     * Verb matching is case-insensitive; the path must match exactly.
     */
    std::shared_ptr<RestHandler> route(const std::string& verb, const std::string& path) const {
        const auto v = routes_.find(upper_verb(verb));
        if (v == routes_.end()) {
            return nullptr;
        }
        const auto it = v->second.find(path);
        return it == v->second.end() ? nullptr : it->second;
    }

    /**
     * @brief Every registered endpoint in deterministic order (verb, then
     * path) — the input to the OpenAPI spec and the reflection endpoint.
     */
    std::vector<RestEndpoint> endpoints() const {
        std::vector<RestEndpoint> out;
        for (const auto& [verb, by_path] : routes_) {
            for (const auto& [path, handler] : by_path) {
                out.push_back(handler->describe());
            }
        }
        return out;
    }

    std::size_t size() const {
        std::size_t n = 0;
        for (const auto& [verb, by_path] : routes_) {
            n += by_path.size();
        }
        return n;
    }

private:
    static std::string upper_verb(std::string v) {
        for (char& c : v) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return v;
    }

    std::map<std::string, std::map<std::string, std::shared_ptr<RestHandler>>> routes_;
};

}  // namespace server

#endif  // REST_HANDLER_H
