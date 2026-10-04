#include "server/uml_server.h"

#include <boost/beast.hpp>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "server/http_util.h"
#include "server/openapi.h"
#include "server/rest_handlers.h"

namespace server {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace {

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
    Session(tcp::socket socket, std::string body, Router* router)
        : socket_(std::move(socket)), body_(std::move(body)), router_(router) {}

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

    // The raw query string (the target after '?'), for parse_query(). Empty
    // when the request has no query part.
    std::string raw_query() const {
        const std::string target(req_.target());
        const std::size_t q = target.find('?');
        return (q == std::string::npos) ? std::string{} : target.substr(q + 1);
    }

    // The verb as a plain string — the key the Router is built under.
    static std::string verb_name(http::verb v) {
        switch (v) {
            case http::verb::get: return "GET";
            case http::verb::post: return "POST";
            case http::verb::head: return "HEAD";
            case http::verb::put: return "PUT";
            case http::verb::delete_: return "DELETE";
            case http::verb::patch: return "PATCH";
            case http::verb::options: return "OPTIONS";
            default: return "OTHER";
        }
    }

    void do_write() {
        // res_ is a member (not a local) because async_write is asynchronous:
        // it must outlive the do_write() frame or the in-flight serializer would
        // dereference a destroyed response. req_ is a member for the same reason.
        res_.version(req_.version());
        res_.set(http::field::server, "umlsrv");

        // Convert the wire request into the normalized form handlers take:
        // (path, raw query pairs, body) — the "endpoint and variables".
        Request req;
        req.path = request_path();
        req.query = parse_query(raw_query());
        req.body = req_.body();

        const std::string verb = verb_name(req_.method());
        if (const auto handler = router_->route(verb, req.path)) {
            const RestResponse r = handler->handle(req);
            res_.result(static_cast<http::status>(r.status));
            res_.set(http::field::content_type, r.content_type);
            if (r.content_disposition) {
                res_.set(http::field::content_disposition, *r.content_disposition);
            }
            res_.body() = r.body;
        } else if (verb == "GET") {
            // Unmatched GET: the spliced viewer page.
            res_.result(http::status::ok);
            res_.set(http::field::content_type, "text/html; charset=utf-8");
            res_.body() = body_;
        } else {
            // A verb/path no handler registered (HEAD and every other verb).
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
    Router* router_;
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

    // The plugin registry: every endpoint registers itself (verb, path) here,
    // holding references to the shared state above. The domain endpoints come
    // first; then the two meta endpoints (GET /openapi.json, GET /api), which
    // point back at this same router so they document the full API — themselves
    // included. The router is destroyed before the state it holds pointers to
    // (see the member-order note in the header), so no handler outlives state.
    register_domain_handlers(router_, sources_, review_, title_);
    register_meta_handlers(router_, title_);
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
                auto* session = new Session(std::move(socket), body_, &router_);
                session->start();
                do_accept();  // re-arm only on success; a stopped io_context
            }
            // else: we were stopped (or a transient error) — do NOT re-arm, so the
            // accept loop terminates cleanly instead of recursing.
        });
}

}  // namespace server
