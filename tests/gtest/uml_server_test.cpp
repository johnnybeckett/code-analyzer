#include <gtest/gtest.h>

#include "server/uml_server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <chrono>
#include <map>
#include <string>

namespace {

// Minimal raw-socket HTTP client for the Connection: close server: open a loopback
// TCP connection, send a full request, read until EOF, return the raw response.
// Returns an empty string if the connection could not be made (server not ready).
std::string raw_request(unsigned short port, const std::string& request, int timeout_ms = 2000) {
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

    char buf[4096];
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

// Parse the integer status code out of a response's first line ("HTTP/1.1 200 OK").
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

// The message body: everything after the blank line separating headers from body.
std::string body_of(const std::string& response) {
    const std::size_t sep = response.find("\r\n\r\n");
    if (sep == std::string::npos) {
        return {};
    }
    return response.substr(sep + 4);
}

}  // namespace

TEST(UmlServerTest, ServesSplicedBodyAnd405sNonGet) {
    // A distinctive body stands in for the real model page (built/verified by
    // uml_model_test); this test isolates the server's request/response behavior.
    const std::string body = "<!DOCTYPE html><html><body>SPLICE_MARKER_XYZ</body></html>";

    server::UmlServer srv(body, {}, 0);  // port 0 -> OS-assigned ephemeral port
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);

    std::thread worker{[&] { srv.run(); }};

    // Readiness: the accept loop may take a moment to come up; poll with a bound.
    std::string resp;
    for (int i = 0; i < 50 && status_of(resp) == -1; ++i) {
        resp = raw_request(port, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // GET / -> 200, the exact body, HTML content type.
    EXPECT_EQ(status_of(resp), 200);
    EXPECT_NE(resp.find("SPLICE_MARKER_XYZ"), std::string::npos);
    EXPECT_NE(resp.find("text/html; charset=utf-8"), std::string::npos);

    // Any path serves the same page.
    EXPECT_EQ(status_of(raw_request(port, "GET /whatever/deep HTTP/1.1\r\nHost: x\r\n\r\n")), 200);

    // Non-GET methods -> 405.
    EXPECT_EQ(status_of(raw_request(port, "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 2\r\n\r\nok")), 405);
    EXPECT_EQ(status_of(raw_request(port, "HEAD / HTTP/1.1\r\nHost: x\r\n\r\n")), 405);

    srv.stop();
    worker.join();
}

TEST(UmlServerTest, SourceRoute_AllowlistedServedEverythingElse404s) {
    // A real-looking allowlisted source file (path -> content) is handed in at
    // construction; the route may return it by exact match and nothing else.
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";
    const std::map<std::string, std::string> sources{{"/src/a.cpp", "int x = 1;"}};

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

    // Allowlisted path -> 200, the exact stored content, plain-text type.
    resp = raw_request(port, "GET /source?path=/src/a.cpp HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(resp), 200);
    EXPECT_EQ(body_of(resp), "int x = 1;");
    EXPECT_NE(resp.find("text/plain; charset=utf-8"), std::string::npos);

    // Percent-encoded (what the pane sends via encodeURIComponent).
    resp = raw_request(port, "GET /source?path=%2Fsrc%2Fa.cpp HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(resp), 200);
    EXPECT_EQ(body_of(resp), "int x = 1;");

    // Not in the allowlist -> 404; the server never reads the filesystem.
    EXPECT_EQ(status_of(raw_request(port,
                                    "GET /source?path=/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n")), 404);

    // No path= param at all -> 404.
    EXPECT_EQ(status_of(raw_request(port, "GET /source HTTP/1.1\r\nHost: x\r\n\r\n")), 404);

    // A longer param that merely contains "path=" must not match.
    EXPECT_EQ(status_of(raw_request(port,
                                    "GET /source?xpath=/src/a.cpp HTTP/1.1\r\nHost: x\r\n\r\n")), 404);

    // Non-GET on the route -> 405.
    EXPECT_EQ(status_of(raw_request(port,
                                    "POST /source?path=/src/a.cpp HTTP/1.1\r\nHost: x\r\nContent-Length: 2\r\n\r\nok")),
              405);

    srv.stop();
    worker.join();
}
