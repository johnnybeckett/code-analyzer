#include "server/uml_server.h"

#include <boost/beast.hpp>
#include <cstddef>
#include <iostream>
#include <map>

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
            const std::map<std::string, std::string>* sources)
        : socket_(std::move(socket)), body_(std::move(body)), sources_(sources) {}

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

    // True when the request path (ignoring any query string) is exactly /source.
    bool is_source() const {
        const std::string target(req_.target());
        const std::size_t q = target.find('?');
        const std::string path = (q == std::string::npos) ? target : target.substr(0, q);
        return path == "/source";
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
        const bool is_get = req_.method() == http::verb::get;
        res_.version(req_.version());
        res_.set(http::field::server, "umlsrv");
        if (!is_get) {
            res_.result(http::status::method_not_allowed);
            res_.set(http::field::content_type, "text/plain; charset=utf-8");
            res_.body() = "Method not allowed: use GET.\n";
        } else if (is_source()) {
            serve_source();
        } else {
            res_.result(http::status::ok);
            res_.set(http::field::content_type, "text/html; charset=utf-8");
            res_.body() = body_;
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
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
};

}  // namespace

UmlServer::UmlServer(std::string body,
                     std::map<std::string, std::string> sources,
                     std::uint16_t port)
    : body_(std::move(body)), sources_(std::move(sources)), acceptor_(io_) {
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
                auto* session = new Session(std::move(socket), body_, &sources_);
                session->start();
                do_accept();  // re-arm only on success; a stopped io_context
            }
            // else: we were stopped (or a transient error) — do NOT re-arm, so the
            // accept loop terminates cleanly instead of recursing.
        });
}

}  // namespace server
