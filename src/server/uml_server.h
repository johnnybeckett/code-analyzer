#ifndef UML_SERVER_H
#define UML_SERVER_H

#include <boost/asio.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "server/review_store.h"

namespace server {

/**
 * @brief A minimal Boost.Beast HTTP server that serves the UML model page.
 *
 * Serves the fully-built UML viewer HTML (received as an opaque body string)
 * on any unmatched GET, plus a small set of routes:
 *   - `GET  /source?path=...` — the raw source of a class, looked up exactly
 *     (percent-decoded) against the allowlist of source files it was handed.
 *   - `GET  /comments` — the review comments as JSON (empty list if the store
 *     is null, i.e. single-input mode).
 *   - `POST /comments` — add a review comment (201, or 400/500 on error).
 *   - `GET  /export/review` — the review rendered as a Markdown download.
 *
 * It knows nothing about *how* the page was built (that is UmlModel's job); it
 * simply returns the bytes it was handed. This keeps the server reusable and
 * isolates all the HTTP/Beast detail in one place.
 *
 * The `/source` route is an allowlist, not a filesystem: `path` is matched
 * against the preloaded source map, never used to build a path, so the query
 * cannot escape the set of files the caller chose to expose. The review routes
 * operate on the shared `ReviewStore` (owned via a `shared_ptr`) and the same
 * preloaded source map — they never read the filesystem themselves.
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
     * @param sources Allowlisted class source files (path -> content) that
     *        `GET /source?path=...` may return. Only exact matches of a key are
     *        served; everything else on that route is a 404.
     * @param port TCP port to bind on 0.0.0.0; 0 lets the OS pick an ephemeral
     *             port (read it back via local_port() — used by the tests).
     * @param review Shared review-comment store backing `/comments` and
     *        `/export/review`. `nullptr` (the default) disables review: GET
     *        `/comments` returns an empty list, POST returns 400, and export
     *        renders an empty document — the right shape for single-input mode.
     * @param title A human title (e.g. "old.json -> new.json") shown in the
     *        Markdown export header. Empty in single-input mode.
     *
     * The listener is opened/bound/listened immediately so local_port() is valid
     * before run().
     * @throws boost::system::system_error if the bind or listen fails (e.g. the
     *         port is already in use), so a bad port is reported up front rather
     *         than mid-run.
     */
    UmlServer(std::string body,
              std::map<std::string, std::string> sources,
              std::uint16_t port,
              std::shared_ptr<ReviewStore> review = nullptr,
              std::string title = {});

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
    std::map<std::string, std::string> sources_;
    std::shared_ptr<ReviewStore> review_;
    std::string title_;
    boost::asio::io_context io_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::uint16_t assigned_port_{0};
};

}  // namespace server

#endif  // UML_SERVER_H
