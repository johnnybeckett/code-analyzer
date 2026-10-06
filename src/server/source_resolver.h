#ifndef SOURCE_RESOLVER_H
#define SOURCE_RESOLVER_H

#include <filesystem>
#include <optional>
#include <string>

namespace server {

/**
 * @brief Resolve a class's recorded source path to the readable file behind it.
 *
 * `recorded` is the path string exactly as the analyzer recorded it on the
 * class — absolute, or relative to whatever CWD the analyzer ran from. It is
 * tried as-is first (so an absolute path, or one already valid against the
 * server's CWD, is used unchanged). If that is not a readable regular file and
 * the path is relative, the same path is tried under `json_dir` (the directory
 * the input JSON lives in). The first readable candidate's contents are
 * returned.
 *
 * This is a load-time helper: it runs once while preloading the `/source`
 * allowlist, and is never handed attacker-controlled input — so resolving a
 * recorded path into a candidate file here does not weaken the server's
 * allowlist posture (the `/source` route still answers only by exact match on
 * the recorded key and never builds a path from the query).
 *
 * @param recorded The recorded source path (absolute or relative).
 * @param json_dir Directory the input JSON lives in (second candidate root).
 * @return The file's contents, or `std::nullopt` when no candidate is readable.
 */
std::optional<std::string> resolve_source(const std::string& recorded,
                                          const std::filesystem::path& json_dir);

}  // namespace server

#endif  // SOURCE_RESOLVER_H
