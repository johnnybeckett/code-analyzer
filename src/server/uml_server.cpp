#include "server/uml_server.h"

#include <boost/beast.hpp>
#include <iostream>

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
    Session(tcp::socket socket, std::string body)
        : socket_(std::move(socket)), body_(std::move(body)) {}

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

    void do_write() {
        // res_ is a member (not a local) because async_write is asynchronous:
        // it must outlive the do_write() frame or the in-flight serializer would
        // dereference a destroyed response. req_ is a member for the same reason.
        const bool is_get = req_.method() == http::verb::get;
        res_.version(req_.version());
        res_.result(is_get ? http::status::ok : http::status::method_not_allowed);
        res_.set(http::field::server, "umlsrv");
        if (is_get) {
            res_.set(http::field::content_type, "text/html; charset=utf-8");
            res_.body() = body_;
        } else {
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
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
};

}  // namespace

UmlServer::UmlServer(std::string body, std::uint16_t port)
    : body_(std::move(body)), acceptor_(io_) {
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
                auto* session = new Session(std::move(socket), body_);
                session->start();
                do_accept();  // re-arm only on success; a stopped io_context
            }
            // else: we were stopped (or a transient error) — do NOT re-arm, so the
            // accept loop terminates cleanly instead of recursing.
        });
}

}  // namespace server
