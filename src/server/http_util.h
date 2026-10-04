#ifndef HTTP_UTIL_H
#define HTTP_UTIL_H

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "server/review_store.h"

namespace server {

/**
 * @brief Decode %-escapes (%XX) in a query value.
 *
 * Only hex escape sequences are decoded; everything else — including `+`, which
 * the client never emits because it uses encodeURIComponent — is passed through
 * unchanged. A malformed escape (no two hex digits) is left as-is rather than
 * dropped, so a lookup simply fails to 404 instead of mangling the key.
 */
std::string percent_decode(const std::string& in);

/**
 * @brief Parse a `&`-separated query string into (name, value) pairs, in order.
 *
 * Each `&`-segment is split on its *first* `=`: the part before is the key, the
 * rest is the value (a segment with no `=` is a key with an empty value). Empty
 * segments are skipped. The first occurrence of a key wins, so `a=1&a=2` yields
 * a single `("a","1")`. Values are NOT percent-decoded — the caller decodes.
 */
std::vector<std::pair<std::string, std::string>> parse_query(const std::string& query);

/**
 * @brief The first `name=` value in a `&`-separated query string, or "".
 *
 * The key is matched exactly as a `&`-delimited segment, so a longer param that
 * merely contains `name=` (e.g. `xpath=` for `path`) is not mistaken for a
 * match. The value is returned as-is (not percent-decoded).
 */
std::string first_query(const std::string& query, const std::string& name);

/** @brief True when `s` is empty or whitespace-only. */
bool blank(const std::string& s);

/** @brief A UTC `YYYY-MM-DDTHH:MM:SSZ` timestamp for the current instant. */
std::string utc_now();

/** @brief Split `s` on newlines (stripping a trailing CR), in order. */
void split_lines(const std::string& s, std::vector<std::string>& out);

/** @brief The `(old L<n>)` / `(added line)` / `(removed line)` annotation. */
std::string line_annotation(const ReviewComment& c);

/**
 * @brief Render one comment (line header + quoted source + the comment block)
 *        as Markdown. `lines` is the source file already split into lines (may
 *        be empty when the file isn't in the allowlist).
 */
std::string comment_md(const ReviewComment& c, const std::vector<std::string>& lines);

/**
 * @brief Build the full Markdown review document.
 *
 * Files appear in first-seen comment order; within a file, comments are ordered
 * by primary line (newLine if present, else oldLine). Quoted source lines come
 * from the `sources` allowlist (never the filesystem).
 */
std::string build_review_markdown(const std::vector<ReviewComment>& comments,
                                  const std::map<std::string, std::string>& sources,
                                  const std::string& title);

/**
 * @brief True when `path` ends with `suffix` (case-insensitively, so
 * `Graph.DOT` routes the same as `graph.dot`).
 */
bool ends_with_ci(const std::string& path, const char* suffix);

/**
 * @brief True if `bin` is resolvable on PATH (a clean 503 is better than an
 * empty body when the renderer is simply not installed on this host).
 */
bool renderer_available(const std::string& bin);

/**
 * @brief Run a renderer over `source` and capture its stdout as the SVG.
 *
 * popen("r") hands us a read-only pipe, so the renderer reads the source from
 * a file instead: mkstemps() creates an unguessable, O_EXCL temp file
 * (mkstemps only appends alphanumerics, so quoting it later is safe), we
 * write the source into it, exec `<cmd> '<tmp>'`, and delete it on every
 * path. `cmd` is always one of the fixed literals chosen by the render handler
 * — never built from the request. nullopt on write error, non-zero exit, or
 * empty output; the caller stages a 502 for that.
 */
std::optional<std::string> run_renderer(const std::string& cmd, const std::string& source);

}  // namespace server

#endif  // HTTP_UTIL_H
