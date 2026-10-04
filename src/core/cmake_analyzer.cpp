#include "core/cmake_analyzer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string_view>

namespace {

/**
 * @brief A CMake command: its name plus its (already tokenised) arguments,
 * and the directory of the CMakeLists.txt / .cmake file it came from.
 * `dir` is empty for parse_text() input — there is no directory to qualify
 * sources against, so they stay exactly as written.
 */
struct Command {
    std::string name;
    std::vector<std::string> args;
    std::string dir;
};

/** @brief Lowercase a string (ASCII; CMake identifiers and keywords are ASCII). */
std::string lower(std::string_view s) {
    std::string out;
    out.resize(s.size());
    std::transform(s.begin(), s.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

/**
 * @brief Keyword tags we must not mistake for source files or dependency names.
 * Matched case-insensitively, on the whole token (so "object.cpp" is a file,
 * not the "object" keyword).
 */
bool is_keyword(std::string_view token) {
    static const std::set<std::string> keywords = {
        "before", "private", "public", "interface", "static", "shared",
        "module", "object", "alias", "imported", "global", "win32",
        "macosx_bundle", "exclude_from_all", "optional"};
    return keywords.count(lower(token)) != 0;
}

/**
 * @brief True if a target_link_libraries item names a target we can keep as a
 * dependency (vs. a -l flag, a library path, or a generator expression /
 * variable that names no target).
 */
bool is_target_name(std::string_view item) {
    if (item.empty()) {
        return false;
    }
    if (item.front() == '-') {
        return false;  // -lfoo, -L/path, --flag
    }
    if (item.front() == '$') {
        return false;  // $<...> generator expression or ${...} variable
    }
    if (item.find('/') != std::string_view::npos) {
        return false;  // a path to a prebuilt library, not a target in our graph
    }
    return true;
}

/**
 * @brief Tokenise CMake text into tokens, with '(' and ')' as their own tokens.
 *
 * Handles comments (# to end of line), quoted strings (one token each, with
 * backslash unescaping), and leaves $<...> / ${...} as single tokens. Newlines
 * are ordinary whitespace, so commands spanning multiple lines tokenize right.
 */
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string cur;
    auto flush = [&]() {
        if (!cur.empty()) {
            tokens.push_back(std::move(cur));
            cur.clear();
        }
    };
    // A while-loop (not for/++i): several branches below advance `i` to the
    // *next* character to examine (the plain-token run stops exactly at the
    // following paren/space). An auto-incrementing for would skip over that
    // boundary char — dropping every '(' / ')' token — so each branch here owns
    // advancing `i` itself.
    const std::size_t n = text.size();
    std::size_t i = 0;
    while (i < n) {
        const char c = text[i];
        if (c == '#') {  // comment: skip to end of line
            while (i < n && text[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            flush();
            ++i;
            continue;
        }
        if (c == '(' || c == ')') {
            flush();
            tokens.push_back(std::string(1, c));
            ++i;
            continue;
        }
        if (c == '"') {  // quoted string: one token, unescape backslash pairs
            ++i;
            while (i < n && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < n) {
                    cur += text[i + 1];
                    i += 2;
                } else {
                    cur += text[i];
                    ++i;
                }
            }
            if (i < n) {
                ++i;  // skip the closing quote
            }
            flush();
            continue;
        }
        // Plain token (covers $<...> and ${...}): run to whitespace or paren.
        while (i < n && !std::isspace(static_cast<unsigned char>(text[i])) &&
               text[i] != '(' && text[i] != ')') {
            cur += text[i++];
        }
        flush();
    }
    flush();
    return tokens;
}

/** @brief Group each `name ( ... )` in the token stream into a Command. */
std::vector<Command> extract_commands(const std::string& text) {
    const std::vector<std::string> tokens = tokenize(text);
    std::vector<Command> commands;
    const std::size_t n = tokens.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (tokens[i] == "(" || tokens[i] == ")") {
            continue;
        }
        if (i + 1 < n && tokens[i + 1] == "(") {
            Command cmd;
            cmd.name = lower(tokens[i]);
            i += 2;  // consume the name and the opening '('
            while (i < n && tokens[i] != ")") {
                if (tokens[i] != "(") {
                    cmd.args.push_back(tokens[i]);
                }
                ++i;
            }
            commands.push_back(std::move(cmd));  // i now at ')'; loop's ++i skips it
        }
        // else: a stray token with no following '(' — not a command, ignore
    }
    return commands;
}

/** @brief Read a whole file into a string; empty on any failure. */
std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::string();
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/** @brief Discover CMakeLists.txt / *.cmake files under a root directory. */
std::vector<std::string> discover_cmake_files(const std::string& root,
                                              const std::vector<std::string>& skip_dirs) {
    std::vector<std::string> files;
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec) || ec) {
        return files;
    }

    std::filesystem::recursive_directory_iterator it(root,
        std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        return files;
    }
    std::filesystem::recursive_directory_iterator end;

    for (; it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        std::error_code e2;
        const std::filesystem::directory_entry entry = *it;
        if (entry.is_directory(e2)) {
            const std::string name = entry.path().filename().string();
            if (std::find(skip_dirs.begin(), skip_dirs.end(), name) != skip_dirs.end()) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!entry.is_regular_file(e2)) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        const bool is_cmakelists = (name == "CMakeLists.txt");
        const bool is_cmake = name.size() > 6 && name.compare(name.size() - 6, 6, ".cmake") == 0;
        if (is_cmakelists || is_cmake) {
            files.push_back(entry.path().string());
        }
    }
    return files;
}

