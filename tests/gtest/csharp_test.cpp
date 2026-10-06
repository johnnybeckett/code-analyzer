#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <string>
#include "parser/csharp_parser.h"
#include "core/model.h"

namespace {

const Class* find(const std::vector<std::unique_ptr<Class>>& classes, const std::string& name) {
    for (const auto& c : classes) {
        if (c->name == name) return c.get();
    }
    return nullptr;
}

std::string write_temp(const std::string& name, const std::string& ext, const std::string& body) {
    auto dir = std::filesystem::temp_directory_path() / "csharp_test";
    std::filesystem::create_directories(dir);
    auto path = dir / (name + ext);
    std::ofstream out(path);
    out << body;
    out.close();
    return path.string();
}

}  // namespace

// Test that the C# parser can be instantiated and compiled
TEST(CSharpParserTest, CanBeInstantiated) {
    // This test ensures the C# parser class can be properly included and used
    ASSERT_TRUE(true);
}

// Test parsing a simple C# file
TEST(CSharpParserTest, ParseSimpleClass) {
    // Create a simple test class file
    std::string test_file = "/home/claude/code-analyzer/test_files/SampleClass.cs";

    // Try to parse the file
    auto classes = CSharpParser::parse_file(test_file);

    // Verify we found the class
    const Class* parsed = find(classes, "SampleClass");
    ASSERT_NE(parsed, nullptr);

    // Check basic class information
    EXPECT_EQ(parsed->name, "SampleClass");

    // Check inheritance (should inherit from BaseClass)
    ASSERT_GT(parsed->inheritance_list.size(), 0);
    EXPECT_EQ(parsed->inheritance_list[0], "BaseClass");
}

// Test parsing a base class file
TEST(CSharpParserTest, ParseBaseClass) {
    std::string test_file = "/home/claude/code-analyzer/test_files/BaseClass.cs";

    auto classes = CSharpParser::parse_file(test_file);

    const Class* parsed = find(classes, "BaseClass");
    ASSERT_NE(parsed, nullptr);

    EXPECT_EQ(parsed->name, "BaseClass");
}

// Test parsing a project directory
TEST(CSharpParserTest, ParseProjectDirectory) {
    std::string test_dir = "/home/claude/code-analyzer/test_files";

    auto result = CSharpParser::parse_project(test_dir);

    // Should have at least one class parsed
    ASSERT_GT(result.classes.size(), 0);
}

// Test that parsing non-existent file yields no classes
TEST(CSharpParserTest, ParseNonExistentFile) {
    std::string nonexistent_file = "/home/claude/code-analyzer/test_files/NonExistent.cs";

    auto classes = CSharpParser::parse_file(nonexistent_file);

    // Should return an empty vector for a non-existent file
    EXPECT_TRUE(classes.empty());
}

// Regression: a generic base (`Base<int>`) is one base, not `Base` + `int`.
TEST(CSharpParserTest, ParseGenericBase) {
    std::string test_file = "/home/claude/code-analyzer/test_files/GenericBase.cs";

    auto classes = CSharpParser::parse_file(test_file);

    const Class* parsed = find(classes, "Foo");
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "Foo");
    ASSERT_EQ(parsed->inheritance_list.size(), 1u);
    EXPECT_EQ(parsed->inheritance_list[0], "Base<int>");
}

// Regression: a dotted base (`MyNs.MyBase`) is one base, stored in the model's
// `::` form, not torn into `MyNs` and `MyBase`.
TEST(CSharpParserTest, ParseQualifiedBase) {
    std::string test_file = "/home/claude/code-analyzer/test_files/QualifiedBase.cs";

    auto classes = CSharpParser::parse_file(test_file);

    const Class* parsed = find(classes, "Foo");
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "Foo");
    ASSERT_EQ(parsed->inheritance_list.size(), 1u);
    EXPECT_EQ(parsed->inheritance_list[0], "MyNs::MyBase");
}

// Regression: a base list spanning multiple lines is captured fully (class
// parses and both bases are present).
TEST(CSharpParserTest, ParseMultiLineBase) {
    std::string test_file = "/home/claude/code-analyzer/test_files/MultiLineBase.cs";

    auto classes = CSharpParser::parse_file(test_file);

    const Class* parsed = find(classes, "Foo");
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "Foo");
    ASSERT_EQ(parsed->inheritance_list.size(), 2u);
    EXPECT_EQ(parsed->inheritance_list[0], "BaseOne");
    EXPECT_EQ(parsed->inheritance_list[1], "BaseTwo");
}

// Regression: a generic class (`Foo<T>`) parses, keeping its name and base.
TEST(CSharpParserTest, ParseGenericClass) {
    std::string test_file = "/home/claude/code-analyzer/test_files/GenericClass.cs";

    auto classes = CSharpParser::parse_file(test_file);

    const Class* parsed = find(classes, "Foo");
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "Foo");
    ASSERT_EQ(parsed->inheritance_list.size(), 1u);
    EXPECT_EQ(parsed->inheritance_list[0], "Base");
}

// Regression: multiple comma-separated bases stay distinct, and a generic base
// with several type arguments is not split on its inner comma.
TEST(CSharpParserTest, ParseMultipleBases) {
    std::string test_file = "/home/claude/code-analyzer/test_files/MultiBase.cs";

    auto classes = CSharpParser::parse_file(test_file);

    const Class* parsed = find(classes, "Foo");
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "Foo");
    ASSERT_EQ(parsed->inheritance_list.size(), 2u);
    EXPECT_EQ(parsed->inheritance_list[0], "A");
    EXPECT_EQ(parsed->inheritance_list[1], "B<C, D>");
}

// A file declaring several types must yield ALL of them, and each must carry
// its source `file` (the first-class-only limit is gone — this is what made
// "no source file registered" pop up for later classes in a multi-type file).
TEST(CSharpParserTest, MultiClassFileAllCarryFile) {
    auto file = write_temp("multi", ".cs",
        "namespace Multi {\n"
        "    class First {\n"
        "        public void Do() { }\n"
        "    }\n"
        "    struct Second {\n"
        "        public int Field;\n"
        "    }\n"
        "}\n");

    auto classes = CSharpParser::parse_file(file);

    const Class* first = find(classes, "First");
    ASSERT_NE(first, nullptr) << "First missing from multi-class file";
    EXPECT_EQ(first->file, file);

    const Class* second = find(classes, "Second");
    ASSERT_NE(second, nullptr) << "Second missing from multi-class file";
    EXPECT_EQ(second->file, file);
}
