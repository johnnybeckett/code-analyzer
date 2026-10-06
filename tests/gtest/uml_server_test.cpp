#include <gtest/gtest.h>

#include "server/uml_server.h"
#include "uml/uml_model.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include <chrono>
#include <map>
#include <string>
#include <vector>

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

// POST a raw JSON body to /comments and return the full raw response.
std::string post_comment(unsigned short port, const std::string& body) {
    std::string req = "POST /comments HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
                      "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    return raw_request(port, req);
}

// A CWD-relative review file unique to a test, guaranteed fresh: any stale copy
// left by a prior (failed) run is removed up front. The caller removes it again
// in cleanup once the last server built on it has stopped.
std::string fresh_review_path(const char* tag) {
    const std::string path = std::string("uml_review_test_") + tag + ".json";
    std::remove(path.c_str());
    return path;
}

// A small, hand-checkable index mirroring class_index_test.cpp: five classes in
// two namespaces. Alpha sits at (0,0,0); group "B" is tiled far along +X, so
// Delta/Epsilon are always the farthest classes from the origin.
std::shared_ptr<server::ClassIndex> index_five() {
    auto make_rec = [](const char* name, const char* ns) {
        server::ClassRecord r;
        r.name = name;
        r.namespace_name = ns;
        return r;
    };
    return std::make_shared<server::ClassIndex>(std::vector<server::ClassRecord>{
        make_rec("Alpha", "A"),
        make_rec("Beta", "A"),
        make_rec("Gamma", "A"),
        make_rec("Delta", "B"),
        make_rec("Epsilon", "B"),
    });
}

// Count occurrences of `needle` in `haystack`. The test records carry no
// methods or variables, so each serialized record contributes exactly one
// "name" key — counting them counts the records.
std::size_t count_of(const std::string& haystack, const std::string& needle) {
    std::size_t count = 0, pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

// Write an analyzer-shaped JSON fixture to a temp dir and return its path.
// Lets a test build the *real* spliced page (via uml::UmlModel::build_html)
// rather than a marker body, so assertions run on what the server actually
// serves.
std::string write_json(const std::string& name, const std::string& content) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "uml_server_test";
    std::filesystem::create_directories(dir);
    const std::filesystem::path file = dir / name;
    std::ofstream out(file);
    out << content;
    return file.string();
}

