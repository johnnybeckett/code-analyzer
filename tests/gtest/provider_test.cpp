#include <gtest/gtest.h>
#include "core/analyzer.h"
#include "core/compile_commands_provider.h"
#include "core/directory_provider.h"
#include "core/parser_registry.h"
#include "core/provider_registry.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

// Write a temp file (creating parent directories) and return nothing —
// tests reference the path they passed in.
void write_file(const fs::path& path, const std::string& body) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    out << body;
}

std::set<std::string> names(const AnalysisResult& result) {
    std::set<std::string> out;
    for (const auto& c : result.classes) out.insert(c->name);
    return out;
}

} // namespace

// The directory provider walks recursively, returns only regular files, and
// prunes exactly the skip directories it was given (configurable, not fixed).
TEST(DirectoryProviderTest, ListsFilesAndPrunesSkipDirs) {
    auto root = fs::temp_directory_path() / "provider_dir_test";
    fs::remove_all(root);
    write_file(root / "app.cpp", "class AppMain { public: int m; };\n");
    write_file(root / "src" / "widget.hpp", "class Widget { public: int w; };\n");
    write_file(root / "src" / "detail" / "helper.tpp", "class Helper { public: int h; };\n");
    write_file(root / "generated" / "ignored.cpp", "class Ignored { public: int i; };\n");

    DirectoryFileProvider provider(root.string(), {"generated"});
    auto files = provider.files();

    std::set<std::string> basenames;
    for (const auto& f : files) basenames.insert(fs::path(f).filename().string());
    ASSERT_EQ(files.size(), 3u);
    EXPECT_EQ(basenames, (std::set<std::string>{"app.cpp", "widget.hpp", "helper.tpp"}));

    // Without the skip entry the generated file joins the file set
    DirectoryFileProvider unpruned(root.string(), {});
    EXPECT_EQ(unpruned.files().size(), 4u);
}

TEST(DirectoryProviderTest, BannerNamesTheRoot) {
    DirectoryFileProvider provider("/some/project", {});
    EXPECT_EQ(provider.banner(), "Analyzing project: /some/project");
}

// Only usable "file" entries are returned, in database order: non-objects,
// missing fields, non-string values, and empty paths are all skipped.
TEST(CompileCommandsProviderTest, ListsUsableFileEntriesInOrder) {
    auto dir = fs::temp_directory_path() / "provider_cc_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    auto db = dir / "compile_commands.json";
    {
        std::ofstream out(db);
        out << "[\n"
            << "  {\"directory\": \".\", \"command\": \"g++\", \"file\": \"/proj/app.cpp\"},\n"
            << "  {\"file\": \"/proj/src/widget.hpp\"},\n"
            << "  \"not an object\",\n"
            << "  {\"command\": \"no file field\"},\n"
            << "  {\"file\": 42},\n"
            << "  {\"file\": \"\"},\n"
            << "  {\"file\": \"/proj/src/detail/helper.tpp\"}\n"
            << "]\n";
    }

    CompileCommandsFileProvider provider(db.string());
    EXPECT_EQ(provider.files(), (std::vector<std::string>{
        "/proj/app.cpp", "/proj/src/widget.hpp", "/proj/src/detail/helper.tpp"}));
}

// A missing database reports on stderr and yields an empty list — the same
// graceful degradation the compile-database path has always had.
TEST(CompileCommandsProviderTest, MissingDatabaseDegradesGracefully) {
    auto path = fs::temp_directory_path() / "provider_cc_test" / "absent" / "db.json";
    CompileCommandsFileProvider provider(path.string());
    EXPECT_TRUE(provider.files().empty());
    EXPECT_EQ(provider.banner(), "Analyzing compile_commands.json: " + path.string());
}

TEST(CompileCommandsProviderTest, MalformedDatabaseDegradesGracefully) {
    auto dir = fs::temp_directory_path() / "provider_cc_test";
    fs::create_directories(dir);

    auto bad_json = dir / "bad.json";
    { std::ofstream out(bad_json); out << "{ this is not json"; }
    CompileCommandsFileProvider bad(bad_json.string());
    EXPECT_TRUE(bad.files().empty());

    auto not_array = dir / "not_array.json";
    { std::ofstream out(not_array); out << "{\"files\": []}"; }
    CompileCommandsFileProvider object(not_array.string());
    EXPECT_TRUE(object.files().empty());
}

// The standard registry offers exactly the two built-in kinds, and an unknown
// kind yields nullptr rather than a silently-wrong default provider.
TEST(ProviderRegistryTest, StandardHasBothKinds) {
    ProviderRegistry registry = ProviderRegistry::standard({".git", "build"});

    EXPECT_TRUE(registry.has_provider("directory"));
    EXPECT_TRUE(registry.has_provider("compile-commands"));
    EXPECT_NE(registry.create("directory", "/some/root"), nullptr);
    EXPECT_NE(registry.create("compile-commands", "/some/db.json"), nullptr);

    EXPECT_FALSE(registry.has_provider("vcpkg"));
    EXPECT_EQ(registry.create("vcpkg", "/whatever"), nullptr);
}

// skip_dirs registered on the registry flow through to the provider it builds.
TEST(ProviderRegistryTest, DirectoryProviderRespectsRegisteredSkipDirs) {
    auto root = fs::temp_directory_path() / "provider_registry_test";
    fs::remove_all(root);
    write_file(root / "app.cpp", "class AppMain { public: int m; };\n");
    write_file(root / "build" / "gen.cpp", "class Generated { public: int g; };\n");

    ProviderRegistry registry = ProviderRegistry::standard({"build"});
    auto provider = registry.create("directory", root.string());
    ASSERT_NE(provider, nullptr);
    EXPECT_EQ(provider->files().size(), 1u);
}

// The parity case that motivated the providers: with a compile database that
// lists the headers, both input modes must yield the same classes — including
// the header- and .tpp-declared ones that --compile-commands used to miss.
TEST(AnalyzerTest, CompileCommandsPathMatchesDirectoryPath) {
    auto root = fs::temp_directory_path() / "provider_parity_test";
    fs::remove_all(root);
    write_file(root / "app.cpp", "class AppMain { public: int m; };\n");
    write_file(root / "src" / "widget.hpp", "class Widget { public: int w; };\n");
    write_file(root / "src" / "detail" / "helper.tpp", "class Helper { public: int h; };\n");

    auto db_dir = fs::temp_directory_path() / "provider_parity_db";
    fs::create_directories(db_dir);
    auto db = db_dir / "compile_commands.json";
    {
        // fs::path streams via std::quoted, so append .string() to keep the
        // JSON valid — the raw path must not arrive pre-quoted
        std::ofstream out(db);
        out << "[\n"
            << "  {\"file\": \"" << (root / "app.cpp").string() << "\"},\n"
            << "  {\"file\": \"" << (root / "src" / "widget.hpp").string() << "\"},\n"
            << "  {\"file\": \"" << (root / "src" / "detail" / "helper.tpp").string() << "\"}\n"
            << "]\n";
    }

    Config config;
    ParserRegistry parsers = ParserRegistry::standard();
    Analyzer analyzer(config, parsers);

    auto via_directory = analyzer.analyze_project(root.string());
    auto via_database = analyzer.analyze_compile_commands(db.string());

    EXPECT_EQ(via_directory.classes.size(), 3u);
    EXPECT_EQ(names(via_directory), names(via_database));
    EXPECT_EQ(names(via_database), (std::set<std::string>{"AppMain", "Widget", "Helper"}));
}
