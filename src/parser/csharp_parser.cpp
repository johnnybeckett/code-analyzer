#include "parser/csharp_parser.h"
#include "parser/call_scanner.h"
#include "core/model.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>
#include <cctype>
#include <iterator>
#include <vector>
#include <unordered_set>
#include <filesystem>

namespace {

/**
 * @brief Find the index just past the '}' matching the '{' at open_pos
 * @return npos if braces are unbalanced
 */
size_t find_matching_brace(const std::string& content, size_t open_pos) {
    int depth = 0;
    for (size_t i = open_pos; i < content.size(); ++i) {
        if (content[i] == '{') ++depth;
        else if (content[i] == '}') {
            --depth;
            if (depth == 0) return i + 1;
        }
    }
    return std::string::npos;
}

/**
 * @brief Compute the full namespace path enclosing the given position
 *
 * Handles both the block form (`namespace A.B { ... }`) and nested
 * declarations (`namespace A { namespace B { ... } }`). C# uses dot
 * separators (`A.B`), which are converted to the `::` form used
 * everywhere else in the model so namespace grouping stays uniform.
 * @param pos Character offset of a type declaration
 * @return Namespace segments from outermost to innermost, joined by `::`
 */
std::string namespace_at(const std::string& content, size_t pos) {
    std::string path;
    const std::regex namespace_re(
        R"(\bnamespace\s+([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*)\s*\{)");
    for (auto it = std::sregex_iterator(content.cbegin(), content.cend(), namespace_re);
         it != std::sregex_iterator(); ++it) {
        size_t open_pos = it->position(0) + it->length(0) - 1;  // index of '{'
        size_t close_pos = find_matching_brace(content, open_pos);
        if (close_pos == std::string::npos) continue;           // unbalanced braces
        if (it->position(0) > pos || pos >= close_pos) continue;

        // Convert `A.B.C` into the `A::B::C` form used by the model
        std::string qualified = (*it)[1];
        for (char& c : qualified) {
            if (c == '.') c = ':';
        }
        // Each `.` separator becomes `::`, so collapse the single-colon
        // form produced above into the model's double-colon separators
        std::string converted;
        for (size_t i = 0; i < qualified.size(); ++i) {
            converted += qualified[i];
            if (qualified[i] == ':') converted += ':';
        }
        if (!path.empty()) path += "::";
        path += converted;
    }
    return path;
}

/**
 * @brief Split a C# base list into individual base types.
 *
 * Splits on top-level commas only, so commas inside a generic argument list
 * (e.g. `Base<A, B>`) do not split. Each piece is trimmed of surrounding
 * whitespace; empty pieces are dropped. Thus `Base<int>` and `MyNs.MyBase`
 * each remain a single base, while `A, B<C, D>` yields `A` and `B<C, D>`.
 * @param list The text after the `:`, up to the opening brace
 * @return The base types, in order, with no spurious fragments
 */
std::vector<std::string> split_bases(const std::string& list) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;  // nesting inside generic `<...>` argument lists
    for (char c : list) {
        if (c == '<') ++depth;
        else if (c == '>') { if (depth > 0) --depth; }
        else if (c == ',' && depth == 0) {
            const size_t b = cur.find_first_not_of(" \t\r\n");
            if (b != std::string::npos) {
                const size_t e = cur.find_last_not_of(" \t\r\n");
                out.push_back(cur.substr(b, e - b + 1));
            }
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }
    const size_t b = cur.find_first_not_of(" \t\r\n");
    if (b != std::string::npos) {
        const size_t e = cur.find_last_not_of(" \t\r\n");
        out.push_back(cur.substr(b, e - b + 1));
    }
    return out;
}

/**
 * @brief C# keywords that can appear as `<keyword>(` but are not calls
 *
 * Stripped by the shared CallScanner so only genuine callee names remain
 * (`typeof(`, `foreach(`, `lock(`, `sizeof(`, etc.).
 */
const std::unordered_set<std::string>& csharp_call_blacklist() {
    static const std::unordered_set<std::string> blacklist = {
        "if", "for", "foreach", "while", "switch", "catch", "using",
        "sizeof", "typeof", "new", "lock", "await", "checked", "unchecked",
        "throw", "return", "in", "when"
    };
    return blacklist;
}

} // namespace

