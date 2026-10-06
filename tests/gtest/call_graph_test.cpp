#include <gtest/gtest.h>
#include "parser/call_scanner.h"
#include "parser/cpp_parser.h"
#include "parser/csharp_parser.h"
#include "parser/python_parser.h"
#include "core/model.h"
#include "core/json_serializer.h"
#include <boost/json.hpp>
#include <fstream>
#include <filesystem>
#include <string>
#include <memory>
#include <vector>
#include <unordered_set>
#include <algorithm>

namespace {

std::string write_temp(const std::string& name, const std::string& ext, const std::string& body) {
    auto dir = std::filesystem::temp_directory_path() / "call_graph_test";
    std::filesystem::create_directories(dir);
    auto path = dir / (name + ext);
    std::ofstream out(path);
    out << body;
    out.close();
    return path.string();
}

const Class* find_class(const std::vector<std::unique_ptr<Class>>& classes, const std::string& name) {
    for (const auto& c : classes) {
        if (c->name == name) return c.get();
    }
    return nullptr;
}

const Method* find_method(const Class* cls, const std::string& name) {
    if (!cls) return nullptr;
    for (const auto& m : cls->methods) {
        if (m->name == name) return m.get();
    }
    return nullptr;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

// An empty blacklist: CallScanner should surface every genuine call token.
const std::unordered_set<std::string> kEmptyBlacklist;

} // namespace

// ---------------------------------------------------------------------------
// White-box: CallScanner::extract — the shared, language-agnostic call-edge
// extractor. One rule: a "call" is any `(` whose immediately-preceding token
// (ignoring whitespace) is an identifier. Everything else is rejected.
// ---------------------------------------------------------------------------

TEST(CallScannerTest, DetectsSimpleCall) {
    auto calls = CallScanner::extract("int x = foo(1);", kEmptyBlacklist);
    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0], "foo");
}

TEST(CallScannerTest, DetectsQualifiedForms) {
    auto calls = CallScanner::extract("obj.foo(); a->bar(); A::baz();", kEmptyBlacklist);
    ASSERT_EQ(calls.size(), 3u);
    EXPECT_EQ(calls[0], "foo");
    EXPECT_EQ(calls[1], "bar");
    EXPECT_EQ(calls[2], "baz");
}

TEST(CallScannerTest, DetectsCallAtStartOfBody) {
    auto calls = CallScanner::extract("foo(1);", kEmptyBlacklist);
    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0], "foo");
}

TEST(CallScannerTest, WhitespaceBeforeParenIsIgnored) {
    auto calls = CallScanner::extract("foo (1);", kEmptyBlacklist);
    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0], "foo");
}

TEST(CallScannerTest, RejectsGroupingParens) {
    auto calls = CallScanner::extract("int y = (a + b) * 2;", kEmptyBlacklist);
    EXPECT_TRUE(calls.empty());
}

TEST(CallScannerTest, RejectsCStyleCast) {
    auto calls = CallScanner::extract("int z = (int)x;", kEmptyBlacklist);
    EXPECT_TRUE(calls.empty());
}

TEST(CallScannerTest, RejectsTemplateCast) {
    auto calls = CallScanner::extract("auto w = static_cast<int>(x);", kEmptyBlacklist);
    EXPECT_TRUE(calls.empty());
}

TEST(CallScannerTest, StripsBlacklistedKeywords) {
    static const std::unordered_set<std::string> blacklist = {"if", "for", "while"};
    auto calls = CallScanner::extract("if (x) { for (i) { } doStuff(); }", blacklist);
    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0], "doStuff");
}

TEST(CallScannerTest, DeduplicatesPreservingFirstSeenOrder) {
    auto calls = CallScanner::extract("foo(); bar(); foo(); baz(); bar();", kEmptyBlacklist);
    ASSERT_EQ(calls.size(), 3u);
    EXPECT_EQ(calls[0], "foo");
    EXPECT_EQ(calls[1], "bar");
    EXPECT_EQ(calls[2], "baz");
}

TEST(CallScannerTest, EmptyBodyYieldsEmpty) {
    EXPECT_TRUE(CallScanner::extract("", kEmptyBlacklist).empty());
}

