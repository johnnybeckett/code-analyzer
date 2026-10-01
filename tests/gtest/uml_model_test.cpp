#include <gtest/gtest.h>

#include "uml/uml_model.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

// Write an analyzer-shaped JSON fixture to a temp dir and return its path.
std::string write_json(const std::string& name, const std::string& content) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "uml_model_test";
    std::filesystem::create_directories(dir);
    const std::filesystem::path file = dir / name;
    std::ofstream out(file);
    out << content;
    return file.string();
}

// Extract the value of a `const <name> = <value>;` declaration from the page:
// everything after `const <name> = ` up to (not including) the closing `;`.
std::string section(const std::string& page, const std::string& name) {
    const std::string token = "const " + name + " = ";
    const std::size_t start = page.find(token);
    if (start == std::string::npos) {
        return {};
    }
    const std::size_t begin = start + token.size();
    const std::size_t end = page.find(';', begin);
    if (end == std::string::npos) {
        return {};
    }
    return page.substr(begin, end - begin);
}

}  // namespace

TEST(UmlModelTest, SingleFile_NotDiff) {
    const std::string single = write_json(
        "single.json",
        R"({"classes":[{"name":"Foo","namespace":"","methods":[],"attributes":[],"inheritance":[]}]})");

    uml::UmlModel model({single}, {});
    ASSERT_FALSE(model.is_diff());

    const std::string page = model.build_html();

    EXPECT_EQ(section(page, "DIFF_MODE"), "false");
    EXPECT_EQ(section(page, "OLD_CLASSES"), "[]");
    // The single file's classes land in NEW_CLASSES.
    EXPECT_NE(section(page, "NEW_CLASSES").find("Foo"), std::string::npos);
}

TEST(UmlModelTest, TwoFiles_Diff_SplicedOldAndNew) {
    const std::string older = write_json(
        "older.json",
        R"({"classes":[{"name":"OldOnly","namespace":"","methods":[],"attributes":[],"inheritance":[]},{"name":"Shared","namespace":"","methods":[],"attributes":[],"inheritance":[]}]})");
    const std::string newer = write_json(
        "newer.json",
        R"({"classes":[{"name":"Shared","namespace":"","methods":[],"attributes":[],"inheritance":[]},{"name":"NewOnly","namespace":"","methods":[],"attributes":[],"inheritance":[]}]})");

    uml::UmlModel model({older, newer}, {});
    ASSERT_TRUE(model.is_diff());

    const std::string page = model.build_html();
    const std::string old_side = section(page, "OLD_CLASSES");
    const std::string new_side = section(page, "NEW_CLASSES");

    EXPECT_EQ(section(page, "DIFF_MODE"), "true");

    // Old-only class is on the old side only.
    EXPECT_NE(old_side.find("OldOnly"), std::string::npos);
    EXPECT_EQ(new_side.find("OldOnly"), std::string::npos);
    // New-only class is on the new side only.
    EXPECT_NE(new_side.find("NewOnly"), std::string::npos);
    EXPECT_EQ(old_side.find("NewOnly"), std::string::npos);
    // The shared class is present in the newer side.
    EXPECT_NE(new_side.find("Shared"), std::string::npos);
}

TEST(UmlModelTest, MissingFile_Throws) {
    uml::UmlModel model({"/nonexistent/uml_model_test/no_such_file.json"}, {});
    EXPECT_THROW(model.build_html(), std::runtime_error);
}

TEST(UmlModelTest, BadJSON_Throws) {
    const std::string bad = write_json("bad.json", "this is { not valid json ");
    uml::UmlModel model({bad}, {});
    EXPECT_THROW(model.build_html(), std::runtime_error);
}

TEST(UmlModelTest, SourceFiles_SingleFile_DistinctFirstSeenOrder) {
    // Two classes share one file, a third points elsewhere; the empty "file"
    // is ignored. Order is document order, deduped.
    const std::string single = write_json(
        "src_single.json",
        R"({"classes":[
            {"name":"A","file":"/src/a.cpp"},
            {"name":"B","file":"/src/a.cpp"},
            {"name":"C","file":""},
            {"name":"D","file":"/src/d.cpp"}]})");

    uml::UmlModel model({single}, {});
    const auto sources = model.source_files();
    ASSERT_EQ(sources.size(), 2u);
    EXPECT_EQ(sources[0], "/src/a.cpp");
    EXPECT_EQ(sources[1], "/src/d.cpp");
}

TEST(UmlModelTest, SourceFiles_TwoFiles_UnionDedupFirstFileFirst) {
    // files[0] before files[1]; a path present in both appears once, in the
    // position it was first seen (files[0]).
    const std::string older = write_json(
        "src_older.json",
        R"({"classes":[{"name":"A","file":"/old/a.cpp"},{"name":"S","file":"/shared.cpp"}]})");
    const std::string newer = write_json(
        "src_newer.json",
        R"({"classes":[{"name":"S","file":"/shared.cpp"},{"name":"N","file":"/new/n.cpp"}]})");

    uml::UmlModel model({older, newer}, {});
    const auto sources = model.source_files();
    ASSERT_EQ(sources.size(), 3u);
    EXPECT_EQ(sources[0], "/old/a.cpp");
    EXPECT_EQ(sources[1], "/shared.cpp");
    EXPECT_EQ(sources[2], "/new/n.cpp");
}

TEST(UmlModelTest, SourceFiles_NoFileFields_Empty) {
    // Classes without a "file" field contribute nothing.
    const std::string single = write_json(
        "src_none.json",
        R"({"classes":[{"name":"A"},{"name":"B"}]})");

    uml::UmlModel model({single}, {});
    EXPECT_TRUE(model.source_files().empty());
}

TEST(UmlModelTest, SourceFiles_MissingFile_FailSoftEmpty) {
    uml::UmlModel model({"/nonexistent/uml_model_test/no_such_file.json"}, {});
    // source_files() is fail-soft: a missing input contributes nothing
    // (unlike build_html(), which must never serve an empty page).
    EXPECT_TRUE(model.source_files().empty());
}
