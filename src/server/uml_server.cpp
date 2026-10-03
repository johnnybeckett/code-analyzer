#include "server/uml_server.h"

#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <algorithm>
#include <cstddef>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace json = boost::json;

namespace server {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace {

/** @brief The value of a hex digit, or -1 if not one. */
int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * @brief Decode %-escapes (%XX) in a query value.
 *
 * Only hex escape sequences are decoded; everything else — including `+`, which
 * the client never emits because it uses encodeURIComponent — is passed through
 * unchanged. A malformed escape (no two hex digits) is left as-is rather than
 * dropped, so a lookup simply fails to 404 instead of mangling the key.
 */
std::string percent_decode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        if (c == '%' && i + 2 < in.size()) {
            const int hi = hex_val(in[i + 1]);
            const int lo = hex_val(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(c);
    }
    return out;
}

/**
 * @brief The first `name=` value in a `&`-separated query string, or "".
 *
 * The key must begin the query or immediately follow `&`, so a longer param
 * that merely contains `name=` (e.g. `xpath=` for `path`) is not mistaken for
 * a match.
 */
std::string query_param(const std::string& query, const std::string& name) {
    const std::string key = name + "=";
    std::size_t pos = 0;
    while ((pos = query.find(key, pos)) != std::string::npos) {
        if (pos == 0 || query[pos - 1] == '&') {
            const std::size_t value_start = pos + key.size();
            std::size_t value_end = query.find('&', value_start);
            if (value_end == std::string::npos) value_end = query.size();
            return query.substr(value_start, value_end - value_start);
        }
        ++pos;
    }
    return {};
}

/** @brief True when `s` is empty or whitespace-only. */
bool blank(const std::string& s) {
    for (char c : s) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\v' && c != '\f') {
            return false;
        }
    }
    return true;
}

/** @brief A UTC `YYYY-MM-DDTHH:MM:SSZ` timestamp for the current instant. */
std::string utc_now() {
    const std::time_t now = std::time(nullptr);
    const std::tm* gm = std::gmtime(&now);
    if (gm == nullptr) {
        return {};
    }
    char buf[32] = {0};
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gm) == 0) {
        return {};
    }
    return buf;
}

/** @brief Split `s` on newlines (stripping a trailing CR), in order. */
void split_lines(const std::string& s, std::vector<std::string>& out) {
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = s.find('\n', start);
        std::string line = (nl == std::string::npos) ? s.substr(start)
                                                      : s.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        out.push_back(std::move(line));
        if (nl == std::string::npos) {
            break;
        }
        start = nl + 1;
    }
}

/** @brief The `(old L<n>)` / `(added line)` / `(removed line)` annotation. */
std::string line_annotation(const ReviewComment& c) {
    if (c.new_line > 0 && c.old_line > 0) {
        return "(old L" + std::to_string(c.old_line) + ")";
    }
    if (c.new_line > 0) {
        return "(added line)";
    }
    return "(removed line)";  // old_line > 0, new_line == 0
}

/**
 * @brief Render one comment (line header + quoted source + the comment block)
 *        as Markdown. `lines` is the source file already split into lines (may
 *        be empty when the file isn't in the allowlist).
 */
std::string comment_md(const ReviewComment& c, const std::vector<std::string>& lines) {
    std::ostringstream os;
    const long long primary = c.new_line > 0 ? c.new_line : c.old_line;
    os << "**L" << primary << "** " << line_annotation(c) << " — ";

    if (primary >= 1 && static_cast<std::size_t>(primary) <= lines.size()) {
        const std::string& quoted = lines[primary - 1];
        if (quoted.find('`') == std::string::npos) {
            os << "`" << quoted << "`";
        } else {
            // A backtick in the source would break an inline-code span, so fall
            // back to an indented (4-space) code line.
            os << "\n    " << quoted;
        }
    } else {
        os << "_source not available_";
    }

    os << "\n";
    // The comment text, each line as a blockquote, then a blank "> " line and
    // the creation stamp.
    {
        std::vector<std::string> text_lines;
        split_lines(c.text, text_lines);
        for (const auto& tl : text_lines) {
            os << "> " << tl << "\n";
        }
    }
    os << ">\n> _created " << c.created << "_\n";
    return os.str();
}

/**
 * @brief Build the full Markdown review document.
 *
 * Files appear in first-seen comment order; within a file, comments are ordered
 * by primary line (newLine if present, else oldLine). Quoted source lines come
 * from the `sources` allowlist (never the filesystem).
 */
