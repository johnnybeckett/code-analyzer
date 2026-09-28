#include "cpp_parser.h"
#include "../core/model.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <sstream>
#include <cctype>
#include <filesystem>
#include <set>
#include <json/json.h>

namespace {

/**
 * @brief C++ keywords and syntax tokens that must not be mistaken
 *        for method or member variable names
 */
const std::set<std::string>& syntax_keywords() {
    static const std::set<std::string> keywords = {
        "if", "for", "while", "switch", "case", "break", "continue",
        "return", "else", "do", "goto", "sizeof", "new", "delete",
        "throw", "try", "catch", "using", "namespace", "typedef",
        "template", "class", "struct", "enum", "union", "static_assert",
        "assert", "operator", "public", "private", "protected",
        "const", "constexpr", "static", "virtual", "inline", "explicit",
        "friend", "override", "noexcept", "decltype", "auto", "nullptr",
        "true", "false", "and", "or", "not"
    };
    return keywords;
}

/**
 * @brief Trim leading/trailing whitespace
 */
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

/**
 * @brief Remove // line comments and /* *​/ block comments from source text
 */
std::string strip_comments(const std::string& content) {
    std::string result = std::regex_replace(content, std::regex(R"(//[^\n]*)"), "");
    result = std::regex_replace(result, std::regex(R"(/\*[\s\S]*?\*/)"), "");
    return result;
}

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
 * @brief Split a parameter list (or base class list) on top-level commas
 */
std::vector<std::string> split_parameters(const std::string& params) {
    std::vector<std::string> out;
    int angle = 0, paren = 0;
    std::string current;
    for (char c : params) {
        if (c == '<') ++angle;
        else if (c == '>') angle = angle > 0 ? angle - 1 : 0;
        else if (c == '(') ++paren;
        else if (c == ')') paren = paren > 0 ? paren - 1 : 0;
        if (c == ',' && angle == 0 && paren == 0) {
            std::string t = trim(current);
            if (!t.empty()) out.push_back(t);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    std::string t = trim(current);
    if (!t.empty()) out.push_back(t);
    return out;
}

/**
 * @brief Heuristically extract methods and member variables from a class body
 *
 * Only statements at the top level of the body are considered: brace and
 * paren depth are tracked across lines, so local variables inside inline
 * method definitions and continuation lines of multi-line declarations are
 * not mistaken for class members.
 */
void extract_members(Class& cls, const std::string& body) {
    // Default member access inside a class is private
    Visibility current = Visibility::PRIVATE;

    const std::regex visibility_re(R"(\b(public|private|protected)\s*:)");

    // First words that introduce nested types or aliases rather than the
    // plain members we can model
    static const std::set<std::string> nested_declarations = {
        "enum", "struct", "union", "using", "typedef",
        "namespace", "template", "friend", "static_assert"
    };

    auto first_word = [](const std::string& s) {
        std::istringstream ws(s);
        std::string w;
        ws >> w;
        return w;
    };

    int brace_depth = 0;  // relative to the class body
    int paren_depth = 0;  // > 0 means the line continues a multi-line statement

    std::istringstream stream(body);
    std::string line;
    while (std::getline(stream, line)) {
        const int line_brace_depth = brace_depth;
        const int line_paren_depth = paren_depth;

        // Update the running depth for the next line
        for (char c : line) {
            if (c == '{') ++brace_depth;
            else if (c == '}') --brace_depth;
            else if (c == '(') ++paren_depth;
            else if (c == ')') --paren_depth;
        }

        line = trim(line);
        if (line.empty() || line[0] == '#' ||
            line_brace_depth != 0 || line_paren_depth != 0) {
            continue;
        }

        std::smatch m;
        if (std::regex_search(line, m, visibility_re)) {
            std::string vis = m[1];
            if (vis == "public") current = Visibility::PUBLIC;
            else if (vis == "protected") current = Visibility::PROTECTED;
            else current = Visibility::PRIVATE;
            continue;
        }

        // Constructor initialiser lists (`: member(arg), ...`) and
        // nested-type / alias declarations are not members we model
        if (line[0] == ':' || nested_declarations.count(first_word(line))) continue;

        const bool ends_with_semi = line.back() == ';';
        const bool ends_with_open_brace = line.back() == '{';

        // --- Method / function declaration:  <type> name(params) ...; or {
        if ((ends_with_semi || ends_with_open_brace) &&
            line.find('(') != std::string::npos) {
            // Locate the parameter list: last ')' and its matching '('
            size_t close = line.find_last_of(')');
            size_t open = std::string::npos;
            int bal = 0;
            for (size_t i = close; i != std::string::npos; --i) {
                if (line[i] == ')') ++bal;
                else if (line[i] == '(' && --bal == 0) { open = i; break; }
            }

            if (open != std::string::npos) {
                // The identifier immediately before '(' is the function name
                size_t name_end = open;
                while (name_end > 0 && (line[name_end - 1] == ' ' || line[name_end - 1] == '\t')) --name_end;
                size_t name_start = name_end;
                while (name_start > 0 &&
                       (std::isalnum(static_cast<unsigned char>(line[name_start - 1])) ||
                        line[name_start - 1] == '_')) {
                    --name_start;
                }
                std::string name = line.substr(name_start, name_end - name_start);
                std::string type = trim(line.substr(0, name_start));

                if (!name.empty() && name.back() != '~'          // not a destructor
                    && !syntax_keywords().count(name)            // not `if (...)`, ...
                    && !type.empty()                              // not a constructor
                    && type.find('~') == std::string::npos       // not `virtual ~X()`
                    && type.find('=') == std::string::npos) {    // not an initialiser
                    auto method = std::make_unique<Method>(name, cls.name, current);
                    method->return_type = type;
                    method->is_static = line.find("static") != std::string::npos;
                    method->parameters = split_parameters(line.substr(open + 1, close - open - 1));
                    cls.add_method(std::move(method));
                }
                continue;
            }
        }

        // --- Member variable declaration:  <type> name [= init];
        if (ends_with_semi && line.find('(') == std::string::npos) {
            std::string decl = line;
            if (decl.back() == ';') decl.pop_back();

            // Drop a top-level `= <initializer>` so the name is not confused
            // with the initialiser's last token (e.g. `bool x = false;`)
            int angle = 0;
            for (size_t i = 0; i + 1 < decl.size(); ++i) {
                if (decl[i] == '<') ++angle;
                else if (decl[i] == '>') angle = angle > 0 ? angle - 1 : 0;
                else if (decl[i] == '=' && angle == 0 &&
                         (i == 0 || std::isspace(static_cast<unsigned char>(decl[i - 1])))) {
                    decl = decl.substr(0, i);
                    break;
                }
            }

            // The last token is the member name, everything before it the type
            decl = trim(decl);
            size_t name_end = decl.find_last_not_of(" \t") + 1;
            size_t name_start = name_end;
            while (name_start > 0 &&
                   (std::isalnum(static_cast<unsigned char>(decl[name_start - 1])) ||
                    decl[name_start - 1] == '_')) {
                --name_start;
            }
            std::string name = decl.substr(name_start, name_end - name_start);
            std::string type = trim(decl.substr(0, name_start));

            if (!name.empty() && !syntax_keywords().count(name) &&
                !nested_declarations.count(type)) {
                auto variable = std::make_unique<Variable>(
                    name, cls.name, type,
                    line.find("const") != std::string::npos ? Mutability::CONST : Mutability::READ_WRITE,
                    current);
                cls.add_variable(std::move(variable));
            }
        }
    }
}

} // namespace

std::vector<std::unique_ptr<Class>> CppParser::parse_file(const std::string& file_path) {
    std::vector<std::unique_ptr<Class>> classes;

    // Check if the file exists
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "File not found: " << file_path << std::endl;
        return classes;
    }

    // Read the file contents
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Unable to open file: " << file_path << std::endl;
        return classes;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    content = strip_comments(content);

    // Match every `class Name [: base, base, ...] {` declaration in the file
    const std::regex class_regex(R"(class\s+(\w+)(?:\s*:\s*([\w:\s,]+?))?\s*\{)");

    for (auto it = std::sregex_iterator(content.cbegin(), content.cend(), class_regex);
         it != std::sregex_iterator(); ++it) {
        const std::smatch& match = *it;
        std::string class_name = match[1];

        auto parsed_class = std::make_unique<Class>(class_name, "");

        // Extract base classes (last identifier of each comma-separated item)
        if (match[2].matched) {
            for (const auto& base : split_parameters(match[2])) {
                std::istringstream bs(base);
                std::string tok, last;
                while (bs >> tok) last = tok;
                if (!last.empty() && !syntax_keywords().count(last)) {
                    parsed_class->add_inheritance(last);
                }
            }
        }

        // Locate the class body via brace matching and extract its members
        size_t open_pos = match.position(0) + match.length(0) - 1; // index of '{'
        size_t close_pos = find_matching_brace(content, open_pos);
        if (close_pos != std::string::npos) {
            std::string body = content.substr(open_pos + 1, close_pos - open_pos - 1);
            extract_members(*parsed_class, body);
        }

        std::cout << "Found C++ class: " << class_name << std::endl;
        classes.push_back(std::move(parsed_class));
    }

    return classes;
}

AnalysisResult CppParser::parse_compile_commands(const std::string& compile_commands_path) {
    AnalysisResult result;

    std::ifstream file(compile_commands_path);
    if (!file.is_open()) {
        std::cerr << "Unable to open compile_commands file: " << compile_commands_path << std::endl;
        return result;
    }

    Json::Value root;
    Json::CharReaderBuilder reader_builder;
    std::string errors;
    if (!Json::parseFromStream(reader_builder, file, &root, &errors)) {
        std::cerr << "Error parsing compile_commands.json: " << errors << std::endl;
        return result;
    }

    // Parse each source file listed in the compilation database
    for (const auto& entry : root) {
        std::string file_path = entry.get("file", "").asString();
        if (file_path.empty()) continue;

        std::cout << "Processing compile command for: " << file_path << std::endl;
        for (auto& parsed_class : parse_file(file_path)) {
            result.add_class(std::move(parsed_class));
        }
    }

    return result;
}
