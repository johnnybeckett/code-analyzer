#include "server/rest_handlers.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "server/http_util.h"

namespace json = boost::json;

namespace server {

namespace {

/** @brief A `{"error":...}` JSON response with the given status. */
RestResponse json_error(int status, const std::string& msg) {
    json::object obj;
    obj["error"] = msg;
    RestResponse r;
    r.status = status;
    r.content_type = "application/json; charset=utf-8";
    r.body = json::serialize(obj);
    return r;
}

/** @brief Overview size for /classes/index: cheap far-field, bounded. */
constexpr std::size_t kOverviewSample = 64;

/** @brief The first raw (not percent-decoded) value for `name`, or "". */
const std::string* first_query(const Request& req, const char* name) {
    for (const auto& kv : req.query) {
        if (kv.first == name) {
            return &kv.second;
        }
    }
    return nullptr;
}

}  // namespace

RestResponse SourceHandler::handle(const Request& req) const {
    std::string path;
    if (const std::string* raw = first_query(req, "path")) {
        path = percent_decode(*raw);
    }

    if (sources_ && !path.empty()) {
        const auto it = sources_->find(path);
        if (it != sources_->end()) {
            RestResponse r;
            r.status = 200;
            r.content_type = "text/plain; charset=utf-8";
            r.body = it->second;
            return r;
        }
    }

    RestResponse r;
    r.status = 404;
    r.content_type = "text/plain; charset=utf-8";
    r.body = "source not found\n";
    return r;
}

RestEndpoint SourceHandler::describe() const {
    return RestEndpoint{
        "GET", "/source", "One allowlisted source file, by path",
        {{"path", "query", "string", true, "File path (percent-decoded) as exposed by the server"}},
        {{200, "The file content as plain text"},
         {404, "No such allowlisted source (or path missing)"}}};
}

RestResponse IndexHandler::handle(const Request&) const {
    if (!index_) {
        return json_error(501, "no class index is loaded for this server");
    }

    const ClassIndexBounds b = index_->bounds();
    json::object bounds;
    bounds["min_x"] = b.min_x;
    bounds["max_x"] = b.max_x;
    bounds["min_y"] = b.min_y;
    bounds["max_y"] = b.max_y;
    bounds["min_z"] = b.min_z;
    bounds["max_z"] = b.max_z;

    json::array sample;
    for (const auto& rec : index_->sample(kOverviewSample)) {
        sample.emplace_back(to_json(rec));
    }

    json::object obj;
    obj["count"] = index_->size();
    obj["bounds"] = std::move(bounds);
    obj["cellSize"] = index_->cellSize();
    obj["sample"] = std::move(sample);

    RestResponse r;
    r.status = 200;
    r.content_type = "application/json; charset=utf-8";
    r.body = json::serialize(obj);
    return r;
}

RestEndpoint IndexHandler::describe() const {
    return RestEndpoint{
        "GET", "/classes/index", "Spatial overview of the project's classes",
        {},
        {{200, "The class count, layout bounds, cell size, and a small sample"},
         {501, "No class index is loaded for this server"}}};
}

RestResponse NearestHandler::handle(const Request& req) const {
    auto parse_double = [&req](const char* name) -> std::optional<double> {
        const std::string* raw = first_query(req, name);
        if (raw == nullptr) {
            return std::nullopt;
        }
        try {
            const double v = std::stod(*raw);
            return std::isfinite(v) ? std::optional<double>(v) : std::nullopt;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    };

    const std::optional<double> x = parse_double("x");
    const std::optional<double> y = parse_double("y");
    const std::optional<double> z = parse_double("z");
    if (!x || !y || !z) {
        return json_error(400, "x, y and z are required numeric coordinates");
    }

    const std::string* raw_count = first_query(req, "count");
    if (raw_count == nullptr) {
        return json_error(400, "count is required (positive integer)");
    }
    std::size_t count;
    try {
        count = std::stoul(*raw_count);
    } catch (const std::exception&) {
        return json_error(400, "count must be a positive integer");
    }
    count = std::clamp(count, std::size_t{1}, ClassIndex::kMaxNearest);

    if (!index_) {
        return json_error(501, "no class index is loaded for this server");
    }

    json::array out;
    for (const auto& rec : index_->nearest(count, *x, *y, *z)) {
        out.emplace_back(to_json(rec));
    }

    RestResponse r;
    r.status = 200;
    r.content_type = "application/json; charset=utf-8";
    r.body = json::serialize(out);
    return r;
}

RestEndpoint NearestHandler::describe() const {
    return RestEndpoint{
        "GET", "/classes/near", "The N classes closest to a coordinate (streaming viewer)",
        {{"x", "query", "number", true, "X coordinate in layout space"},
         {"y", "query", "number", true, "Y coordinate in layout space"},
         {"z", "query", "number", true, "Z coordinate in layout space"},
         {"count", "query", "integer", true,
          std::string{"How many classes to return (clamped to [1, "} +
              std::to_string(ClassIndex::kMaxNearest) + "]"}},
        {{200, "The nearest classes, closest first"},
         {400, "Missing or non-numeric x, y, z, or count"},
         {501, "No class index is loaded for this server"}}};
}

RestResponse RenderHandler::handle(const Request& req) const {
    std::string path;
    if (const std::string* raw = first_query(req, "path")) {
        path = percent_decode(*raw);
    }

    if (sources_ && !path.empty()) {
        const auto it = sources_->find(path);
        if (it != sources_->end()) {
            std::string bin, cmd;
            if (ends_with_ci(path, ".dot")) {
                bin = "dot";
                cmd = "dot -Tsvg";
            } else if (ends_with_ci(path, ".drawio") || ends_with_ci(path, ".draw.io")) {
                bin = "drawio";
                cmd = "drawio -x -f svg";
            } else {
                return json_error(400, "unsupported format for rendering: " + path);
            }

            if (!renderer_available(bin)) {
                return json_error(503, "renderer '" + bin + "' not available on this host");
            }

            const std::optional<std::string> svg = run_renderer(cmd, it->second);
            if (!svg) {
                return json_error(502, "renderer '" + bin + "' failed to produce output");
            }

            RestResponse r;
            r.status = 200;
            r.content_type = "image/svg+xml; charset=utf-8";
            r.body = std::move(*svg);
            return r;
        }
    }

    return json_error(404, "no such source");
}

RestEndpoint RenderHandler::describe() const {
    return RestEndpoint{
        "GET", "/render", "Render an allowlisted diagram (.dot, .drawio) to SVG",
        {{"path", "query", "string", true, "Diagram path (percent-decoded) as exposed by the server"}},
        {{200, "The rendered SVG"},
         {400, "Unsupported diagram format"},
         {404, "No such allowlisted source (or path missing)"},
         {502, "The renderer produced no output"},
         {503, "The renderer binary is not installed on this host"}}};
}

RestResponse CommentsGetHandler::handle(const Request&) const {
    RestResponse r;
    r.status = 200;
    r.content_type = "application/json; charset=utf-8";
    r.body = review_ ? review_->to_json() : std::string(R"({"version":1,"comments":[]})");
    return r;
}

RestEndpoint CommentsGetHandler::describe() const {
    return RestEndpoint{
        "GET", "/comments", "The review comments as JSON (empty list when review is disabled)",
        {},
        {{200, "The comments"}}};
}

RestResponse CommentsPostHandler::handle(const Request& req) const {
    if (!review_) {
        return json_error(400, "review comments are not enabled for this server");
    }

    json::value parsed;
    try {
        parsed = json::parse(req.body);
    } catch (const std::exception&) {
        return json_error(400, "body must be a JSON object");
    }
    if (!parsed.is_object()) {
        return json_error(400, "body must be a JSON object");
    }
    const auto& obj = parsed.as_object();

    // text: required string, non-blank after trim.
    const auto text_it = obj.find("text");
    if (text_it == obj.end() || !text_it->value().is_string()) {
        return json_error(400, "'text' must be a non-empty string");
    }
    // Direct-initialize (parens), not copy-init: as_string() yields
    // boost::json::string, which only converts to std::string_view, so
    // `const std::string text = ...as_string()` would be an invalid
    // two-step user conversion.
    const std::string text(text_it->value().as_string());
    if (blank(text)) {
        return json_error(400, "'text' must be non-empty");
    }

    // oldLine/newLine: optional integers >= 0; at least one must be positive.
    long long old_line = 0, new_line = 0;
    if (const auto o = obj.find("oldLine"); o != obj.end()) {
        if (!o->value().is_int64() || o->value().as_int64() < 0) {
            return json_error(400, "'oldLine' must be an integer >= 0");
        }
        old_line = o->value().as_int64();
    }
    if (const auto n = obj.find("newLine"); n != obj.end()) {
        if (!n->value().is_int64() || n->value().as_int64() < 0) {
            return json_error(400, "'newLine' must be an integer >= 0");
        }
        new_line = n->value().as_int64();
    }
    if (old_line == 0 && new_line == 0) {
        return json_error(400, "at least one of oldLine/newLine must be positive");
    }

    std::string file;
    if (const auto f = obj.find("file"); f != obj.end() && f->value().is_string()) {
        file = f->value().as_string();
    }

    ReviewComment comment;
    comment.file = std::move(file);
    comment.old_line = old_line;
    comment.new_line = new_line;
    comment.text = std::move(text);

    try {
        const ReviewComment stored = review_->add(std::move(comment));
        json::object out;
        out["id"] = stored.id;
        out["file"] = stored.file;
        out["oldLine"] = static_cast<std::int64_t>(stored.old_line);
        out["newLine"] = static_cast<std::int64_t>(stored.new_line);
        out["text"] = stored.text;
        out["created"] = stored.created;
        RestResponse r;
        r.status = 201;
        r.content_type = "application/json; charset=utf-8";
        r.body = json::serialize(out);
        return r;
    } catch (const std::exception& e) {
        return json_error(500, std::string("failed to store comment: ") + e.what());
    }
}

RestEndpoint CommentsPostHandler::describe() const {
    return RestEndpoint{
        "POST", "/comments", "Add a review comment",
        {{"text", "body", "string", true, "The comment text (non-blank)"},
         {"file", "body", "string", false, "The file the comment is on (empty = general)"},
         {"oldLine", "body", "integer", false, "Line in the old revision (>= 0)"},
         {"newLine", "body", "integer", false, "Line in the new revision (>= 0)"}},
        {{201, "The stored comment"},
         {400, "Invalid or missing fields (or review disabled)"},
         {500, "Failed to persist the comment"}}};
}

RestResponse ExportReviewHandler::handle(const Request&) const {
    std::vector<ReviewComment> comments = review_ ? review_->list() : std::vector<ReviewComment>{};
    const std::map<std::string, std::string> empty_sources;
    RestResponse r;
    r.status = 200;
    r.content_type = "text/markdown; charset=utf-8";
    r.content_disposition = "attachment; filename=\"review.md\"";
    r.body = build_review_markdown(comments, sources_ ? *sources_ : empty_sources,
                                  title_ ? *title_ : std::string{});
    return r;
}

RestEndpoint ExportReviewHandler::describe() const {
    return RestEndpoint{
        "GET", "/export/review", "The review rendered as a Markdown download",
        {},
        {{200, "The Markdown review"}}};
}

void register_domain_handlers(Router& router,
                              const std::map<std::string, std::string>& sources,
                              std::shared_ptr<ReviewStore> review,
                              const std::string& title,
                              const ClassIndex* classIndex) {
    // The /classes routes come first so they lead GET /api and /openapi.json;
    // both answer 501 when `classIndex` is null.
    router.register_handler(std::make_shared<IndexHandler>(classIndex));
    router.register_handler(std::make_shared<NearestHandler>(classIndex));
    router.register_handler(std::make_shared<SourceHandler>(&sources));
    router.register_handler(std::make_shared<RenderHandler>(&sources));
    router.register_handler(std::make_shared<CommentsGetHandler>(review));
    router.register_handler(std::make_shared<CommentsPostHandler>(review));
    router.register_handler(std::make_shared<ExportReviewHandler>(review, &sources, &title));
}

}  // namespace server
