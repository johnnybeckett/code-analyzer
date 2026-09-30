#ifndef CONFIG_H
#define CONFIG_H

#include <optional>
#include <string>
#include <vector>

/**
 * @brief Typed, value-semantic analysis configuration.
 *
 * Replaces the old untyped key/string singleton: options are plain data
 * members (copyable, comparable, testable — no global state to leak between
 * tests or call sites), and documents round-trip through JSON via load/save.
 *
 * JSON shape:
 *   {
 *     "source_extensions": [".cpp", ".cs", ".py"],
 *     "skip_dirs":         [".git", "build"]
 *   }
 * Either key may be omitted; the built-in default is kept for that option.
 */
struct Config {
    /**
     * File extensions (including the dot) that may be parsed. This is the
     * analyzer's standard set — the same list ParserRegistry::standard()
     * registers. Extensions named here that no parser handles are ignored.
     */
    std::vector<std::string> source_extensions{
        ".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hxx", ".hh", ".tpp", ".tcc",
        ".cs", ".py"
    };

    /**
     * Directory names never to descend into during a project walk (VCS
     * metadata, build artifacts, tooling state).
     */
    std::vector<std::string> skip_dirs{
        ".git", "build", "cmake-build", "out", "node_modules", ".claude"
    };

    /**
     * @brief Load configuration from a JSON file.
     * @param path Path to the configuration file
     * @return The parsed configuration, or std::nullopt if the file cannot be
     *         read or is not a valid configuration document.
     */
    static std::optional<Config> load(const std::string& path);

    /**
     * @brief Save this configuration to a JSON file.
     * @param path Path to write
     * @return The serialized document on success, std::nullopt if the file
     *         could not be written.
     */
    std::optional<std::string> save(const std::string& path) const;
};

#endif // CONFIG_H
