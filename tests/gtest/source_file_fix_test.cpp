#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/directory_provider.h"
#include "server/source_resolver.h"

namespace {

namespace fs = std::filesystem;

void write_file(const fs::path& path, const std::string& body) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    out << body;
}

}  // namespace

// The root cause of "no source file registered when setting the focus": with a
// relative project root the analyzer recorded a relative `file` per class, and
// the server then failed to open it from its own CWD. The provider must anchor
// every path to absolute, so the JSON `file` field is CWD-independent.
TEST(SourceFileFix, ProviderYieldsAbsolutePathsRegardlessOfCwd) {
    const auto root = fs::temp_directory_path() / "source_file_fix_abs";
    fs::remove_all(root);
    write_file(root / "resolver_fixture.cpp", "class Widget { public: int w; };\n");

    // Express the root RELATIVE to the test's CWD, then confirm the provider
    // still returns an absolute path (the JSON `file` field is the CWD-proof
    // path the server must later resolve).
    const fs::path cwd = fs::current_path();
    const fs::path relative_root = fs::relative(root, cwd);
    ASSERT_FALSE(relative_root.is_absolute());  // sanity: we really did make it relative

    DirectoryFileProvider provider(relative_root.string(), {});
    auto files = provider.files();
    ASSERT_EQ(files.size(), 1u);
    EXPECT_TRUE(fs::path(files.front()).is_absolute());
    EXPECT_EQ(fs::weakly_canonical(fs::path(files.front())),
              fs::weakly_canonical(root / "resolver_fixture.cpp"));
}

// A recorded path that already resolves as-is (here: absolute) is returned
// unchanged — the fast path for the common case.
TEST(SourceFileFix, ResolverFindsAbsolutePathAsIs) {
    const auto file = fs::temp_directory_path() / "source_file_fix_as" / "real.cpp";
    fs::remove_all(file.parent_path());
    write_file(file, "class Real { public: int r; };\n");

    const auto data = server::resolve_source(file.string(), fs::path("/"));
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(*data, "class Real { public: int r; };\n");
}

// The reported bug: the analyzer ran from one CWD (recording a relative
// `file`), the server from another. Resolving the relative path under the
// input JSON's directory must still find the file — CWD-independent, since the
// JSON dir is passed explicitly rather than inferred from the process CWD.
TEST(SourceFileFix, ResolverFindsRelativePathUnderJsonDir) {
    const auto json_dir = fs::temp_directory_path() / "source_file_fix_rel" / "proj";
    fs::remove_all(json_dir.parent_path());
    write_file(json_dir / "resolver_fixture_widget.cpp",
               "class Widget { public: int w; };\n");

    // Recorded relative to the project, NOT under the test CWD — so only the
    // json_dir candidate can locate it.
    const auto data = server::resolve_source("resolver_fixture_widget.cpp", json_dir);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(*data, "class Widget { public: int w; };\n");
}

// A path that exists nowhere yields std::nullopt — the caller warns and the
// route 404s for it, rather than the server guessing some other location.
TEST(SourceFileFix, ResolverMissesWhenNowhereReadable) {
    const auto data =
        server::resolve_source("no/such/resolver_fixture_xyz.cpp", fs::path("/"));
    EXPECT_FALSE(data.has_value());
}