std::string build_review_markdown(const std::vector<ReviewComment>& comments,
                                  const std::map<std::string, std::string>& sources,
                                  const std::string& title) {
    std::ostringstream os;
    os << "# Code Review";
    if (!title.empty()) {
        os << " — " << title;
    }
    os << "\n\n";
    os << "_Generated " << utc_now() << " · " << comments.size() << " comment(s)._\n\n";

    if (comments.empty()) {
        os << "No review comments.\n";
        return os.str();
    }

    // First-seen file order.
    std::vector<std::string> file_order;
    for (const auto& c : comments) {
        if (std::find(file_order.begin(), file_order.end(), c.file) == file_order.end()) {
            file_order.push_back(c.file);
        }
    }

    // Pre-split the source of each file we quote (only those that appear).
    std::map<std::string, std::vector<std::string>> line_cache;
    auto lines_for = [&](const std::string& file) -> const std::vector<std::string>& {
        auto it = line_cache.find(file);
        if (it != line_cache.end()) {
            return it->second;
        }
        std::vector<std::string> lines;
        if (const auto s = sources.find(file); s != sources.end()) {
            split_lines(s->second, lines);
        }
        return line_cache.emplace(file, std::move(lines)).first->second;
    };

    for (const auto& file : file_order) {
        std::vector<const ReviewComment*> rows;
        for (const auto& c : comments) {
            if (c.file == file) {
                rows.push_back(&c);
            }
        }
        std::sort(rows.begin(), rows.end(), [](const ReviewComment* a, const ReviewComment* b) {
            const long long pa = a->new_line > 0 ? a->new_line : a->old_line;
            const long long pb = b->new_line > 0 ? b->new_line : b->old_line;
            return pa < pb;
        });

        os << "## " << file << "\n\n";
        const std::vector<std::string>& lines = lines_for(file);
        for (const auto* c : rows) {
            os << comment_md(*c, lines) << "\n";
        }
    }
    return os.str();
}

/**
 * @brief One HTTP connection: read a single request, answer it, close.
 *
 * Heap-owned and destroyed from its own terminal handler. Because everything runs
 * on a single-threaded io_context, a raw `this` capture in the async handlers is
 * safe: we `delete this` exactly once, when the last operation for this
 * connection finishes.
 */
class Session {
public:
    Session(tcp::socket socket, std::string body,
            const std::map<std::string, std::string>* sources,
            std::shared_ptr<ReviewStore> review, std::string title)
        : socket_(std::move(socket)), body_(std::move(body)), sources_(sources),
          review_(std::move(review)), title_(std::move(title)) {}

    void start() { do_read(); }

private:
    void do_read() {
        req_.clear();
        buffer_.consume(buffer_.size());
        http::async_read(
            socket_, buffer_, req_,
            [this](beast::error_code ec, std::size_t bytes) { on_read(ec, bytes); });
    }

    void on_read(beast::error_code ec, std::size_t) {
        if (ec == beast::error_code{}) {
            do_write();
            return;
        }
        do_close();
    }

    // The request target with any query string stripped (e.g. "/source",
    // "/comments", "/export/review"). Dispatch keys off this exact path.
    std::string request_path() const {
        const std::string target(req_.target());
        const std::size_t q = target.find('?');
        return (q == std::string::npos) ? target : target.substr(0, q);
    }

    // Stage a JSON `{"error":...}` response with the given status.
    void json_error(http::status st, const std::string& msg) {
        json::object obj;
        obj["error"] = msg;
        res_.result(st);
        res_.set(http::field::content_type, "application/json; charset=utf-8");
        res_.body() = json::serialize(obj);
    }

    // GET /comments -> the review store as JSON (an empty list when disabled).
    void serve_comments() {
        res_.result(http::status::ok);
        res_.set(http::field::content_type, "application/json; charset=utf-8");
        res_.body() = review_ ? review_->to_json() : std::string(R"({"version":1,"comments":[]})");
    }

    // POST /comments -> validate, store, and echo the stored comment (201).
    // Every failure is a 400 with a JSON error; a persist failure is a 500.
    void submit_comment() {
        if (!review_) {
            json_error(http::status::bad_request,
                       "review comments are not enabled for this server");
            return;
        }

        json::value parsed;
        try {
            parsed = json::parse(req_.body());
        } catch (const std::exception&) {
            json_error(http::status::bad_request, "body must be a JSON object");
            return;
        }
        if (!parsed.is_object()) {
            json_error(http::status::bad_request, "body must be a JSON object");
            return;
        }
        const auto& obj = parsed.as_object();

        // text: required string, non-blank after trim.
        const auto text_it = obj.find("text");
        if (text_it == obj.end() || !text_it->value().is_string()) {
            json_error(http::status::bad_request, "'text' must be a non-empty string");
            return;
        }
        // Direct-initialize (parens), not copy-init: as_string() yields
        // boost::json::string, which only converts to std::string_view, so
        // `const std::string text = ...as_string()` would be an invalid
        // two-step user conversion.
        const std::string text(text_it->value().as_string());
        if (blank(text)) {
            json_error(http::status::bad_request, "'text' must be non-empty");
            return;
        }

        // oldLine/newLine: optional integers >= 0; at least one must be positive.
        long long old_line = 0, new_line = 0;
        if (const auto o = obj.find("oldLine"); o != obj.end()) {
            if (!o->value().is_int64() || o->value().as_int64() < 0) {
                json_error(http::status::bad_request, "'oldLine' must be an integer >= 0");
                return;
            }
            old_line = o->value().as_int64();
        }
        if (const auto n = obj.find("newLine"); n != obj.end()) {
            if (!n->value().is_int64() || n->value().as_int64() < 0) {
                json_error(http::status::bad_request, "'newLine' must be an integer >= 0");
                return;
            }
            new_line = n->value().as_int64();
        }
        if (old_line == 0 && new_line == 0) {
            json_error(http::status::bad_request,
                       "at least one of oldLine/newLine must be positive");
            return;
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
            res_.result(http::status::created);
            res_.set(http::field::content_type, "application/json; charset=utf-8");
            res_.body() = json::serialize(out);
        } catch (const std::exception& e) {
            json_error(http::status::internal_server_error,
                       std::string("failed to store comment: ") + e.what());
        }
    }

