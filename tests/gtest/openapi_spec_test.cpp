#include <gtest/gtest.h>

#include "server/uml_server.h"

#include <boost/json.hpp>

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

namespace json = boost::json;

namespace {

// Raw-socket helpers, copied verbatim from uml_server_test.cpp (which keeps
// them in its own anonymous namespace, so they are not linkable across TUs).
// Minimal raw-socket HTTP client for the Connection: close server: open a
// loopback TCP connection, send a full request, read until EOF, return the
// raw response. Returns an empty string if the connection could not be made.
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

// Poll GET / until the accept loop answers 200 (it may take a moment to come
// up). Bounded so a hung server fails the test rather than blocking forever.
bool wait_ready(unsigned short port) {
    for (int i = 0; i < 50; ++i) {
        const std::string resp = raw_request(port, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        if (status_of(resp) == 200) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

}  // namespace

// A UmlServer with no sources and no review store still registers all five
// domain handlers plus the two meta handlers, so the spec/reflection always
// describe the full API surface.
TEST(OpenApiSpecTest, SpecRoute_ReturnsOpenApiJson) {
    const std::string body = "<!DOCTYPE html><html><body>HTML_MARKER_XYZ</body></html>";

    server::UmlServer srv(body, {}, 0);
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    const std::string resp = raw_request(port, "GET /openapi.json HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(resp), 200);
    EXPECT_NE(resp.find("application/json"), std::string::npos);

    const std::string b = body_of(resp);
    // It is a JSON document, not the HTML fallback page.
    EXPECT_EQ(b.find("HTML_MARKER_XYZ"), std::string::npos);
    EXPECT_NE(b.find("\"openapi\":\"3.0.3\""), std::string::npos);
    EXPECT_NE(b.find("\"info\""), std::string::npos);
    EXPECT_NE(b.find("\"paths\""), std::string::npos);
    EXPECT_NE(b.find("\"/source\""), std::string::npos);
    EXPECT_NE(b.find("\"/comments\""), std::string::npos);
    EXPECT_NE(b.find("\"/openapi.json\""), std::string::npos);
    EXPECT_NE(b.find("\"/api\""), std::string::npos);

    srv.stop();
    worker.join();
}

TEST(OpenApiSpecTest, Spec_CommentsPathHasGetAndPost) {
    server::UmlServer srv("<html>page</html>", {}, 0);
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    const std::string b = body_of(raw_request(port, "GET /openapi.json HTTP/1.1\r\nHost: x\r\n\r\n"));

    const auto spec = json::parse(b);
    ASSERT_TRUE(spec.is_object());
    const auto& paths = spec.as_object().at("paths").as_object();
    ASSERT_TRUE(paths.contains("/comments"));

    const auto& comments = paths.at("/comments").as_object();
    ASSERT_TRUE(comments.contains("get"));
    ASSERT_TRUE(comments.contains("post"));

    // The POST declares a JSON requestBody (built from its body params).
    const auto& post = comments.at("post").as_object();
    ASSERT_TRUE(post.contains("requestBody"));
    const auto& reqbody = post.at("requestBody").as_object();
    ASSERT_TRUE(reqbody.contains("content"));
    const auto& content = reqbody.at("content").as_object();
    ASSERT_TRUE(content.contains("application/json"));
    const auto& schema = content.at("application/json").as_object().at("schema").as_object();
    ASSERT_TRUE(schema.contains("properties"));
    ASSERT_TRUE(schema.at("properties").as_object().contains("text"));

    srv.stop();
    worker.join();
}

TEST(OpenApiSpecTest, Spec_SourceHasPathParameter) {
    server::UmlServer srv("<html>page</html>", {}, 0);
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    const std::string b = body_of(raw_request(port, "GET /openapi.json HTTP/1.1\r\nHost: x\r\n\r\n"));

    const auto spec = json::parse(b);
    ASSERT_TRUE(spec.is_object());
    const auto& paths = spec.as_object().at("paths").as_object();
    ASSERT_TRUE(paths.contains("/source"));
    const auto& source = paths.at("/source").as_object();
    ASSERT_TRUE(source.contains("get"));
    const auto& get = source.at("get").as_object();
    ASSERT_TRUE(get.contains("parameters"));

    bool found = false;
    for (const auto& p : get.at("parameters").as_array()) {
        const auto& po = p.as_object();
        if (po.contains("name") && po.at("name").as_string() == "path" &&
            po.contains("in") && po.at("in").as_string() == "query" &&
            po.contains("required") && po.at("required").as_bool()) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "/source's GET must declare a required query parameter named \"path\"";

    srv.stop();
    worker.join();
}

TEST(OpenApiSpecTest, ReflectionApi_ListsEndpoints) {
    server::UmlServer srv("<html>page</html>", {}, 0);
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    const std::string resp = raw_request(port, "GET /api HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(resp), 200);
    EXPECT_NE(resp.find("application/json"), std::string::npos);

    const auto arr = json::parse(body_of(resp));
    ASSERT_TRUE(arr.is_array());

    bool has_post_comments = false;
    for (const auto& e : arr.as_array()) {
        const auto& eo = e.as_object();
        ASSERT_TRUE(eo.contains("method"));
        ASSERT_TRUE(eo.contains("path"));
        ASSERT_TRUE(eo.contains("summary"));
        ASSERT_TRUE(eo.contains("responses"));
        if (eo.at("method").as_string() == "POST" && eo.at("path").as_string() == "/comments") {
            has_post_comments = true;
        }
    }
    EXPECT_TRUE(has_post_comments) << "reflection must list POST /comments";

    srv.stop();
    worker.join();
}

TEST(OpenApiSpecTest, Routing_MethodKeyedMissIs405) {
    server::UmlServer srv("<html>page</html>", {}, 0);
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    // A known path under the wrong verb is a method miss, not the HTML fallback.
    EXPECT_EQ(status_of(raw_request(port,
                                    "POST /openapi.json HTTP/1.1\r\nHost: x\r\n"
                                    "Content-Length: 2\r\n\r\nok")),
              405);
    EXPECT_EQ(status_of(raw_request(port,
                                    "POST /api HTTP/1.1\r\nHost: x\r\n"
                                    "Content-Length: 2\r\n\r\nok")),
              405);

    srv.stop();
    worker.join();
}

TEST(OpenApiSpecTest, Routing_HtmlFallbackIntact) {
    const std::string body = "<!DOCTYPE html><html><body>HTML_MARKER_XYZ</body></html>";

    server::UmlServer srv(body, {}, 0);
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    // An unregistered GET still serves the spliced viewer page.
    const std::string resp =
        raw_request(port, "GET /definitely/not/a/route HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(resp), 200);
    EXPECT_NE(resp.find("text/html"), std::string::npos);
    EXPECT_NE(body_of(resp).find("HTML_MARKER_XYZ"), std::string::npos);

    // A non-GET on a registered path is 405.
    EXPECT_EQ(status_of(raw_request(port, "HEAD /api HTTP/1.1\r\nHost: x\r\n\r\n")), 405);

    srv.stop();
    worker.join();
}
