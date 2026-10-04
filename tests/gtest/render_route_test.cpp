#include <gtest/gtest.h>

#include "server/uml_server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <chrono>
#include <map>
#include <string>

namespace {

// Minimal raw-socket HTTP client for the Connection: close server (same shape
// as uml_server_test's): open a loopback TCP connection, send a full request,
// read until EOF. Empty string if the connection could not be made.
std::string raw_request(unsigned short port, const std::string& request, int timeout_ms = 3000) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return {};
    }

    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    std::string response;
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return {};
    }

    const char* p = request.data();
    std::size_t remaining = request.size();
    while (remaining > 0) {
        const ssize_t n = ::send(fd, p, remaining, 0);
        if (n <= 0) {
            break;
        }
        p += n;
        remaining -= static_cast<std::size_t>(n);
    }

    char buf[8192];
    for (;;) {
        const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n < 0) {
            break;  // timeout or error: best-effort
        }
        if (n == 0) {
            break;  // server closed the connection (Connection: close)
        }
        response.append(buf, static_cast<std::size_t>(n));
    }

    ::close(fd);
    return response;
}

int status_of(const std::string& response) {
    const std::size_t sp1 = response.find(' ');
    if (sp1 == std::string::npos) {
        return -1;
    }
    const std::size_t sp2 = response.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) {
        return -1;
    }
    return std::stoi(response.substr(sp1 + 1, sp2 - sp1 - 1));
}

std::string body_of(const std::string& response) {
    const std::size_t sep = response.find("\r\n\r\n");
    if (sep == std::string::npos) {
        return {};
    }
    return response.substr(sep + 4);
}

/** @brief True when `bin` resolves on PATH (the server's own availability probe). */
bool binary_available(const std::string& bin) {
    return std::system(("command -v " + bin + " >/dev/null 2>&1").c_str()) == 0;
}

}  // namespace

TEST(RenderRouteTest, NonDiagramPathIsRejected) {
    // Markdown is rendered client-side; it must NOT be routed to the server
    // renderer (which only knows .dot/.drawio).
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";
    const std::map<std::string, std::string> sources{{"x/notes.md", "# hi"}};

    server::UmlServer srv(body, sources, 0);
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);

    std::thread worker{[&] { srv.run(); }};

    std::string resp;
    for (int i = 0; i < 50 && status_of(resp) == -1; ++i) {
        resp = raw_request(port, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_EQ(status_of(resp), 200);

    resp = raw_request(port, "GET /render?path=x/notes.md HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(resp), 400);
    EXPECT_NE(body_of(resp).find("unsupported format"), std::string::npos);

    srv.stop();
    worker.join();
}

TEST(RenderRouteTest, PathMustBeAllowlisted) {
    // The route must behave like /source on the security axis: only an exact
    // allowlist key is ever considered, and the filesystem is never read from
    // the query string.
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";
    const std::map<std::string, std::string> sources{{"x/graph.dot", "digraph g { a -> b; }"}};

    server::UmlServer srv(body, sources, 0);
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);

    std::thread worker{[&] { srv.run(); }};

    std::string resp;
    for (int i = 0; i < 50 && status_of(resp) == -1; ++i) {
        resp = raw_request(port, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_EQ(status_of(resp), 200);

    // A path that exists on the host but is not in the allowlist -> 404.
    resp = raw_request(port, "GET /render?path=/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(resp), 404);
    EXPECT_NE(body_of(resp).find("no such source"), std::string::npos);

    // No path= param at all -> 404.
    EXPECT_EQ(status_of(raw_request(port, "GET /render HTTP/1.1\r\nHost: x\r\n\r\n")), 404);

    // A longer param that merely contains "path=" must not match.
    resp = raw_request(port, "GET /render?xpath=x/graph.dot HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(resp), 404);

    srv.stop();
    worker.join();
}

TEST(RenderRouteTest, DotRenderFollowsBinaryAvailability) {
    // On a host without `dot`, the route degrades to a clean JSON 503 rather
    // than an empty body; on a host with it, it returns image/svg+xml.
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";
    const std::map<std::string, std::string> sources{
        {"x/graph.dot", "digraph g { \"a\" -> \"b\"; }"},
        {"x/diagram.drawio", "<mxfile><diagram>fixture</diagram></mxfile>"},
    };

    server::UmlServer srv(body, sources, 0);
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);

    std::thread worker{[&] { srv.run(); }};

    std::string resp;
    for (int i = 0; i < 50 && status_of(resp) == -1; ++i) {
        resp = raw_request(port, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_EQ(status_of(resp), 200);

    // --- .dot -------------------------------------------------------------
    if (binary_available("dot")) {
        resp = raw_request(port, "GET /render?path=x/graph.dot HTTP/1.1\r\nHost: x\r\n\r\n");
        EXPECT_EQ(status_of(resp), 200);
        EXPECT_NE(resp.find("image/svg+xml"), std::string::npos);
        EXPECT_NE(body_of(resp).find("<svg"), std::string::npos);
    } else {
        resp = raw_request(port, "GET /render?path=x/graph.dot HTTP/1.1\r\nHost: x\r\n\r\n");
        EXPECT_EQ(status_of(resp), 503);
        EXPECT_NE(resp.find("application/json"), std::string::npos);
        EXPECT_NE(body_of(resp).find("not available"), std::string::npos);
    }

    // --- .drawio ----------------------------------------------------------
    if (binary_available("drawio")) {
        resp = raw_request(port, "GET /render?path=x/diagram.drawio HTTP/1.1\r\nHost: x\r\n\r\n");
        EXPECT_EQ(status_of(resp), 200);
        EXPECT_NE(resp.find("image/svg+xml"), std::string::npos);
    } else {
        resp = raw_request(port, "GET /render?path=x/diagram.drawio HTTP/1.1\r\nHost: x\r\n\r\n");
        EXPECT_EQ(status_of(resp), 503);
        EXPECT_NE(body_of(resp).find("not available"), std::string::npos);
    }

    srv.stop();
    worker.join();
}
