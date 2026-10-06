#include "parser/python_parser.h"
#include "parser/call_scanner.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace {

/**
 * @brief Count leading whitespace (spaces and tabs) on a line
 */
size_t leading_ws(const std::string& line) {
    size_t n = 0;
    while (n < line.size() && (line[n] == ' ' || line[n] == '\t')) ++n;
    return n;
}

/**
 * @brief Trim leading/trailing spaces and tabs
 */
std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

/**
 * @brief Python keywords that can appear as `<keyword>(` but are not calls
 *
 * Stripped by the shared CallScanner so only genuine callee names remain
 * (`if (`, `for (`, `with (`, `assert (`, `lambda`, ...). Real builtins
 * that are calls (`print`, `len`, ...) are deliberately kept.
 */
const std::unordered_set<std::string>& python_call_blacklist() {
    static const std::unordered_set<std::string> blacklist = {
        "if", "for", "while", "with", "assert", "lambda", "del",
        "not", "is", "in", "and", "or", "def", "class",
        "return", "yield", "raise"
    };
    return blacklist;
}

} // namespace

/**
 * @brief Parse a Python file and extract class information
 * @param file_path Path to the Python source file
 * @return All classes found in the file (empty on error or no classes)
 */
std::vector<std::unique_ptr<Class>> PythonParser::parse_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << file_path << std::endl;
        return {};
    }

    // Read the entire file content
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Find every class declaration in the file (a module may declare several).
    std::regex class_regex(R"(class\s+(\w+)(?:\s*\(([^)]*)\))?\s*:)");

    std::vector<std::unique_ptr<Class>> classes;
    for (auto it = std::sregex_iterator(content.cbegin(), content.cend(), class_regex);
         it != std::sregex_iterator(); ++it) {
        const std::smatch& matches = *it;
        std::string class_name = matches[1].str();

        // Create the class with proper namespace handling
        auto parsed_class = std::make_unique<Class>(class_name, "");
        parsed_class->kind = "class";  // Python types are always classes
        parsed_class->file = file_path;

        // Extract inheritance information if present. Each comma-separated
        // token is trimmed, and keyword arguments such as
        // `metaclass=ABCMeta` are skipped — only plain base class names are
        // recorded as inheritance.
        if (matches.size() > 2 && !matches[2].str().empty()) {
            std::string inheritance_list = matches[2].str();
            size_t start = 0;
            while (true) {
                size_t pos = inheritance_list.find(',', start);
                std::string token = (pos == std::string::npos)
                    ? inheritance_list.substr(start)
                    : inheritance_list.substr(start, pos - start);

                size_t b = token.find_first_not_of(" \t");
                size_t e = token.find_last_not_of(" \t");
                token = (b == std::string::npos) ? "" : token.substr(b, e - b + 1);

                if (!token.empty() && token.find('=') == std::string::npos) {
                    parsed_class->add_inheritance(token);
                }
                if (pos == std::string::npos) break;
                start = pos + 1;
            }
        }

        // Extract the class's methods (and their callee sets) from the
        // indented body that follows the `class X:` declaration
        parse_class_content(content, matches.position(0), parsed_class.get());

        std::cout << "Found Python class: " << class_name << " in file: " << file_path << std::endl;
        classes.push_back(std::move(parsed_class));
    }

    return classes;
}

/**
 * @brief Parse a Python project directory
 * @param project_path Path to the Python project directory
 * @return AnalysisResult containing parsed information
 */
AnalysisResult PythonParser::parse_project(const std::string& project_path) {
    AnalysisResult result;

    // In a real implementation, this would identify Python source files in the project
    // and parse each one using parse_file()
    std::cout << "Parsing Python project: " << project_path << std::endl;

    return result;
}

/**
 * @brief Locate a class body by indentation and extract its members
 *
 * Python has no braces: the body is the block of lines indented deeper than
 * the `class X:` declaration line, ending at the first non-blank line whose
 * indent is strictly shallower than the body's own indent. A line *at* the
 * body indent (a sibling method / field) stays inside the body.
 * @param content Full file content
 * @param start_pos Position where the class declaration starts
 * @param class_obj Class object to populate with extracted data
 */