    // GET /export/review -> the review as a Markdown file download.
    void serve_export_review() {
        std::vector<ReviewComment> comments = review_ ? review_->list() : std::vector<ReviewComment>{};
        const std::map<std::string, std::string> empty_sources;
        res_.result(http::status::ok);
        res_.set(http::field::content_type, "text/markdown; charset=utf-8");
        res_.set(http::field::content_disposition, "attachment; filename=\"review.md\"");
        res_.body() = build_review_markdown(comments, sources_ ? *sources_ : empty_sources, title_);
    }

    // Look up the percent-decoded path= value in the allowlist and, if present,
    // stage its content on res_; otherwise stage a 404. Never builds a path.
    void serve_source() {
        const std::string target(req_.target());
        const std::size_t q = target.find('?');
        const std::string query = (q == std::string::npos) ? "" : target.substr(q + 1);
        const std::string path = percent_decode(query_param(query, "path"));

        if (sources_ && !path.empty()) {
            const auto it = sources_->find(path);
            if (it != sources_->end()) {
                res_.result(http::status::ok);
                res_.set(http::field::content_type, "text/plain; charset=utf-8");
                res_.body() = it->second;
                return;
            }
        }

        res_.result(http::status::not_found);
        res_.set(http::field::content_type, "text/plain; charset=utf-8");
        res_.body() = "source not found\n";
    }

    void do_write() {
        // res_ is a member (not a local) because async_write is asynchronous:
        // it must outlive the do_write() frame or the in-flight serializer would
        // dereference a destroyed response. req_ is a member for the same reason.
        res_.version(req_.version());
        res_.set(http::field::server, "umlsrv");

        const http::verb method = req_.method();
        const std::string path = request_path();
        if (method == http::verb::post && path == "/comments") {
            submit_comment();
        } else if (method == http::verb::get) {
            if (path == "/source") {
                serve_source();
            } else if (path == "/comments") {
                serve_comments();
            } else if (path == "/export/review") {
                serve_export_review();
            } else {
                res_.result(http::status::ok);
                res_.set(http::field::content_type, "text/html; charset=utf-8");
                res_.body() = body_;
            }
        } else {
            // HEAD and every other verb on any route: the model is GET-only.
            res_.result(http::status::method_not_allowed);
            res_.set(http::field::content_type, "text/plain; charset=utf-8");
            res_.body() = "Method not allowed: use GET.\n";
        }

        res_.keep_alive(false);  // Connection: close
        res_.set(http::field::content_length, std::to_string(res_.body().size()));

        http::async_write(
            socket_, res_,
            [this](beast::error_code ec, std::size_t bytes) { on_write(ec, bytes); });
    }

    void on_write(beast::error_code ec, std::size_t) {
        if (ec != beast::error_code{}) {
            std::cerr << "UmlServer: write failed: " << ec.message() << "\n";
        }
        do_close();
    }

    void do_close() {
        beast::error_code ec;
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
        delete this;  // terminal; last operation for this connection
    }

    tcp::socket socket_;
    std::string body_;
    const std::map<std::string, std::string>* sources_;
    std::shared_ptr<ReviewStore> review_;
    std::string title_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
};

}  // namespace

UmlServer::UmlServer(std::string body,
                     std::map<std::string, std::string> sources,
                     std::uint16_t port,
                     std::shared_ptr<ReviewStore> review,
                     std::string title)
    : body_(std::move(body)), sources_(std::move(sources)), review_(std::move(review)),
      title_(std::move(title)), acceptor_(io_) {
    tcp::endpoint endpoint(tcp::v4(), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(net::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    assigned_port_ = acceptor_.local_endpoint().port();
}

UmlServer::~UmlServer() = default;

bool UmlServer::run() {
    do_accept();
    io_.run();
    return true;
}

void UmlServer::stop() {
    io_.stop();
}

std::uint16_t UmlServer::local_port() const {
    return assigned_port_;
}

net::io_context& UmlServer::io() {
    return io_;
}

void UmlServer::do_accept() {
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            if (ec == beast::error_code{}) {
                auto* session = new Session(std::move(socket), body_, &sources_,
                                            review_, title_);
                session->start();
                do_accept();  // re-arm only on success; a stopped io_context
            }
            // else: we were stopped (or a transient error) — do NOT re-arm, so the
            // accept loop terminates cleanly instead of recursing.
        });
}

}  // namespace server
