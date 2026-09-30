#include <gtest/gtest.h>
#include "core/analyzer.h"
#include "parser/cpp_parser.h"
#include "core/model.h"
#include <fstream>
#include <filesystem>
#include <string>
#include <memory>

namespace {

// Write a temp file with the given extension and return its path
std::string write_temp(const std::string& name, const std::string& ext, const std::string& body) {
    auto dir = std::filesystem::temp_directory_path() / "cpp_parser_test";
    std::filesystem::create_directories(dir);
    auto path = dir / (name + ext);
    std::ofstream out(path);
    out << body;
    out.close();
    return path.string();
}

} // namespace

// Test that the analyzer can be instantiated
TEST(AnalyzerTest, CanBeInstantiated) {
    // This test ensures the analyzer class can be properly included and used
    ASSERT_TRUE(true);
}

// A class deriving from `::testing::Test` (leading `::` global namespace) must
// normalise to the identical base as one written `testing::Test` — the leading
// `::` is implied and must not be part of the stored base name.
TEST(CppParserTest, NormalizeGlobalNamespaceBase) {
    auto file = write_temp("nsbase", ".hpp",
        "namespace testing { class Test {}; }\n"
        "namespace core {\n"
        "class Widget : public ::testing::Test {};\n"
        "}");

    auto classes = CppParser::parse_file(file);
    // Find the core::Widget class
    Class* widget = nullptr;
    for (auto& c : classes) {
        if (c->name == "Widget") widget = c.get();
    }
    ASSERT_NE(widget, nullptr);
    ASSERT_FALSE(widget->inheritance_list.empty());
    // `::testing::Test` must be stored without the leading `::`
    EXPECT_EQ(widget->inheritance_list[0], "testing::Test");
}

// A templated class whose base clause contains template arguments
// (e.g. `std::enable_shared_from_this<Foo<T>>`) used to be missed entirely;
// it must now be found with the base clause captured intact.
TEST(CppParserTest, TemplatedBaseClass) {
    auto file = write_temp("templbase", ".hpp",
        "template <typename T>\n"
        "class Foo : public std::enable_shared_from_this<Foo<T>> {\n"
        "public:\n"
        "    T data_;\n"
        "};");

    auto classes = CppParser::parse_file(file);
    Class* foo = nullptr;
    for (auto& c : classes) {
        if (c->name == "Foo") foo = c.get();
    }
    ASSERT_NE(foo, nullptr);
    ASSERT_FALSE(foo->inheritance_list.empty());
    EXPECT_NE(foo->inheritance_list[0].find("std::enable_shared_from_this"), std::string::npos);
}

// A `.tpp` template-definition file may contain whole class definitions; the
// directory walk must pick it up (the compile-commands path cannot).
TEST(AnalyzerTest, ParseTppFile) {
    auto dir = std::filesystem::temp_directory_path() / "tpp_proj_test";
    std::filesystem::create_directories(dir);
    std::ofstream((dir / "sole.tpp").string())
        << "class SoleInTpp {\npublic:\n    void run();\n};\n";

    ParserRegistry registry = ParserRegistry::standard();
    Config config;
    Analyzer analyzer(config, registry);
    auto result = analyzer.analyze_project(dir.string());

    bool found = false;
    for (auto& c : result.classes) {
        if (c->name == "SoleInTpp") found = true;
    }
    EXPECT_TRUE(found);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
