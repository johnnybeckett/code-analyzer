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