// ---------------------------------------------------------------------------
// Parser-level: each language parser must populate Method::called_methods from
// an inline / concrete method body, keeping genuine callees and dropping the
// language's control-flow / cast / allocation keywords.
// ---------------------------------------------------------------------------

TEST(CppCallGraphTest, InlineMethodCapturesCallees) {
    auto file = write_temp("cpp_calls", ".cpp",
        "class Widget {\n"
        "    int helper();\n"
        "public:\n"
        "    int compute() {\n"
        "        int x = helper();\n"
        "        if (x > 0) {\n"
        "            return helper();\n"
        "        }\n"
        "        return 0;\n"
        "    }\n"
        "};\n");

    auto classes = CppParser::parse_file(file);
    const Class* widget = find_class(classes, "Widget");
    ASSERT_NE(widget, nullptr) << "Widget not parsed";
    const Method* compute = find_method(widget, "compute");
    ASSERT_NE(compute, nullptr) << "compute() not parsed";

    EXPECT_TRUE(contains(compute->called_methods, "helper"))
        << "expected callee 'helper' in compute()";
    EXPECT_FALSE(contains(compute->called_methods, "if"))
        << "keyword 'if' must not be a callee";
    EXPECT_FALSE(contains(compute->called_methods, "return"))
        << "keyword 'return' must not be a callee";
}

TEST(CSharpCallGraphTest, MethodCapturesCallees) {
    auto file = write_temp("csharp_calls", ".cs",
        "namespace Test\n"
        "{\n"
        "    class Calc\n"
        "    {\n"
        "        public int helper()\n"
        "        {\n"
        "            return 42;\n"
        "        }\n"
        "\n"
        "        public int compute()\n"
        "        {\n"
        "            int x = helper();\n"
        "            foreach (var item in list)\n"
        "            {\n"
        "                Console.WriteLine(item);\n"
        "            }\n"
        "            return x;\n"
        "        }\n"
        "    }\n"
        "}\n");

    auto classes = CSharpParser::parse_file(file);
    const Class* calc = find_class(classes, "Calc");
    ASSERT_NE(calc, nullptr) << "Calc not parsed";
    const Method* compute = find_method(calc, "compute");
    ASSERT_NE(compute, nullptr) << "compute() not parsed";

    EXPECT_TRUE(contains(compute->called_methods, "helper"))
        << "expected callee 'helper' in compute()";
    EXPECT_TRUE(contains(compute->called_methods, "WriteLine"))
        << "expected callee 'WriteLine' in compute()";
    EXPECT_FALSE(contains(compute->called_methods, "foreach"))
        << "keyword 'foreach' must not be a callee";
    EXPECT_FALSE(contains(compute->called_methods, "return"))
        << "keyword 'return' must not be a callee";
}

// A file with two top-level types: both must resolve their namespace
// absolutely and each method must carry its callee set. (Regression: the
// advancing regex `search_start` made `position(0)` relative, silently
// corrupting the 2nd+ type's body/namespace extraction.)
TEST(CSharpCallGraphTest, MultiClassFileAllMethodsCarryCallees) {
    auto file = write_temp("csharp_multi", ".cs",
        "namespace App\n"
        "{\n"
        "    class Alpha\n"
        "    {\n"
        "        public int shared()\n        {\n            return 1;\n        }\n"
        "        public int run()\n        {\n            return shared();\n        }\n"
        "    }\n"
        "    class Beta\n"
        "    {\n"
        "        public int helper()\n        {\n            return 2;\n        }\n"
        "        public int go()\n        {\n"
        "            Console.WriteLine(helper());\n"
        "            return helper();\n"
        "        }\n"
        "    }\n"
        "}\n");

    auto classes = CSharpParser::parse_file(file);
    const Class* alpha = find_class(classes, "Alpha");
    const Class* beta = find_class(classes, "Beta");
    ASSERT_NE(alpha, nullptr) << "Alpha not parsed";
    ASSERT_NE(beta, nullptr) << "Beta not parsed";

    // Both types attribute to the enclosing namespace ...
    EXPECT_EQ(alpha->full_namespace, "App");
    EXPECT_EQ(beta->full_namespace, "App")
        << "second type in a file must resolve its namespace absolutely";

    // ... and the second type's methods must carry their callee set.
    const Method* go = find_method(beta, "go");
    ASSERT_NE(go, nullptr) << "Beta::go() not parsed";
    EXPECT_TRUE(contains(go->called_methods, "helper"))
        << "expected callee 'helper' in Beta::go()";
    EXPECT_TRUE(contains(go->called_methods, "WriteLine"))
        << "expected callee 'WriteLine' in Beta::go()";

    const Method* run = find_method(alpha, "run");
    ASSERT_NE(run, nullptr) << "Alpha::run() not parsed";
    EXPECT_TRUE(contains(run->called_methods, "shared"))
        << "expected callee 'shared' in Alpha::run()";
}

