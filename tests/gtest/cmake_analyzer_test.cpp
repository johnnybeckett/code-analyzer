#include <gtest/gtest.h>

#include "core/cmake_analyzer.h"
#include "core/cmake_model.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

/** @brief Locate a target by name in a graph; nullptr when absent. */
const CMakeTarget* find_target(const CMakeGraph& graph, const std::string& name) {
    for (const auto& t : graph.targets) {
        if (t.name == name) {
            return &t;
        }
    }
    return nullptr;
}

/** @brief True when `needle` is one of the target's sources. */
bool has_source(const CMakeTarget& t, const std::string& needle) {
    return std::find(t.sources.begin(), t.sources.end(), needle) != t.sources.end();
}

/** @brief True when `needle` is one of the target's links. */
bool has_link(const CMakeTarget& t, const std::string& needle) {
    return std::find(t.links.begin(), t.links.end(), needle) != t.links.end();
}

}  // namespace

// ---------------------------------------------------------------------------
// White-box: parse_text() against in-memory CMake blobs. No fixture files, no
// filesystem — the extraction rules themselves are under test.
// ---------------------------------------------------------------------------

TEST(CMakeAnalyzerTest, ParseText_AddLibraryKinds) {
    CMakeAnalyzer analyzer;
    const CMakeGraph g = analyzer.parse_text(R"(
        add_library(core STATIC core.cpp core.h)
        add_library(net SHARED net.cpp)
        add_library(iface INTERFACE)
        add_executable(app app.cpp)
    )");

    ASSERT_EQ(g.targets.size(), 4u);
    const CMakeTarget* core = find_target(g, "core");
    const CMakeTarget* net = find_target(g, "net");
    const CMakeTarget* iface = find_target(g, "iface");
    const CMakeTarget* app = find_target(g, "app");
    ASSERT_NE(core, nullptr);
    ASSERT_NE(net, nullptr);
    ASSERT_NE(iface, nullptr);
    ASSERT_NE(app, nullptr);

    EXPECT_EQ(core->kind, "library");   // STATIC
    EXPECT_EQ(net->kind, "library");    // SHARED
    EXPECT_EQ(iface->kind, "interface");
    EXPECT_EQ(app->kind, "executable");

    EXPECT_TRUE(has_source(*core, "core.cpp"));
    EXPECT_TRUE(has_source(*core, "core.h"));
    EXPECT_TRUE(has_source(*net, "net.cpp"));
    EXPECT_TRUE(iface->sources.empty());
    EXPECT_TRUE(has_source(*app, "app.cpp"));
}

TEST(CMakeAnalyzerTest, ParseText_AliasTarget) {
    CMakeAnalyzer analyzer;
    const CMakeGraph g = analyzer.parse_text(R"(
        add_library(core STATIC core.cpp)
        add_library(core-alias ALIAS core)
    )");

    const CMakeTarget* real = find_target(g, "core");
    const CMakeTarget* alias = find_target(g, "core-alias");
    ASSERT_NE(real, nullptr);
    ASSERT_NE(alias, nullptr);

    EXPECT_EQ(real->kind, "library");
    EXPECT_EQ(alias->kind, "alias");
    EXPECT_EQ(alias->alias_of, "core");
    EXPECT_TRUE(alias->sources.empty());
    EXPECT_TRUE(alias->links.empty());
}

TEST(CMakeAnalyzerTest, ParseText_TargetSourcesMergesAndStripsKeywords) {
    CMakeAnalyzer analyzer;
    const CMakeGraph g = analyzer.parse_text(R"(
        add_library(core STATIC core.cpp)
        target_sources(core PUBLIC common.h PRIVATE extra.cpp)
        target_sources(core INTERFACE)
    )");

    const CMakeTarget* core = find_target(g, "core");
    ASSERT_NE(core, nullptr);
    ASSERT_EQ(core->sources.size(), 3u);
    EXPECT_TRUE(has_source(*core, "core.cpp"));
    EXPECT_TRUE(has_source(*core, "common.h"));
    EXPECT_TRUE(has_source(*core, "extra.cpp"));
    // PRIVATE / PUBLIC / INTERFACE must never appear as sources.
    EXPECT_FALSE(has_source(*core, "PRIVATE"));
    EXPECT_FALSE(has_source(*core, "PUBLIC"));
    EXPECT_FALSE(has_source(*core, "INTERFACE"));
}