/**
 * @brief Parse a C# file and extract class information
 * @param file_path Path to the C# file
 * @return All classes found in the file (empty on error or no types)
 */
std::vector<std::unique_ptr<Class>> CSharpParser::parse_file(const std::string& file_path) {
    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "Error: File does not exist - " << file_path << std::endl;
        return {};
    }

    // Read the entire file
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file - " << file_path << std::endl;
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Find type declarations: `class`, `struct`, and `record` (including
    // `record struct`). Group 1 is the declaration kind, the optional group
    // 2 covers the `struct` keyword of a `record struct` pair, group 3 is the
    // type name, the optional group 4 is a generic argument list (`Foo<T>`),
    // and the optional group 5 is the base list after `:`. The base list is
    // matched with `[^{]*?` (any character but `{`, lazy) so it may span
    // multiple lines, and it stops before the opening brace of the body.
    std::regex class_regex(
        R"(\b(class|struct|record)\s+(struct\s+)?(\w+)(\s*<[^{};:]*>)?(?:\s*:\s*([^{]*?))?\s*\{)");
    std::smatch matches;
    std::string::const_iterator search_start(content.cbegin());

    std::vector<std::unique_ptr<Class>> classes;

    while (std::regex_search(search_start, content.cend(), matches, class_regex)) {
        // `matches.position(0)` is relative to the advancing `search_start`,
        // so anchor it absolutely: the 2nd+ type in a file must resolve its
        // namespace and body against the start of the string, not against the
        // previous type's end.
        const size_t class_pos =
            std::distance(content.cbegin(), search_start) + matches.position(0);

        std::string kind = matches[1];
        std::string class_name = matches[3];
        std::string base_classes = matches[5];

        // Attribute the type to the innermost `namespace X.Y` enclosing it
        std::string namespace_path = namespace_at(content, class_pos);

        // Create the class object
        auto type = std::make_unique<Class>(class_name, namespace_path);
        type->kind = kind;
        type->file = file_path;

        // Parse inheritance if exists. Split on top-level commas so a generic
        // base (`Base<A, B>`) and a dotted name (`MyNs.MyBase`) each stay a
        // single base, then convert dotted names to the model's `::` form so
        // the viewer can link the real base class.
        if (!base_classes.empty()) {
            for (const std::string& raw : split_bases(base_classes)) {
                std::string base = raw;
                for (char& c : base) {
                    if (c == '.') c = ':';
                }
                std::string converted;
                for (size_t i = 0; i < base.size(); ++i) {
                    converted += base[i];
                    if (base[i] == ':') converted += ':';
                }
                type->add_inheritance(converted);
            }
        }

        // Parse methods and fields within the type
        parse_class_content(content, class_pos, type.get());

        classes.push_back(std::move(type));
        search_start = matches.suffix().first;
    }

    return classes;
}

/**
 * @brief Parse the content of a class to extract methods and variables
 * @param content Full file content
 * @param start_pos Position where the class starts
 * @param class_obj Class object to populate with extracted data
 */
void CSharpParser::parse_class_content(const std::string& content, size_t start_pos, Class* class_obj) {
    if (!class_obj) return;

    // Find the end of the class definition
    size_t brace_count = 0;
    size_t class_end_pos = start_pos;
    bool in_class = false;

    for (size_t i = start_pos; i < content.length(); ++i) {
        if (content[i] == '{') {
            if (!in_class) in_class = true;
            brace_count++;
        } else if (content[i] == '}') {
            brace_count--;
            if (brace_count == 0 && in_class) {
                class_end_pos = i;
                break;
            }
        }
    }

    // Extract content within the class
    std::string class_content = content.substr(start_pos, class_end_pos - start_pos + 1);

    // Parse methods (both regular and constructor)
    parse_methods(class_content, class_obj);

    // Parse fields/variables
    parse_fields(class_content, class_obj);
}

