#include "parser/call_scanner.h"

#include <cctype>

namespace {

bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

} // namespace

std::vector<std::string> CallScanner::extract(
    const std::string& body,
    const std::unordered_set<std::string>& blacklist) {
    std::vector<std::string> calls;
    std::unordered_set<std::string> seen;

    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i] != '(') continue;

        // Step back over the whitespace between the callee and the '('.
        size_t j = i;
        while (j > 0 && std::isspace(static_cast<unsigned char>(body[j - 1]))) {
            --j;
        }

        // Read the identifier (if any) ending just before that whitespace.
        const size_t end = j;
        while (j > 0 && is_ident_char(body[j - 1])) {
            --j;
        }
        if (end == j) continue;  // no identifier before '(' -> not a call

        const std::string callee = body.substr(j, end - j);
        if (!is_ident_start(callee.front())) continue;  // not a valid name
        if (blacklist.count(callee)) continue;
        if (seen.count(callee)) continue;                // dedup, keep first-seen order
        seen.insert(callee);
        calls.push_back(std::move(callee));
    }

    return calls;
}