TEST(CMakeAnalyzerTest, ParseText_TargetSources_CreatesUndeclaredTarget) {
    // target_sources may name a target that is declared later (or only in
    // another file): the merge must not lose either.
    CMakeAnalyzer analyzer;
    const CMakeGraph g = analyzer.parse_text(R"(
        target_sources(late PUBLIC first.cpp)
        add_library(late STATIC late.cpp)
    )");

    const CMakeTarget* late = find_target(g, "late");
    ASSERT_NE(late, nullptr);
    EXPECT_TRUE(has_source(*late, "late.cpp"));
    EXPECT_TRUE(has_source(*late, "first.cpp"));
}

TEST(CMakeAnalyzerTest, ParseText_LinkLibraries_StripsKeywordsAndNonTargets) {
    CMakeAnalyzer analyzer;
    const CMakeGraph g = analyzer.parse_text(R"(
        add_library(net SHARED net.cpp)
        target_link_libraries(net
            PRIVATE core
            PUBLIC util
            INTERFACE iface
            ${CMAKE_DL_LIBS}
            -lz
            /usr/lib/libprebuilt.a
            $<TARGET_NAME:core>)
    )");

    const CMakeTarget* net = find_target(g, "net");
    ASSERT_NE(net, nullptr);
    ASSERT_EQ(net->links.size(), 3u);
    EXPECT_TRUE(has_link(*net, "core"));
    EXPECT_TRUE(has_link(*net, "util"));
    EXPECT_TRUE(has_link(*net, "iface"));
    // Keywords, variables, -l flags, paths and generator expressions: gone.
    EXPECT_FALSE(has_link(*net, "PRIVATE"));
    EXPECT_FALSE(has_link(*net, "PUBLIC"));
    EXPECT_FALSE(has_link(*net, "INTERFACE"));
    EXPECT_FALSE(has_link(*net, "${CMAKE_DL_LIBS}"));
    EXPECT_FALSE(has_link(*net, "-lz"));
    EXPECT_FALSE(has_link(*net, "/usr/lib/libprebuilt.a"));
    EXPECT_FALSE(has_link(*net, "$<TARGET_NAME:core>"));
}

TEST(CMakeAnalyzerTest, ParseText_SourcesStayExactlyAsWritten) {
    // parse_text() has no directory to qualify against: relative names are
    // recorded verbatim (the white-box seam), and only plain relative names
    // would ever be prefixed by parse_directory().
    CMakeAnalyzer analyzer;
    const CMakeGraph g = analyzer.parse_text(R"(
        add_library(core STATIC core.cpp)
    )");
    const CMakeTarget* core = find_target(g, "core");
    ASSERT_NE(core, nullptr);
    ASSERT_EQ(core->sources.size(), 1u);
    EXPECT_EQ(core->sources[0], "core.cpp");
}

TEST(CMakeAnalyzerTest, ParseText_EmptyAndGarbageYieldEmptyGraph) {
    CMakeAnalyzer analyzer;
    EXPECT_TRUE(analyzer.parse_text("").empty());
    EXPECT_TRUE(analyzer.parse_text("just some # comments\nno commands").empty());
}

TEST(CMakeAnalyzerTest, ParseText_CaseInsensitiveCommandAndKeywordMatching) {
    CMakeAnalyzer analyzer;
    const CMakeGraph g = analyzer.parse_text(R"(
        ADD_LIBRARY(core STATIC Core.CPP)
        target_link_libraries(core PUBLIC Core)
    )");

    // Command names are matched case-insensitively, but target names and
    // sources keep their original spelling.
    const CMakeTarget* core = find_target(g, "core");
    ASSERT_NE(core, nullptr);
    EXPECT_TRUE(has_source(*core, "Core.CPP"));
    EXPECT_TRUE(has_link(*core, "Core"));
}