void PythonParser::parse_class_content(const std::string& content, size_t start_pos, Class* class_obj) {
    if (!class_obj) return;

    // The body starts on the line after the `class X:` declaration
    size_t body_start = content.find('\n', start_pos);
    if (body_start == std::string::npos) return;
    ++body_start;

    // Discover the body's indent from its first non-blank line, then find
    // where the body ends (the first non-blank line at or above that indent)
    size_t body_indent = std::string::npos;
    size_t end = body_start;
    while (end < content.size()) {
        size_t eol = content.find('\n', end);
        const size_t len = (eol == std::string::npos) ? content.size() - end : eol - end;
        const std::string line = content.substr(end, len);
        const size_t next = (eol == std::string::npos) ? content.size() : eol + 1;

        const size_t indent = leading_ws(line);
        if (indent < line.size()) {
            if (body_indent == std::string::npos) body_indent = indent;
            else if (indent < body_indent) break;  // strictly shallower = left the class
        }
        end = next;
    }

    if (body_indent == std::string::npos) return;  // no body to parse

    const std::string body = content.substr(body_start, end - body_start);
    parse_methods(body, class_obj);
    parse_fields(body, class_obj);
}

/**
 * @brief Extract member methods (and their bodies) from a class body
 *
 * A member method is a `def` at the body's own indent level; deeper `def`s
 * are inner functions, not class methods. A method's body is the following
 * block of lines indented deeper than the `def` line; it is fed to the
 * shared CallScanner so `Method::called_methods` carries the callee set for
 * the call graph.
 * @param content Content of the class body (indentation intact)
 * @param class_obj Class object to populate with methods
 */
void PythonParser::parse_methods(const std::string& content, Class* class_obj) {
    if (!class_obj) return;

    const std::regex def_re(R"(^\s*(?:async\s+)?def\s+(\w+)\s*\(([^)]*)\)\s*:)");
    size_t body_indent = std::string::npos;
    size_t pos = 0;

    while (pos < content.size()) {
        size_t eol = content.find('\n', pos);
        const size_t len = (eol == std::string::npos) ? content.size() - pos : eol - pos;
        const std::string line = content.substr(pos, len);
        const size_t next = (eol == std::string::npos) ? content.size() : eol + 1;

        const size_t indent = leading_ws(line);
        if (indent == line.size() || line[indent] == '#') { pos = next; continue; }

        if (body_indent == std::string::npos) body_indent = indent;
        if (indent != body_indent) { pos = next; continue; }  // a nested scope line

        std::smatch m;
        if (!std::regex_search(line, m, def_re)) { pos = next; continue; }

        const std::string name = m[1].str();
        const std::string params = m[2].str();
        auto method = std::make_unique<Method>(name, class_obj->name);

        // Parameters: split on commas and take the last token of each piece,
        // dropping any `= default` and skipping the `self`/`cls` receiver
        size_t pstart = 0;
        while (true) {
            const size_t comma = params.find(',', pstart);
            std::string piece = trim((comma == std::string::npos)
                ? params.substr(pstart)
                : params.substr(pstart, comma - pstart));
            const size_t eq = piece.find('=');
            if (eq != std::string::npos) piece = trim(piece.substr(0, eq));
            if (!piece.empty()) {
                const size_t t = piece.find_last_of(" \t");
                const std::string pname = (t == std::string::npos) ? piece : piece.substr(t + 1);
                if (!pname.empty() && pname != "self" && pname != "cls") {
                    method->parameters.push_back(std::move(pname));
                }
            }
            if (comma == std::string::npos) break;
            pstart = comma + 1;
        }

        // The method body is the following lines indented deeper than the `def`
        std::string method_body;
        size_t b = next;
        while (b < content.size()) {
            size_t beol = content.find('\n', b);
            const size_t blen = (beol == std::string::npos) ? content.size() - b : beol - b;
            const std::string bl = content.substr(b, blen);
            const size_t bnext = (beol == std::string::npos) ? content.size() : beol + 1;

            const size_t bindent = leading_ws(bl);
            if (bindent < bl.size() && bindent <= indent) break;  // left the method
            method_body += bl;
            method_body += '\n';
            b = bnext;
        }
        method->called_methods = CallScanner::extract(method_body, python_call_blacklist());

        class_obj->add_method(std::move(method));
        pos = b;
    }
}

/**
 * @brief Parse fields/variables from class content
 *
 * Python field declarations carry no type information without annotations and
 * are not modelled; left as a no-op (the minimal-implementation baseline).
 * @param content Content of the class body
 * @param class_obj Class object to populate with variables
 */
void PythonParser::parse_fields(const std::string& content, Class* class_obj) {
    (void)content;
    (void)class_obj;
}
