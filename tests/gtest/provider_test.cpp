#include <gtest/gtest.h>
#include "core/analyzer.h"
#include "core/commit_provider.h"
#include "core/compile_commands_provider.h"
#include "core/directory_provider.h"
#include "core/parser_registry.h"
#include "core/provider_registry.h"
#include <cstdio>
#include <cstdlib>
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

// --- git helpers for the commit-source tests ----------------------------

const char* kGitId = "-c user.email=t@t -c user.name=t -c commit.gpgsign=false";

bool git_run(const std::string& cmd) {
    return std::system(cmd.c_str()) == 0;
}

std::string git_capture(const std::string& cmd) {
    std::string out;
    std::FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return out;
    char buf[256];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, pipe)) > 0) out.append(buf, n);
    pclose(pipe);
    if (!out.empty() && out.back() == '\n') out.pop_back();
    return out;
}

// A scratch superproject with one committed submodule:
//   <base>/super/app.cpp                class AppMain
//   <base>/super/libs/thing/src/sub.cpp class SubWidget (via gitlink)
// <base>/sub_repo is the submodule's own repository.
void build_superproject(const fs::path& base) {
    fs::remove_all(base);
    auto sub = base / "sub_repo";
    fs::create_directories(sub);
    write_file(sub / "src" / "sub.cpp", "class SubWidget { public: int s; };\n");
    ASSERT_TRUE(git_run("cd '" + sub.string() + "' && git init -q && git add -A"
        " && git " + kGitId + " commit -q -m sub"));

    auto super = base / "super";
    fs::create_directories(super);
    write_file(super / "app.cpp", "class AppMain { public: int m; };\n");
    ASSERT_TRUE(git_run("cd '" + super.string() + "' && git init -q && git add app.cpp"
        " && git " + kGitId + " commit -q -m app"));
    // modern git blocks the file transport for submodule clones by default
    // (CVE-2022-39253); this is a local test fixture, so allow it here
    ASSERT_TRUE(git_run("cd '" + super.string() + "' && git -c protocol.file.allow=always"
        " submodule add -q '" + sub.string() + "' libs/thing"
        " && git " + kGitId + " commit -q -m pin"));
}