// ---------------------------------------------------------------------------
// Black-box: parse_directory() against the real fixture tree. Exercises file
// discovery, directory-qualified sources, and cross-command merging on disk.
// ---------------------------------------------------------------------------

TEST(CMakeAnalyzerTest, ParseDirectory_FixtureTree_FiveTargets) {
    CMakeAnalyzer analyzer;
    const CMakeGraph g = analyzer.parse_directory(std::string(SOURCE_DIR) + "/test_files/cmake");

    ASSERT_EQ(g.targets.size(), 5u);

    const CMakeTarget* core = find_target(g, "core");
    const CMakeTarget* net = find_target(g, "net");
    const CMakeTarget* iface = find_target(g, "iface");
    const CMakeTarget* alias = find_target(g, "core-alias");
    const CMakeTarget* app = find_target(g, "app");
    ASSERT_NE(core, nullptr);
    ASSERT_NE(net, nullptr);
    ASSERT_NE(iface, nullptr);
    ASSERT_NE(alias, nullptr);
    ASSERT_NE(app, nullptr);

    EXPECT_EQ(core->kind, "library");
    EXPECT_EQ(net->kind, "library");
    EXPECT_EQ(iface->kind, "interface");
    EXPECT_EQ(alias->kind, "alias");
    EXPECT_EQ(alias->alias_of, "core");
    EXPECT_EQ(app->kind, "executable");
}

TEST(CMakeAnalyzerTest, ParseDirectory_SourcesAreDirectoryQualified) {
    // Sources are relative to the CMakeLists.txt that declares them, so the
    // graph must record them under the fixture's directory — the same shape
    // the /source allowlist uses.
    CMakeAnalyzer analyzer;
    const CMakeGraph g = analyzer.parse_directory(std::string(SOURCE_DIR) + "/test_files/cmake");

    const std::string prefix = std::string(SOURCE_DIR) + "/test_files/cmake/";

    const CMakeTarget* core = find_target(g, "core");
    const CMakeTarget* net = find_target(g, "net");
    const CMakeTarget* app = find_target(g, "app");
    ASSERT_NE(core, nullptr);
    ASSERT_NE(net, nullptr);
    ASSERT_NE(app, nullptr);

    // core: add_library list + target_sources, all under the fixture dir.
    ASSERT_EQ(core->sources.size(), 3u);
    EXPECT_EQ(core->sources[0], prefix + "core.cpp");
    EXPECT_EQ(core->sources[1], prefix + "core.h");
    EXPECT_EQ(core->sources[2], prefix + "common.h");

    EXPECT_EQ(net->sources.size(), 1u);
    EXPECT_EQ(net->sources[0], prefix + "net.cpp");

    EXPECT_EQ(app->sources.size(), 1u);
    EXPECT_EQ(app->sources[0], prefix + "app.cpp");
}

TEST(CMakeAnalyzerTest, ParseDirectory_LinksKeepOnlyRealTargetNames) {
    CMakeAnalyzer analyzer;
    const CMakeGraph g = analyzer.parse_directory(std::string(SOURCE_DIR) + "/test_files/cmake");

    const CMakeTarget* core = find_target(g, "core");
    const CMakeTarget* net = find_target(g, "net");
    const CMakeTarget* iface = find_target(g, "iface");
    const CMakeTarget* app = find_target(g, "app");
    ASSERT_NE(core, nullptr);
    ASSERT_NE(net, nullptr);
    ASSERT_NE(iface, nullptr);
    ASSERT_NE(app, nullptr);

    EXPECT_EQ(core->links, (std::vector<std::string>{"util"}));
    EXPECT_EQ(net->links, (std::vector<std::string>{"core"}));
    EXPECT_EQ(iface->links, (std::vector<std::string>{"net"}));
    // ${CMAKE_DL_LIBS}, -lz and $<TARGET_NAME:core> are filtered out.
    EXPECT_EQ(app->links, (std::vector<std::string>{"net", "util"}));
}

TEST(CMakeAnalyzerTest, ParseDirectory_MissingRoot_YieldsEmptyGraph) {
    CMakeAnalyzer analyzer;
    EXPECT_TRUE(analyzer.parse_directory("/nonexistent/cmake_analyzer_test/no_such_dir").empty());
}
