#ifndef REST_HANDLERS_H
#define REST_HANDLERS_H

#include <map>
#include <memory>
#include <string>

#include "server/class_index.h"
#include "server/rest_handler.h"
#include "server/review_store.h"

namespace server {

/**
 * @brief GET /source?path=... — the raw source of one allowlisted file.
 *
 * The percent-decoded `path` value is looked up exactly in the preloaded
 * source map and never turned into a filesystem path, so the query cannot
 * escape the set of files the caller chose to expose. A miss (or a missing
 * `path`) is a 404.
 */
class SourceHandler : public RestHandler {
public:
    explicit SourceHandler(const std::map<std::string, std::string>* sources)
        : sources_(sources) {}

    RestResponse handle(const Request& req) const override;
    RestEndpoint describe() const override;

private:
    const std::map<std::string, std::string>* sources_;
};

/**
 * @brief GET /classes/index — the spatial overview of a project's classes.
 *
 * Returns the class count, the assigned layout's axis-aligned bounds, the
 * grid cell size, and a small deterministic sample (a cheap far-field
 * overview). Nothing here is filesystem-relative: the index was built from
 * the analyzer's JSON before the server started, so no allowlist is needed.
 * A null index (viewer run without one) is a clean 501.
 */
class IndexHandler : public RestHandler {
public:
    explicit IndexHandler(const ClassIndex* index) : index_(index) {}

    RestResponse handle(const Request& req) const override;
    RestEndpoint describe() const override;

private:
    const ClassIndex* index_;
};

/**
 * @brief GET /classes/near?x=&y=&z=&count=N — the N closest classes.
 *
 * The core of the streaming viewer: instead of shipping every class up front
 * (the RAM problem for a 10k-class project), the client asks for the window
 * around its current view target. `x`/`y`/`z` are required doubles, `count`
 * is clamped to [1, ClassIndex::kMaxNearest]. Missing or non-numeric params
 * are a 400; a null index is a clean 501.
 */
class NearestHandler : public RestHandler {
public:
    explicit NearestHandler(const ClassIndex* index) : index_(index) {}

    RestResponse handle(const Request& req) const override;
    RestEndpoint describe() const override;

private:
    const ClassIndex* index_;
};

/**
 * @brief GET /render?path=... — an allowlisted diagram rendered to SVG.
 *
 * `dot` (graphviz) renders `.dot`; `drawio` renders `.drawio`/`.draw.io`.
 * Same allowlist posture as SourceHandler. A missing binary is a clean 503
 * JSON error so the viewer can fall back to source view.
 */
class RenderHandler : public RestHandler {
public:
    explicit RenderHandler(const std::map<std::string, std::string>* sources)
        : sources_(sources) {}

    RestResponse handle(const Request& req) const override;
    RestEndpoint describe() const override;

private:
    const std::map<std::string, std::string>* sources_;
};

/**
 * @brief GET /comments — the review comments as JSON (an empty list when the
 * store is null, i.e. review is disabled).
 */
class CommentsGetHandler : public RestHandler {
public:
    explicit CommentsGetHandler(std::shared_ptr<ReviewStore> review)
        : review_(std::move(review)) {}

    RestResponse handle(const Request& req) const override;
    RestEndpoint describe() const override;

private:
    std::shared_ptr<ReviewStore> review_;
};

/**
 * @brief POST /comments — validate a comment, store it, echo it (201).
 *
 * Every validation failure is a 400 with a JSON error; a persist failure is
 * a 500. Disabled (null store) is a 400.
 */
class CommentsPostHandler : public RestHandler {
public:
    explicit CommentsPostHandler(std::shared_ptr<ReviewStore> review)
        : review_(std::move(review)) {}

    RestResponse handle(const Request& req) const override;
    RestEndpoint describe() const override;

private:
    std::shared_ptr<ReviewStore> review_;
};

/**
 * @brief GET /export/review — the review rendered as a Markdown download
 * (offered with `Content-Disposition: attachment; filename="review.md"`).
 */
class ExportReviewHandler : public RestHandler {
public:
    ExportReviewHandler(std::shared_ptr<ReviewStore> review,
                        const std::map<std::string, std::string>* sources,
                        const std::string* title)
        : review_(std::move(review)), sources_(sources), title_(title) {}

    RestResponse handle(const Request& req) const override;
    RestEndpoint describe() const override;

private:
    std::shared_ptr<ReviewStore> review_;
    const std::map<std::string, std::string>* sources_;
    const std::string* title_;
};

/**
 * @brief Register the seven domain endpoints on the router.
 *
 * Each handler is injected with the shared state it actually uses (the
 * source allowlist, the review store, the document title, the class index)
 * and all of it outlives the handler — UmlServer owns the state and the
 * router. `classIndex` may be null (the two `/classes` routes then answer
 * 501); the others are unaffected.
 */
void register_domain_handlers(Router& router,
                              const std::map<std::string, std::string>& sources,
                              std::shared_ptr<ReviewStore> review,
                              const std::string& title,
                              const ClassIndex* classIndex);

}  // namespace server

#endif  // REST_HANDLERS_H