/**
 * @brief Qualify a source name against the directory it was declared in.
 *
 * CMake source entries are relative to the CMakeLists.txt that lists them, so
 * `core.cpp` in `src/net/CMakeLists.txt` lives at `src/net/core.cpp`. We join
 * the two only for plain relative names: absolute paths and `$`-prefixed
 * variables point at something else entirely, and an empty `dir` (parse_text
 * input) means "record it exactly as written" — the white-box test seam.
 */
std::string qualify_source(const std::string& dir, const std::string& a) {
    if (dir.empty() || a.empty() || a.find('/') != std::string::npos) {
        return a;
    }
    if (a.front() == '$' || a.front() == '-') {
        return a;
    }
    return dir + "/" + a;
}

/**
 * @brief Fold a batch of commands into a CMakeGraph (single pass, so a target's
 * definition and its target_sources / target_link_libraries merge even across
 * files). Order-preserving: first definition wins the position in the output.
 */
CMakeGraph parse_commands(const std::vector<Command>& commands) {
    std::vector<CMakeTarget> targets;
    std::map<std::string, std::size_t> index;  // name -> position in `targets`

    auto ensure = [&](const std::string& name) -> CMakeTarget& {
        auto it = index.find(name);
        if (it != index.end()) {
            return targets[it->second];
        }
        CMakeTarget t;
        t.name = name;
        t.kind = "library";  // default; a later command may refine it
        index[name] = targets.size();
        targets.push_back(std::move(t));
        return targets.back();
    };
    auto append_unique = [](std::vector<std::string>& v, const std::string& item) {
        if (std::find(v.begin(), v.end(), item) == v.end()) {
            v.push_back(item);
        }
    };

    for (const Command& cmd : commands) {
        if (cmd.name == "add_library" || cmd.name == "add_executable") {
            if (cmd.args.empty()) {
                continue;
            }
            const std::string name = cmd.args[0];

            // ALIAS form: add_library(<alias> ALIAS <real>) — a stand-in target.
            std::string alias_of;
            for (std::size_t i = 1; i + 1 < cmd.args.size(); ++i) {
                if (lower(cmd.args[i]) == "alias") {
                    alias_of = cmd.args[i + 1];
                    break;
                }
            }
            if (!alias_of.empty()) {
                CMakeTarget t;
                t.name = name;
                t.kind = "alias";
                t.alias_of = alias_of;
                auto it = index.find(name);
                if (it == index.end()) {
                    index[name] = targets.size();
                    targets.push_back(std::move(t));
                } else {
                    targets[it->second] = std::move(t);  // redefinition
                }
                continue;
            }

            CMakeTarget& t = ensure(name);
            if (cmd.name == "add_executable") {
                t.kind = "executable";
            } else {
                bool is_interface = false;
                for (const auto& a : cmd.args) {
                    if (lower(a) == "interface") {
                        is_interface = true;
                        break;
                    }
                }
                t.kind = is_interface ? "interface" : "library";
            }
            // Sources: every non-keyword argument after the name, qualified
            // to the declaring file's directory.
            for (std::size_t i = 1; i < cmd.args.size(); ++i) {
                const std::string& a = cmd.args[i];
                if (!is_keyword(a)) {
                    append_unique(t.sources, qualify_source(cmd.dir, a));
                }
            }
        } else if (cmd.name == "target_sources") {
            if (cmd.args.empty()) {
                continue;
            }
            CMakeTarget& t = ensure(cmd.args[0]);
            for (std::size_t i = 1; i < cmd.args.size(); ++i) {
                const std::string& a = cmd.args[i];
                if (!is_keyword(a)) {
                    append_unique(t.sources, qualify_source(cmd.dir, a));
                }
            }
        } else if (cmd.name == "target_link_libraries") {
            if (cmd.args.empty()) {
                continue;
            }
            CMakeTarget& t = ensure(cmd.args[0]);
            for (std::size_t i = 1; i < cmd.args.size(); ++i) {
                const std::string& a = cmd.args[i];
                if (!is_keyword(a) && is_target_name(a)) {
                    append_unique(t.links, a);
                }
            }
        }
    }

    CMakeGraph graph;
    graph.targets = std::move(targets);
    return graph;
}

}  // namespace

CMakeGraph CMakeAnalyzer::parse_directory(const std::string& root,
                                          const std::vector<std::string>& skip_dirs) const {
    std::vector<Command> all;
    for (const std::string& file : discover_cmake_files(root, skip_dirs)) {
        const std::string dir = std::filesystem::path(file).parent_path().string();
        std::vector<Command> commands = extract_commands(read_file(file));
        for (auto& cmd : commands) {
            cmd.dir = dir;  // so relative sources resolve against this file's directory
        }
        all.insert(all.end(),
                   std::make_move_iterator(commands.begin()),
                   std::make_move_iterator(commands.end()));
    }
    return parse_commands(all);
}

CMakeGraph CMakeAnalyzer::parse_text(const std::string& cmake_text) const {
    return parse_commands(extract_commands(cmake_text));
}