/**
 * @brief Parse methods from class content
 * @param content Content of the class
 * @param class_obj Class object to populate with methods
 */
void CSharpParser::parse_methods(const std::string& content, Class* class_obj) {
    if (!class_obj) return;

    // Match method signatures: [access] [static] [virtual] [override] [return_type] method_name(params)
    // This regex handles various C# method patterns including constructors
    std::regex method_regex(R"((?:public|private|protected|internal)\s+(?:static\s+)?(?:virtual\s+)?(?:override\s+)?(?:\w+)\s+(\w+)\s*\(([^)]*)\))");
    std::smatch matches;
    std::string::const_iterator search_start(content.cbegin());

    while (std::regex_search(search_start, content.cend(), matches, method_regex)) {
        std::string method_name = matches[1];
        std::string parameters = matches[2];

        // Create method object - use a generic return type for now
        auto method = std::make_unique<Method>(method_name, "");
        method->return_type = "void";  // Default to void

        // Parse parameters (simplified)
        if (!parameters.empty()) {
            // Match parameter types and names
            std::regex param_regex(R"(\w+\s+(\w+))");
            std::smatch param_matches;
            std::string::const_iterator param_start(parameters.cbegin());

            while (std::regex_search(param_start, parameters.cend(), param_matches, param_regex)) {
                method->parameters.push_back(param_matches[1]);
                param_start = param_matches.suffix().first;
            }
        }

        // A concrete method is followed by a `{` body; brace-match it and
        // record the callee set for the call graph. Abstract (`;`) and
        // expression-bodied (`=>`) members have no brace body to scan.
        // `matches.position(0)` is relative to the advancing `search_start`
        // (not to the start of the string), so anchor it to an absolute
        // offset before locating the body brace.
        const size_t base = std::distance(content.cbegin(), search_start);
        size_t cursor = base + matches.position(0) + matches.length(0);
        while (cursor < content.size() &&
               std::isspace(static_cast<unsigned char>(content[cursor]))) {
            ++cursor;
        }
        if (cursor < content.size() && content[cursor] == '{') {
            const size_t end_pos = find_matching_brace(content, cursor);
            if (end_pos != std::string::npos) {
                const std::string method_body =
                    content.substr(cursor + 1, end_pos - cursor - 1);
                method->called_methods = CallScanner::extract(
                    method_body, csharp_call_blacklist());
            }
        }

        class_obj->add_method(std::move(method));
        search_start = matches.suffix().first;
    }
}

/**
 * @brief Parse fields/variables from class content
 * @param content Content of the class
 * @param class_obj Class object to populate with variables
 */
void CSharpParser::parse_fields(const std::string& content, Class* class_obj) {
    if (!class_obj) return;

    // Match field declarations: [access] [static] [type] variable_name;
    std::regex field_regex(R"((?:public|private|protected|internal)\s+(?:static\s+)?(\w+)\s+(\w+)\s*;)");
    std::smatch matches;
    std::string::const_iterator search_start(content.cbegin());

    while (std::regex_search(search_start, content.cend(), matches, field_regex)) {
        std::string type = matches[1];
        std::string variable_name = matches[2];

        // Create variable object
        auto variable = std::make_unique<Variable>(variable_name, "", type, Mutability::READ_WRITE);
        class_obj->add_variable(std::move(variable));
        search_start = matches.suffix().first;
    }
}

/**
 * @brief Parse C# project files to analyze multiple source files
 * @param project_path Path to the C# project directory
 * @return AnalysisResult containing parsed information
 */
AnalysisResult CSharpParser::parse_project(const std::string& project_path) {
    AnalysisResult result;

    // In a real implementation, this would:
    // 1. Look for .csproj files or other project configuration
    // 2. Identify source files in the project
    // 3. Parse each source file using parse_file()
    // 4. Return an AnalysisResult with parsed data

    std::cout << "Parsing C# project: " << project_path << std::endl;

    // For now, we'll just walk through the directory and parse .cs files
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(project_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".cs") {
                for (auto& cls : parse_file(entry.path().string())) {
                    result.add_class(std::move(cls));
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing project: " << e.what() << std::endl;
    }

    return result;
}