std::set<std::string> basenames(const std::vector<std::string>& files) {
    std::set<std::string> out;
    for (const auto& f : files) out.insert(fs::path(f).filename().string());
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

// The standard registry offers exactly the built-in kinds, and an unknown
// kind yields nullptr rather than a silently-wrong default provider.
TEST(ProviderRegistryTest, StandardHasAllKinds) {
    ProviderRegistry registry = ProviderRegistry::standard({".git", "build"});

    EXPECT_TRUE(registry.has_provider("directory"));
    EXPECT_TRUE(registry.has_provider("compile-commands"));
    EXPECT_TRUE(registry.has_provider("commit"));

    ProviderOptions opts;
    opts.path = "/some/root";
    EXPECT_NE(registry.create("directory", opts), nullptr);
    opts.path = "/some/db.json";
    EXPECT_NE(registry.create("compile-commands", opts), nullptr);
    opts = ProviderOptions{"/some/repo", "v1.0", ""};
    EXPECT_NE(registry.create("commit", opts), nullptr);

    EXPECT_FALSE(registry.has_provider("vcpkg"));
    EXPECT_EQ(registry.create("vcpkg", ProviderOptions{"", "", ""}), nullptr);
}

// skip_dirs registered on the registry flow through to the provider it builds.
TEST(ProviderRegistryTest, DirectoryProviderRespectsRegisteredSkipDirs) {
    auto root = fs::temp_directory_path() / "provider_registry_test";
    fs::remove_all(root);
    write_file(root / "app.cpp", "class AppMain { public: int m; };\n");
    write_file(root / "build" / "gen.cpp", "class Generated { public: int g; };\n");

    ProviderRegistry registry = ProviderRegistry::standard({"build"});
    ProviderOptions opts;
    opts.path = root.string();
    auto provider = registry.create("directory", opts);
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

// The commit source reads the superproject's files AND the submodule's files
// (the gitlink's path), with content taken from git objects at the pinned
// SHA — staged under a single root mirroring the project tree.
TEST(CommitProviderTest, ListsSuperprojectAndSubmoduleFiles) {
    auto base = fs::temp_directory_path() / "commit_provider_test";
    build_superproject(base);
    const auto super = base / "super";

    CommitFileProvider provider(super.string(), "HEAD");
    auto files = provider.files();

    std::string staged_app, staged_sub;
    for (const auto& f : files) {
        if (fs::path(f).filename() == "app.cpp") staged_app = f;
        if (fs::path(f).filename() == "sub.cpp") staged_sub = f;
    }
    // .gitmodules is committed by `git submodule add`, so it is part of the
    // tree too (the analyzer has no parser for it — it contributes no classes).
    EXPECT_EQ(basenames(files),
              (std::set<std::string>{".gitmodules", "app.cpp", "sub.cpp"}));
    ASSERT_FALSE(staged_app.empty());
    ASSERT_FALSE(staged_sub.empty());

    // Everything stages under one root mirroring the tree: sub.cpp sits
    // under its gitlink path relative to the same root as app.cpp.
    const fs::path root = fs::path(staged_app).parent_path();
    EXPECT_EQ(staged_sub.rfind(root.string(), 0), 0u);
    // sub.cpp sits at <staging>/libs/thing/src/sub.cpp — the gitlink's path
    // ("libs/thing") is preserved under the same root as app.cpp.
    EXPECT_EQ(fs::path(staged_sub).parent_path().parent_path().filename().string(),
              "thing");

    // Content is the submodule's committed text — proof of the SHA-pinned
    // git-show read, not whatever the working tree might hold.
    std::ifstream in(staged_sub);
    std::string body((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    EXPECT_EQ(body, "class SubWidget { public: int s; };\n");
}

// The ref may be any rev that resolves to a commit: a tag and the full SHA
// both yield the same file set as HEAD.
TEST(CommitProviderTest, RefByTagAndSha) {
    auto base = fs::temp_directory_path() / "commit_provider_test";
    build_superproject(base);
    const auto super = base / "super";
    ASSERT_TRUE(git_run("cd '" + super.string() + "' && git tag v1"));

    const std::string sha =
        git_capture("git -C '" + super.string() + "' rev-parse HEAD");
    ASSERT_FALSE(sha.empty());

    // includes .gitmodules (see ListsSuperprojectAndSubmoduleFiles)
    const auto expected = (std::set<std::string>{".gitmodules", "app.cpp", "sub.cpp"});
    CommitFileProvider by_ref(super.string(), "HEAD");
    CommitFileProvider by_tag(super.string(), "v1");
    CommitFileProvider by_sha(super.string(), sha);
    EXPECT_EQ(basenames(by_ref.files()), expected);
    EXPECT_EQ(basenames(by_tag.files()), expected);
    EXPECT_EQ(basenames(by_sha.files()), expected);
}

// An unresolvable ref degrades to an empty list (and a stderr warning) —
// the same behavior class as a missing compile database.
TEST(CommitProviderTest, MissingRefDegradesGracefully) {
    auto base = fs::temp_directory_path() / "commit_provider_test";
    build_superproject(base);
    const auto super = base / "super";

    CommitFileProvider provider(super.string(), "no-such-ref-xyz");
    EXPECT_TRUE(provider.files().empty());
    EXPECT_EQ(provider.banner(), "Analyzing commit no-such-ref-xyz of " + super.string());
}

// The default staging dir is created while the provider is alive and removed
// when it is destroyed (RAII); an explicit --staging dir is kept.
TEST(CommitProviderTest, AutoStagingIsCleanedExplicitStagingKept) {
    auto base = fs::temp_directory_path() / "commit_provider_test";
    build_superproject(base);
    const auto super = base / "super";

    auto count_auto = []() {
        std::size_t n = 0;
        std::error_code ec;
        for (auto it = fs::directory_iterator(fs::temp_directory_path(), ec);
             !ec && it != fs::directory_iterator(); ++it) {
            const auto name = it->path().filename().string();
            if (it->is_directory() && name.rfind("code_analyzer_commit_", 0) == 0) ++n;
        }
        return n;
    };

    const std::size_t before = count_auto();
    {
        CommitFileProvider provider(super.string(), "HEAD");
        auto files = provider.files();
        ASSERT_FALSE(files.empty());
        EXPECT_GT(count_auto(), before);          // created while alive
        EXPECT_TRUE(fs::exists(files.front()));
    }
    EXPECT_EQ(count_auto(), before);              // gone after destruction

    // An explicitly supplied staging directory is used and left in place.
    auto keep = fs::temp_directory_path() / "commit_provider_keep";
    fs::remove_all(keep);
    {
        CommitFileProvider provider(super.string(), "HEAD", keep.string());
        auto files = provider.files();
        ASSERT_FALSE(files.empty());
        EXPECT_EQ(fs::path(files.front()).root_path(), fs::weakly_canonical(keep).root_path());
    }
    EXPECT_TRUE(fs::exists(keep / "app.cpp"));
    fs::remove_all(keep);
}

// Parity: the commit source must find exactly the classes a directory walk
// of the same tree finds — including the submodule's class.
TEST(AnalyzerTest, CommitPathMatchesDirectoryPath) {
    auto base = fs::temp_directory_path() / "commit_provider_parity";
    build_superproject(base);
    const auto super = base / "super";

    Config config;
    ParserRegistry parsers = ParserRegistry::standard();
    Analyzer analyzer(config, parsers);

    auto via_commit = analyzer.analyze_commit(super.string(), "HEAD");
    auto via_directory = analyzer.analyze_project(super.string());

    EXPECT_EQ(via_commit.classes.size(), 2u);
    EXPECT_EQ(names(via_commit), names(via_directory));
    EXPECT_EQ(names(via_commit), (std::set<std::string>{"AppMain", "SubWidget"}));
}
