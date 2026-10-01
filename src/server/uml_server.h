#ifndef UML_SERVER_H
#define UML_SERVER_H

#include <boost/asio.hpp>

#include <cstdint>
#include <string>

namespace server {

/**
 * @brief A minimal Boost.Beast HTTP server that serves the UML model page.
 *
 * Single responsibility: serve the fully-built UML viewer HTML (received as an
 * opaque body string) over HTTP — any path, every GET — and nothing else. It
 * knows nothing about *how* the page was built (that is UmlModel's job); it
 * simply returns the bytes it was handed. This keeps the server reusable and
 * isolates all the HTTP/Beast detail in one place.
 *
 * It runs a single-threaded, async io_context that can serve many concurrent
 * clients, but each connection is deliberately simple request/response with
 * `Connection: close` (no keep-alive, no chunking).
 */
class UmlServer {
public:
    /**
     * @brief Bind and prepare the listener.
     * @param body The complete HTML page to serve (built by UmlModel).
     * @param port TCP port to bind on 0.0.0.0; 0 lets the OS pick an ephemeral
     *             port (read it back via local_port() — used by the tests).
     *
     * The listener is opened/bound/listened immediately so local_port() is valid
     * before run().
     * @throws boost::system::system_error if the bind or listen fails (e.g. the
     *         port is already in use), so a bad port is reported up front rather
     *         than mid-run.
     */
    UmlServer(std::string body, std::uint16_t port);

    ~UmlServer();

    UmlServer(const UmlServer&) = delete;
    UmlServer& operator=(const UmlServer&) = delete;

    /**
     * @brief Run the accept loop and io_context until stop() is called.
     * @return True when the server ran and exited via stop().
     */
    bool run();

    /** @brief Stop the io_context (called by the signal handler to shut down). */
    void stop();

    /** @brief The port actually bound (the OS-assigned one when built with 0). */
    std::uint16_t local_port() const;

    /** @brief The io_context, so the caller can attach a signal_set. */
    boost::asio::io_context& io();

private:
    void do_accept();

    std::string body_;
    boost::asio::io_context io_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::uint16_t assigned_port_{0};
};

}  // namespace server

#endif  // UML_SERVER_H