TEST(PythonCallGraphTest, MethodCapturesCallees) {
    auto file = write_temp("python_calls", ".py",
        "class Calc:\n"
        "    def helper(self):\n"
        "        return 42\n"
        "\n"
        "    def compute(self):\n"
        "        x = self.helper()\n"
        "        if x > 0:\n"
        "            print(x)\n"
        "        return x\n");

    auto classes = PythonParser::parse_file(file);
    const Class* calc = find_class(classes, "Calc");
    ASSERT_NE(calc, nullptr) << "Calc not parsed";
    const Method* compute = find_method(calc, "compute");
    ASSERT_NE(compute, nullptr) << "compute() not parsed";

    EXPECT_TRUE(contains(compute->called_methods, "helper"))
        << "expected callee 'helper' in compute()";
    EXPECT_TRUE(contains(compute->called_methods, "print"))
        << "expected builtin callee 'print' in compute()";
    EXPECT_FALSE(contains(compute->called_methods, "if"))
        << "keyword 'if' must not be a callee";
    EXPECT_FALSE(contains(compute->called_methods, "return"))
        << "keyword 'return' must not be a callee";
}

// ---------------------------------------------------------------------------
// Black-box (round-trip): the serializer must emit `called_methods` per method
// so the viewer can build the whole-project call graph.
// ---------------------------------------------------------------------------

TEST(CallGraphSerializeTest, EmitsCalledMethodsPerMethod) {
    AnalysisResult result;
    auto cls = std::make_unique<Class>("Calc", "");
    auto method = std::make_unique<Method>("compute", "");
    method->called_methods = {"helper", "WriteLine"};
    cls->add_method(std::move(method));
    result.add_class(std::move(cls));

    boost::json::value doc = serialize(result, "/tmp/project");
    const auto& root = doc.as_object();
    const auto& classes = root.at("classes").as_array();
    ASSERT_EQ(classes.size(), 1u);
    const auto& cj = classes[0].as_object();
    const auto& methods = cj.at("methods").as_array();
    ASSERT_EQ(methods.size(), 1u);
    const auto& mj = methods[0].as_object();

    ASSERT_TRUE(mj.contains("called_methods"))
        << "method JSON must carry a called_methods array";
    const auto& calls = mj.at("called_methods").as_array();
    ASSERT_EQ(calls.size(), 2u);
    EXPECT_EQ(calls[0].as_string(), "helper");
    EXPECT_EQ(calls[1].as_string(), "WriteLine");
}

TEST(CallGraphSerializeTest, EmptyCalleesSerializeAsEmptyArray) {
    AnalysisResult result;
    auto cls = std::make_unique<Class>("Leaf", "");
    auto method = std::make_unique<Method>("no_op", "");
    cls->add_method(std::move(method));
    result.add_class(std::move(cls));

    boost::json::value doc = serialize(result, "/tmp/project");
    const auto& root = doc.as_object();
    const auto& classes = root.at("classes").as_array();
    const auto& mj = classes[0].as_object().at("methods").as_array()[0].as_object();

    ASSERT_TRUE(mj.contains("called_methods"))
        << "even a body-less method must emit an (empty) called_methods array";
    EXPECT_EQ(mj.at("called_methods").as_array().size(), 0u);
}