// Build the complete UML page exactly as main.cpp does (the UmlModel Facade
// splices the embedded three.js template). One tiny class is enough — the S3
// streaming wiring is static page content, independent of the class data.
std::string real_page() {
    const std::string json = write_json(
        "streaming_page.json",
        R"({"classes":[{"name":"Alpha","namespace":"A","methods":[],"attributes":[],"inheritance":[]}]})");
    return uml::UmlModel({json}, {}).build_html();
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

TEST(UmlServerTest, Comments_PostThenGet_PersistsAcrossRestart) {
    const std::string path = fresh_review_path("persist");
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";
    const auto store = std::make_shared<server::ReviewStore>(path);

    {
        server::UmlServer srv(body, {}, 0, store, "old.json -> new.json");
        const unsigned short port = srv.local_port();
        ASSERT_NE(port, 0);
        std::thread worker{[&] { srv.run(); }};
        ASSERT_TRUE(wait_ready(port));

        // A valid comment -> 201, echoing a server-assigned id and a creation stamp.
        const std::string resp = post_comment(
            port, "{\"file\":\"/src/a.cpp\",\"oldLine\":3,\"newLine\":4,"
                  "\"text\":\"please add a bounds check\"}");
        EXPECT_EQ(status_of(resp), 201);
        EXPECT_NE(body_of(resp).find("\"id\""), std::string::npos);
        EXPECT_NE(body_of(resp).find("please add a bounds check"), std::string::npos);

        // GET /comments -> 200, JSON, and the stored text.
        const std::string g = raw_request(port, "GET /comments HTTP/1.1\r\nHost: x\r\n\r\n");
        EXPECT_EQ(status_of(g), 200);
        EXPECT_NE(g.find("application/json"), std::string::npos);
        EXPECT_NE(g.find("please add a bounds check"), std::string::npos);

        srv.stop();
        worker.join();
    }

    // A brand-new server (and store) on the same file: the comment must still be
    // present — it is file-backed, not in-memory.
    {
        server::UmlServer srv(body, {}, 0, std::make_shared<server::ReviewStore>(path),
                              "old.json -> new.json");
        const unsigned short port = srv.local_port();
        ASSERT_NE(port, 0);
        std::thread worker{[&] { srv.run(); }};
        ASSERT_TRUE(wait_ready(port));

        const std::string g = raw_request(port, "GET /comments HTTP/1.1\r\nHost: x\r\n\r\n");
        EXPECT_EQ(status_of(g), 200);
        EXPECT_NE(g.find("please add a bounds check"), std::string::npos);

        srv.stop();
        worker.join();
    }

    std::remove(path.c_str());
}

TEST(UmlServerTest, Comments_PostValidation) {
    const std::string path = fresh_review_path("valid");
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";
    const auto store = std::make_shared<server::ReviewStore>(path);

    server::UmlServer srv(body, {}, 0, store, "t");
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    // Not JSON at all -> 400.
    EXPECT_EQ(status_of(post_comment(port, "this is not json")), 400);
    // Missing text -> 400.
    EXPECT_EQ(status_of(post_comment(port, "{\"oldLine\":1}")), 400);
    // Whitespace-only text -> 400.
    EXPECT_EQ(status_of(post_comment(port, "{\"text\":\"   \",\"newLine\":2}")), 400);
    // Both lines zero -> 400.
    EXPECT_EQ(status_of(post_comment(port, "{\"text\":\"x\",\"oldLine\":0,\"newLine\":0}")), 400);

    // A minimal valid comment (text + one line) -> 201.
    EXPECT_EQ(status_of(post_comment(port, "{\"text\":\"ok\",\"newLine\":7}")), 201);

    srv.stop();
    worker.join();
    std::remove(path.c_str());
}

TEST(UmlServerTest, Comments_ExportMarkdown) {
    const std::string path = fresh_review_path("export");
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";
    const std::map<std::string, std::string> sources{{"/src/a.cpp", "int x = 1;\nint y = 2;"}};
    const auto store = std::make_shared<server::ReviewStore>(path);

    server::UmlServer srv(body, sources, 0, store, "old.json -> new.json");
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    // Zero comments yet -> 200, but an empty document.
    EXPECT_NE(body_of(raw_request(port, "GET /export/review HTTP/1.1\r\nHost: x\r\n\r\n"))
                  .find("No review comments."),
              std::string::npos);

    // One comment on new line 2 (old line 1), in an allowlisted source file.
    EXPECT_EQ(status_of(post_comment(
                  port, "{\"file\":\"/src/a.cpp\",\"oldLine\":1,\"newLine\":2,"
                        "\"text\":\"rename this variable\"}")),
              201);

    const std::string resp = raw_request(port, "GET /export/review HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(resp), 200);
    // Served as a Markdown file download.
    EXPECT_NE(resp.find("attachment; filename=\"review.md\""), std::string::npos);
    EXPECT_NE(resp.find("text/markdown"), std::string::npos);

    const std::string b = body_of(resp);
    EXPECT_NE(b.find("# Code Review"), std::string::npos);
    EXPECT_NE(b.find("## /src/a.cpp"), std::string::npos);
    EXPECT_NE(b.find("**L2**"), std::string::npos);          // primary = newLine
    EXPECT_NE(b.find("int y = 2;"), std::string::npos);       // quoted source line
    EXPECT_NE(b.find("rename this variable"), std::string::npos);

    // Non-GET on the route -> 405.
    EXPECT_EQ(status_of(raw_request(
                  port, "POST /export/review HTTP/1.1\r\nHost: x\r\nContent-Length: 2\r\n\r\nok")),
              405);

    srv.stop();
    worker.join();
    std::remove(path.c_str());
}

TEST(UmlServerTest, Comments_ReviewDisabledWhenNoStore) {
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";

    server::UmlServer srv(body, {}, 0);  // review defaults to nullptr (single-input mode)
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    // GET /comments -> a well-formed (empty) JSON list, not an error.
    const std::string g = raw_request(port, "GET /comments HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(g), 200);
    EXPECT_NE(body_of(g).find("\"comments\":[]"), std::string::npos);

    // POST /comments -> 400 (review is disabled), not a stored comment.
    EXPECT_EQ(status_of(post_comment(port, "{\"text\":\"x\",\"newLine\":1}")), 400);

    // GET /export/review -> 200, but an empty document.
    const std::string e = raw_request(port, "GET /export/review HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(e), 200);
    EXPECT_NE(body_of(e).find("No review comments."), std::string::npos);

    // The /source allowlist and the HTML catch-all are untouched by the new routes.
    EXPECT_EQ(status_of(raw_request(port,
                                    "GET /source?path=/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n")),
              404);
    EXPECT_EQ(status_of(raw_request(port,
                                    "GET /whatever/deep HTTP/1.1\r\nHost: x\r\n\r\n")),
              200);

    srv.stop();
    worker.join();
}

TEST(UmlServerTest, ClassesIndex_ServesSpatialOverview) {
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";

    server::UmlServer srv(body, {}, 0, nullptr, {}, index_five());
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    const std::string resp = raw_request(port, "GET /classes/index HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(resp), 200);
    EXPECT_NE(resp.find("application/json"), std::string::npos);

    const std::string b = body_of(resp);
    EXPECT_NE(b.find("\"count\":5"), std::string::npos);
    EXPECT_NE(b.find("\"bounds\""), std::string::npos);
    EXPECT_NE(b.find("\"min_x\""), std::string::npos);
    EXPECT_NE(b.find("\"max_z\""), std::string::npos);
    EXPECT_NE(b.find("\"cellSize\":1"), std::string::npos);
    EXPECT_NE(b.find("\"sample\""), std::string::npos);
    EXPECT_NE(b.find("Alpha"), std::string::npos);

    srv.stop();
    worker.join();
}

TEST(UmlServerTest, ClassesNear_ReturnsNearestFirstAndClampsCount) {
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";

    server::UmlServer srv(body, {}, 0, nullptr, {}, index_five());
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    // Two closest to the origin: Alpha (0,0,0) plus one of Beta/Gamma (dist 1).
    const std::string r2 =
        raw_request(port, "GET /classes/near?x=0&y=0&z=0&count=2 HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(r2), 200);
    EXPECT_NE(r2.find("application/json"), std::string::npos);
    const std::string b2 = body_of(r2);
    ASSERT_FALSE(b2.empty());
    EXPECT_EQ(b2.front(), '[');  // a JSON array of records
    EXPECT_NE(b2.find("Alpha"), std::string::npos);
    // Group "B" is tiled far along +X — neither of its classes can be near.
    EXPECT_EQ(b2.find("Delta"), std::string::npos);
    EXPECT_EQ(b2.find("Epsilon"), std::string::npos);
    EXPECT_EQ(count_of(b2, "\"name\""), 2u);

    // count far beyond the index size -> all five records.
    const std::string big =
        raw_request(port, "GET /classes/near?x=0&y=0&z=0&count=1000000 HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(big), 200);
    EXPECT_EQ(count_of(body_of(big), "\"name\""), 5u);

    // count=1 -> exactly one record, the closest (Alpha).
    const std::string one =
        body_of(raw_request(port, "GET /classes/near?x=0&y=0&z=0&count=1 HTTP/1.1\r\nHost: x\r\n\r\n"));
    EXPECT_EQ(count_of(one, "\"name\""), 1u);
    EXPECT_NE(one.find("Alpha"), std::string::npos);

    srv.stop();
    worker.join();
}

TEST(UmlServerTest, ClassesNear_ValidatesParameters) {
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";

    server::UmlServer srv(body, {}, 0, nullptr, {}, index_five());
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    // Missing z -> 400 JSON error.
    const std::string missing =
        raw_request(port, "GET /classes/near?x=0&y=0&count=1 HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(missing), 400);
    EXPECT_NE(body_of(missing).find("\"error\""), std::string::npos);

    // Non-numeric coordinate -> 400.
    EXPECT_EQ(status_of(raw_request(
                  port, "GET /classes/near?x=abc&y=0&z=0&count=1 HTTP/1.1\r\nHost: x\r\n\r\n")),
              400);

    // Missing count -> 400.
    EXPECT_EQ(status_of(raw_request(
                  port, "GET /classes/near?x=0&y=0&z=0 HTTP/1.1\r\nHost: x\r\n\r\n")),
              400);

    // Non-numeric count -> 400.
    EXPECT_EQ(status_of(raw_request(
                  port, "GET /classes/near?x=0&y=0&z=0&count=xyz HTTP/1.1\r\nHost: x\r\n\r\n")),
              400);

    srv.stop();
    worker.join();
}

TEST(UmlServerTest, ClassesRoutes_AppearInApiAndOpenApi) {
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";

    server::UmlServer srv(body, {}, 0, nullptr, {}, index_five());
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    const std::string api = raw_request(port, "GET /api HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(api), 200);
    EXPECT_NE(api.find("/classes/index"), std::string::npos);
    EXPECT_NE(api.find("/classes/near"), std::string::npos);

    const std::string spec = raw_request(port, "GET /openapi.json HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(spec), 200);
    EXPECT_NE(spec.find("/classes/index"), std::string::npos);
    EXPECT_NE(spec.find("/classes/near"), std::string::npos);

    srv.stop();
    worker.join();
}

TEST(UmlServerTest, ClassesRoutes_NullIndexIs501) {
    const std::string body = "<!DOCTYPE html><html><body>PAGE</body></html>";

    server::UmlServer srv(body, {}, 0);  // no class index -> the routes stay 501
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    const std::string idx = raw_request(port, "GET /classes/index HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(idx), 501);
    EXPECT_NE(body_of(idx).find("\"error\""), std::string::npos);

    // Valid params (the handler validates them before the null-index check), so
    // the 501 — not a 400 — is what a well-formed request sees without an index.
    const std::string near =
        raw_request(port, "GET /classes/near?x=0&y=0&z=0&count=1 HTTP/1.1\r\nHost: x\r\n\r\n");
    EXPECT_EQ(status_of(near), 501);
    EXPECT_NE(body_of(near).find("\"error\""), std::string::npos);

    // The rest of the API is unaffected by the missing index.
    EXPECT_EQ(status_of(raw_request(port, "GET /comments HTTP/1.1\r\nHost: x\r\n\r\n")), 200);
    EXPECT_EQ(status_of(raw_request(port, "GET /whatever/deep HTTP/1.1\r\nHost: x\r\n\r\n")), 200);

    srv.stop();
    worker.join();
}

// S3 (Task 1 client): the served page must carry the streaming/nearest-N
// wiring that makes a 10k-class diagram stream the visible window and grid-
// cache it, instead of materializing every box up front. The page is built
// the same way main.cpp builds it (real spliced template), so this is a true
// black-box check of what the browser actually receives — ctest cannot run
// the JS, so the presence of the wiring is the contract.
TEST(UmlServerTest, ServesPageWithStreamingWiring) {
    const std::string body = real_page();
    ASSERT_FALSE(body.empty());

    server::UmlServer srv(body, {}, 0, nullptr, {}, index_five());
    const unsigned short port = srv.local_port();
    ASSERT_NE(port, 0);
    std::thread worker{[&] { srv.run(); }};
    ASSERT_TRUE(wait_ready(port));

    const std::string resp = raw_request(port, "GET / HTTP/1.1\r\nHost: x\r\n\r\n");
    ASSERT_EQ(status_of(resp), 200);
    const std::string page = body_of(resp);

    // The two server endpoints the streaming module drives.
    EXPECT_NE(page.find("/classes/index"), std::string::npos);
    EXPECT_NE(page.find("/classes/near"), std::string::npos);

    // The materialize-on-entry / dispose-on-exit pair that bounds the live set
    // (the RAM fix), and the grid-cell cache that makes a pan-back instant.
    EXPECT_NE(page.find("materialize"), std::string::npos);
    EXPECT_NE(page.find("dispose"), std::string::npos);
    EXPECT_NE(page.find("gridCache"), std::string::npos);

    // The camera-move trigger, the one-in-flight / stale-ignore guard, and the
    // small-project escape hatch that keeps the eager build as the default.
    EXPECT_NE(page.find("onMove"), std::string::npos);
    EXPECT_NE(page.find("maybeEnable"), std::string::npos);
    EXPECT_NE(page.find("requestNear"), std::string::npos);

    srv.stop();
    worker.join();
}
