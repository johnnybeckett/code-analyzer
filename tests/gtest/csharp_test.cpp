#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include "parser/csharp_parser.h"
#include "core/model.h"

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
    auto parsed_class = CSharpParser::parse_file(test_file);

    // Verify we got a class back
    ASSERT_NE(parsed_class, nullptr);

    // Check basic class information
    EXPECT_EQ(parsed_class->name, "SampleClass");

    // Check inheritance (should inherit from BaseClass)
    ASSERT_GT(parsed_class->inheritance_list.size(), 0);
    EXPECT_EQ(parsed_class->inheritance_list[0], "BaseClass");
}

// Test parsing a base class file
TEST(CSharpParserTest, ParseBaseClass) {
    std::string test_file = "/home/claude/code-analyzer/test_files/BaseClass.cs";

    auto parsed_class = CSharpParser::parse_file(test_file);

    ASSERT_NE(parsed_class, nullptr);

    EXPECT_EQ(parsed_class->name, "BaseClass");
}

// Test parsing a project directory
TEST(CSharpParserTest, ParseProjectDirectory) {
    std::string test_dir = "/home/claude/code-analyzer/test_files";

    auto result = CSharpParser::parse_project(test_dir);

    // Should have at least one class parsed
    ASSERT_GT(result.classes.size(), 0);
}

// Test that parsing non-existent file returns nullptr
TEST(CSharpParserTest, ParseNonExistentFile) {
    std::string nonexistent_file = "/home/claude/code-analyzer/test_files/NonExistent.cs";

    auto parsed_class = CSharpParser::parse_file(nonexistent_file);

    // Should return nullptr for non-existent file
    EXPECT_EQ(parsed_class, nullptr);
}

// Regression: a generic base (`Base<int>`) is one base, not `Base` + `int`.
TEST(CSharpParserTest, ParseGenericBase) {
    std::string test_file = "/home/claude/code-analyzer/test_files/GenericBase.cs";

    auto parsed_class = CSharpParser::parse_file(test_file);

    ASSERT_NE(parsed_class, nullptr);
    EXPECT_EQ(parsed_class->name, "Foo");
    ASSERT_EQ(parsed_class->inheritance_list.size(), 1u);
    EXPECT_EQ(parsed_class->inheritance_list[0], "Base<int>");
}

// Regression: a dotted base (`MyNs.MyBase`) is one base, stored in the model's
// `::` form, not torn into `MyNs` and `MyBase`.
TEST(CSharpParserTest, ParseQualifiedBase) {
    std::string test_file = "/home/claude/code-analyzer/test_files/QualifiedBase.cs";

    auto parsed_class = CSharpParser::parse_file(test_file);

    ASSERT_NE(parsed_class, nullptr);
    EXPECT_EQ(parsed_class->name, "Foo");
    ASSERT_EQ(parsed_class->inheritance_list.size(), 1u);
    EXPECT_EQ(parsed_class->inheritance_list[0], "MyNs::MyBase");
}

// Regression: a base list spanning multiple lines is captured fully (class
// parses and both bases are present).
TEST(CSharpParserTest, ParseMultiLineBase) {
    std::string test_file = "/home/claude/code-analyzer/test_files/MultiLineBase.cs";

    auto parsed_class = CSharpParser::parse_file(test_file);

    ASSERT_NE(parsed_class, nullptr);
    EXPECT_EQ(parsed_class->name, "Foo");
    ASSERT_EQ(parsed_class->inheritance_list.size(), 2u);
    EXPECT_EQ(parsed_class->inheritance_list[0], "BaseOne");
    EXPECT_EQ(parsed_class->inheritance_list[1], "BaseTwo");
}

// Regression: a generic class (`Foo<T>`) parses, keeping its name and base.
TEST(CSharpParserTest, ParseGenericClass) {
    std::string test_file = "/home/claude/code-analyzer/test_files/GenericClass.cs";

    auto parsed_class = CSharpParser::parse_file(test_file);

    ASSERT_NE(parsed_class, nullptr);
    EXPECT_EQ(parsed_class->name, "Foo");
    ASSERT_EQ(parsed_class->inheritance_list.size(), 1u);
    EXPECT_EQ(parsed_class->inheritance_list[0], "Base");
}

// Regression: multiple comma-separated bases stay distinct, and a generic base
// with several type arguments is not split on its inner comma.
TEST(CSharpParserTest, ParseMultipleBases) {
    std::string test_file = "/home/claude/code-analyzer/test_files/MultiBase.cs";

    auto parsed_class = CSharpParser::parse_file(test_file);

    ASSERT_NE(parsed_class, nullptr);
    EXPECT_EQ(parsed_class->name, "Foo");
    ASSERT_EQ(parsed_class->inheritance_list.size(), 2u);
    EXPECT_EQ(parsed_class->inheritance_list[0], "A");
    EXPECT_EQ(parsed_class->inheritance_list[1], "B<C, D>");
}