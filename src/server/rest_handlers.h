#ifndef REST_HANDLERS_H
#define REST_HANDLERS_H

#include <map>
#include <memory>
#include <string>

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
 * @brief Register the five domain endpoints on the router.
 *
 * Each handler is injected with the shared state it actually uses (the source
 * allowlist, the review store, the document title) and all of it outlives the
 * handler — UmlServer owns the state and the router.
 */
void register_domain_handlers(Router& router,
                              const std::map<std::string, std::string>& sources,
                              std::shared_ptr<ReviewStore> review,
                              const std::string& title);

}  // namespace server

#endif  // REST_HANDLERS_H
