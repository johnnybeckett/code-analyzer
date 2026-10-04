#include "server/http_util.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace server {

namespace {

/** @brief The value of a hex digit, or -1 if not one. */
int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

std::string percent_decode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        if (c == '%' && i + 2 < in.size()) {
            const int hi = hex_val(in[i + 1]);
            const int lo = hex_val(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(c);
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> parse_query(const std::string& query) {
    std::vector<std::pair<std::string, std::string>> out;
    std::size_t start = 0;
    while (start < query.size()) {
        const std::size_t amp = query.find('&', start);
        const std::size_t end = (amp == std::string::npos) ? query.size() : amp;

        const std::size_t eq = query.find('=', start);
        if (eq == std::string::npos || eq > end) {
            // No '=' in this segment: a bare key with an empty value.
            out.emplace_back(query.substr(start, end - start), std::string{});
        } else {
            out.emplace_back(query.substr(start, eq - start),
                             query.substr(eq + 1, end - (eq + 1)));
        }

        if (amp == std::string::npos) {
            break;
        }
        start = amp + 1;
    }
    return out;
}

std::string first_query(const std::string& query, const std::string& name) {
    for (const auto& kv : parse_query(query)) {
        if (kv.first == name) {
            return kv.second;
        }
    }
    return {};
}

bool blank(const std::string& s) {
    for (char c : s) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\v' && c != '\f') {
            return false;
        }
    }
    return true;
}

std::string utc_now() {
    const std::time_t now = std::time(nullptr);
    const std::tm* gm = std::gmtime(&now);
    if (gm == nullptr) {
        return {};
    }
    char buf[32] = {0};
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gm) == 0) {
        return {};
    }
    return buf;
}

void split_lines(const std::string& s, std::vector<std::string>& out) {
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = s.find('\n', start);
        std::string line = (nl == std::string::npos) ? s.substr(start)
                                                      : s.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        out.push_back(std::move(line));
        if (nl == std::string::npos) {
            break;
        }
        start = nl + 1;
    }
}

std::string line_annotation(const ReviewComment& c) {
    if (c.new_line > 0 && c.old_line > 0) {
        return "(old L" + std::to_string(c.old_line) + ")";
    }
    if (c.new_line > 0) {
        return "(added line)";
    }
    return "(removed line)";  // old_line > 0, new_line == 0
}

std::string comment_md(const ReviewComment& c, const std::vector<std::string>& lines) {
    std::ostringstream os;
    const long long primary = c.new_line > 0 ? c.new_line : c.old_line;
    os << "**L" << primary << "** " << line_annotation(c) << " — ";

    if (primary >= 1 && static_cast<std::size_t>(primary) <= lines.size()) {
        const std::string& quoted = lines[primary - 1];
        if (quoted.find('`') == std::string::npos) {
            os << "`" << quoted << "`";
        } else {
            // A backtick in the source would break an inline-code span, so fall
            // back to an indented (4-space) code line.
            os << "\n    " << quoted;
        }
    } else {
        os << "_source not available_";
    }

    os << "\n";
    // The comment text, each line as a blockquote, then a blank "> " line and
    // the creation stamp.
    {
        std::vector<std::string> text_lines;
        split_lines(c.text, text_lines);
        for (const auto& tl : text_lines) {
            os << "> " << tl << "\n";
        }
    }
    os << ">\n> _created " << c.created << "_\n";
    return os.str();
}

std::string build_review_markdown(const std::vector<ReviewComment>& comments,
                                  const std::map<std::string, std::string>& sources,
                                  const std::string& title) {
    std::ostringstream os;
    os << "# Code Review";
    if (!title.empty()) {
        os << " — " << title;
    }
    os << "\n\n";
    os << "_Generated " << utc_now() << " · " << comments.size() << " comment(s)._\n\n";

    if (comments.empty()) {
        os << "No review comments.\n";
        return os.str();
    }

    // First-seen file order.
    std::vector<std::string> file_order;
    for (const auto& c : comments) {
        if (std::find(file_order.begin(), file_order.end(), c.file) == file_order.end()) {
            file_order.push_back(c.file);
        }
    }

    // Pre-split the source of each file we quote (only those that appear).
    std::map<std::string, std::vector<std::string>> line_cache;
    auto lines_for = [&](const std::string& file) -> const std::vector<std::string>& {
        auto it = line_cache.find(file);
        if (it != line_cache.end()) {
            return it->second;
        }
        std::vector<std::string> lines;
        if (const auto s = sources.find(file); s != sources.end()) {
            split_lines(s->second, lines);
        }
        return line_cache.emplace(file, std::move(lines)).first->second;
    };

    for (const auto& file : file_order) {
        std::vector<const ReviewComment*> rows;
        for (const auto& c : comments) {
            if (c.file == file) {
                rows.push_back(&c);
            }
        }
        std::sort(rows.begin(), rows.end(), [](const ReviewComment* a, const ReviewComment* b) {
            const long long pa = a->new_line > 0 ? a->new_line : a->old_line;
            const long long pb = b->new_line > 0 ? b->new_line : b->old_line;
            return pa < pb;
        });

        os << "## " << file << "\n\n";
        const std::vector<std::string>& lines = lines_for(file);
        for (const auto* c : rows) {
            os << comment_md(*c, lines) << "\n";
        }
    }
    return os.str();
}

bool ends_with_ci(const std::string& path, const char* suffix) {
    const std::size_t n = std::strlen(suffix);
    if (path.size() < n) {
        return false;
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (std::tolower(static_cast<unsigned char>(path[path.size() - n + i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }
    return true;
}

bool renderer_available(const std::string& bin) {
    if (std::FILE* p = popen(("command -v " + bin + " >/dev/null 2>&1").c_str(), "r")) {
        return pclose(p) == 0;
    }
    return false;
}

std::optional<std::string> run_renderer(const std::string& cmd, const std::string& source) {
    char tmpl[] = "/tmp/uml-render-XXXXXX";
    const int fd = mkstemps(tmpl, 0);
    if (fd < 0) {
        return std::nullopt;
    }
    const std::string tmp = tmpl;
    if (source.size() != 0 &&
        (write(fd, source.data(), source.size()) != static_cast<ssize_t>(source.size()) ||
         fsync(fd) != 0)) {
        ::close(fd);
        std::remove(tmp.c_str());
        return std::nullopt;
    }
    ::close(fd);

    std::optional<std::string> svg;
    if (std::FILE* p = popen((cmd + " '" + tmp + "'").c_str(), "r")) {
        std::string out;
        char buf[8192];
        std::size_t got = 0;
        while ((got = std::fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, got);
        }
        const int st = pclose(p);
        if (st == 0 && !out.empty()) {
            svg = std::move(out);
        }
    }
    std::remove(tmp.c_str());
    return svg;
}

}  // namespace server
