#include <gtest/gtest.h>
#include "core/parser_registry.h"
#include "core/model.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace {

// Write a temp file and return its path
std::string write_temp(const std::string& name, const std::string& body) {
    auto dir = std::filesystem::temp_directory_path() / "registry_test";
    std::filesystem::create_directories(dir);
    auto path = dir / name;
    std::ofstream out(path);
    out << body;
    out.close();
    return path.string();
}

} // namespace

// An unregistered extension has no parser: create() returns nullptr rather
// than silently falling back to a default language.
TEST(ParserRegistryTest, UnknownExtensionHasNoParser) {
    ParserRegistry registry = ParserRegistry::standard();
    EXPECT_FALSE(registry.has_parser(".doesnotexist"));
    EXPECT_EQ(registry.create(".doesnotexist"), nullptr);
}

// Every standard extension (including the easy-to-drop ".hxx") resolves to a
// usable parser.
TEST(ParserRegistryTest, StandardExtensionsResolve) {
    ParserRegistry registry = ParserRegistry::standard();
    const char* const exts[] = {
        ".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hxx", ".hh", ".tpp", ".tcc",
        ".cs", ".py"
    };
    for (const auto* ext : exts) {
        EXPECT_TRUE(registry.has_parser(ext)) << "missing " << ext;
        EXPECT_NE(registry.create(ext), nullptr) << "no parser for " << ext;
    }
}

// The C++ adapter forwards a multi-class file unchanged (a C++ file may
// declare several classes).
TEST(ParserRegistryTest, CppAdapterParsesFile) {
    ParserRegistry registry = ParserRegistry::standard();
    auto file = write_temp("two_classes.cpp",
        "class Alpha { public: int a; };\n"
        "class Beta { public: int b; };\n");
    auto parser = registry.create(".cpp");
    ASSERT_NE(parser, nullptr);
    auto classes = parser->parse_file(file);
    ASSERT_EQ(classes.size(), 2u);
    EXPECT_EQ(classes[0]->name, "Alpha");
    EXPECT_EQ(classes[1]->name, "Beta");
}

// The C# adapter lifts its 0-or-1 result into a vector: one class -> size 1.
TEST(ParserRegistryTest, CSharpAdapterWrapsSingleClass) {
    ParserRegistry registry = ParserRegistry::standard();
    auto file = write_temp("one.cs", "public class Gamma { public int g; }\n");
    auto parser = registry.create(".cs");
    ASSERT_NE(parser, nullptr);
    auto classes = parser->parse_file(file);
    ASSERT_EQ(classes.size(), 1u);
    EXPECT_EQ(classes[0]->name, "Gamma");
}

// A C# file with no type yields an empty vector (the 0 case of the wrap).
TEST(ParserRegistryTest, CSharpAdapterEmptyWhenNoClass) {
    ParserRegistry registry = ParserRegistry::standard();
    auto file = write_temp("none.cs", "// no types here\nint x = 0;\n");
    auto parser = registry.create(".cs");
    ASSERT_NE(parser, nullptr);
    EXPECT_EQ(parser->parse_file(file).size(), 0u);
}

// The Python adapter lifts its single class into a one-element vector.
TEST(ParserRegistryTest, PythonAdapterWrapsSingleClass) {
    ParserRegistry registry = ParserRegistry::standard();
    auto file = write_temp("one.py", "class Delta:\n    x = 1\n");
    auto parser = registry.create(".py");
    ASSERT_NE(parser, nullptr);
    auto classes = parser->parse_file(file);
    ASSERT_EQ(classes.size(), 1u);
    EXPECT_EQ(classes[0]->name, "Delta");
}
