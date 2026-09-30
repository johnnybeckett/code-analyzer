#include <gtest/gtest.h>
#include "parser/cpp_parser.h"
#include "parser/csharp_parser.h"
#include "parser/python_parser.h"
#include "core/model.h"
#include <fstream>
#include <filesystem>
#include <string>
#include <memory>
#include <vector>

namespace {

std::string fixture(const std::string& name) {
    return std::string(SOURCE_DIR) + "/test_files/" + name;
}

std::string write_temp(const std::string& name, const std::string& ext, const std::string& body) {
    auto dir = std::filesystem::temp_directory_path() / "cpp_kind_test";
    std::filesystem::create_directories(dir);
    auto path = dir / (name + ext);
    std::ofstream out(path);
    out << body;
    out.close();
    return path.string();
}

const Class* find(const std::vector<std::unique_ptr<Class>>& classes, const std::string& name) {
    for (const auto& c : classes) {
        if (c->name == name) return c.get();
    }
    return nullptr;
}

bool absent(const std::vector<std::unique_ptr<Class>>& classes, const std::string& name) {
    return find(classes, name) == nullptr;
}

} // namespace

// The shared fixture must parse into exactly the expected set of types,
// each with the right kind and namespace (nested, C++17-qualified,
// anonymous, and global namespaces all covered).
TEST(CppKindTest, FixtureKindsAndNamespaces) {
    auto classes = CppParser::parse_file(fixture("cxx_struct_namespace.cpp"));

    const Class* c = nullptr;

    c = find(classes, "NestedStruct");
    ASSERT_NE(c, nullptr) << "NestedStruct missing";
    EXPECT_EQ(c->kind, "struct");
    EXPECT_EQ(c->full_namespace, "one::two");

    c = find(classes, "NestedClass");
    ASSERT_NE(c, nullptr) << "NestedClass (final class) missing";
    EXPECT_EQ(c->kind, "class");
    EXPECT_EQ(c->full_namespace, "one::two");

    c = find(classes, "NestedUnion");
    ASSERT_NE(c, nullptr) << "NestedUnion missing";
    EXPECT_EQ(c->kind, "union");
    EXPECT_EQ(c->full_namespace, "one");

    c = find(classes, "DeepClass");
    ASSERT_NE(c, nullptr) << "DeepClass (C++17 qualified ns) missing";
    EXPECT_EQ(c->kind, "class");
    EXPECT_EQ(c->full_namespace, "one::four::five");

    c = find(classes, "AnonStruct");
    ASSERT_NE(c, nullptr) << "AnonStruct (anonymous ns) missing";
    EXPECT_EQ(c->kind, "struct");
    EXPECT_EQ(c->full_namespace, "(anonymous)");

    c = find(classes, "Templated");
    ASSERT_NE(c, nullptr) << "Templated (template class) missing";
    EXPECT_EQ(c->kind, "class");
    EXPECT_EQ(c->full_namespace, "");

    c = find(classes, "PlainStruct");
    ASSERT_NE(c, nullptr) << "PlainStruct (global struct) missing";
    EXPECT_EQ(c->kind, "struct");
    EXPECT_EQ(c->full_namespace, "");
}

// Scoped enums and string-literal false positives must not be captured.
TEST(CppKindTest, Rejections) {
    auto classes = CppParser::parse_file(fixture("cxx_struct_namespace.cpp"));

    EXPECT_TRUE(absent(classes, "Color")) << "enum class Color was captured as a type";
    EXPECT_TRUE(absent(classes, "Prio")) << "enum class Prio : int was captured as a type";
    EXPECT_TRUE(absent(classes, "FakeInString"))
        << "declaration inside a string literal was captured";
}

// A C# struct inside a dotted namespace must record kind "struct" and the
// model-wide `::` namespace form.
TEST(CSharpKindTest, StructInNamespace) {
    auto parsed = CSharpParser::parse_file(fixture("StructPoint.cs"));
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "Point");
    EXPECT_EQ(parsed->kind, "struct");
    EXPECT_EQ(parsed->full_namespace, "NS::Sub");
}

// A C# class keeps kind "class".
TEST(CSharpKindTest, ClassKind) {
    auto parsed = CSharpParser::parse_file(fixture("SampleClass.cs"));
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "SampleClass");
    EXPECT_EQ(parsed->kind, "class");
    EXPECT_EQ(parsed->full_namespace, "TestNamespace");
}

// Python classes record kind "class" and only plain base names (keyword
// arguments like `metaclass=` are skipped).
TEST(PythonKindTest, KindAndMetaclassSkipped) {
    auto file = write_temp("metaclass", ".py",
        "class Foo(Base, metaclass=Meta):\n"
        "    def method(self):\n"
        "        return 1\n");

    auto parsed = PythonParser::parse_file(file);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->name, "Foo");
    EXPECT_EQ(parsed->kind, "class");
    ASSERT_EQ(parsed->inheritance_list.size(), 1u);
    EXPECT_EQ(parsed->inheritance_list[0], "Base");
}
