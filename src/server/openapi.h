#ifndef OPENAPI_H
#define OPENAPI_H

#include <string>
#include <vector>

#include "server/rest_handler.h"

namespace server {

/**
 * @brief Render the registered endpoints as a valid OpenAPI 3.0.3 document.
 *
 * The output is a `boost::json::object` serialized to a string (auto-escaping
 * of any description text is handled by the serializer). `paths` is keyed by
 * endpoint path; each HTTP verb under it is an operation with a `summary`,
 * `parameters[]` (for query/path params), a `requestBody` (for body params),
 * and a `responses` map (status code -> description). Nothing here is
 * hard-coded: the whole document is derived from each handler's `describe()`.
 *
 * @param endpoints The endpoints to document, in any order (grouped by path).
 * @param title The `info.title` value; an empty string yields a sensible
 *        default ("UML Server API") — single-input mode has no real title.
 */
std::string build_spec(const std::vector<RestEndpoint>& endpoints,
                       const std::string& title);

/**
 * @brief Render the registered endpoints as a compact JSON array.
 *
 * Each entry is `{method, path, summary, params:[{name,in,type,required,
 * description}], responses:[{code, description}]}` — a human- and
 * machine-readable description of the whole API, in the same deterministic
 * order the spec uses.
 */
std::string build_reflection(const std::vector<RestEndpoint>& endpoints);

/**
 * @brief GET /openapi.json — the live OpenAPI 3.0.3 document for this server.
 *
 * The endpoint set is read from the router at request time, so the document
 * always reflects exactly what is registered — including this endpoint and
 * the reflection endpoint, making the API self-describing. The router (owned
 * by the UmlServer) outlives the handler, so the non-owning pointer is safe.
 */
class OpenApiHandler : public RestHandler {
public:
    OpenApiHandler(Router* router, std::string title);

    RestResponse handle(const Request& req) const override;
    RestEndpoint describe() const override;

private:
    Router* router_;
    std::string title_;
};

/**
 * @brief GET /api — a compact description of every registered endpoint.
 *
 * Like OpenApiHandler, it reads the live endpoint set from the router, so it
 * always lists the full API (itself included).
 */
class ReflectionHandler : public RestHandler {
public:
    explicit ReflectionHandler(Router* router);

    RestResponse handle(const Request& req) const override;
    RestEndpoint describe() const override;

private:
    Router* router_;
};

/**
 * @brief Register the two meta endpoints (GET /openapi.json, GET /api) on the
 * router, pointing both at that same router so they document it.
 */
void register_meta_handlers(Router& router, const std::string& title);

}  // namespace server

#endif  // OPENAPI_H
