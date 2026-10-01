#ifndef UML_CLASS_LOADER_H
#define UML_CLASS_LOADER_H

#include <string>
#include <vector>

namespace uml {

/**
 * @brief Reads analyzer JSON files and extracts their class records
 */
class JsonClassLoader {
public:
    /**
     * @brief Parse one JSON file and extract its class records
     * @param filename Path to a single analyzer JSON file
     * @return Vector of serialized class objects (empty on any failure, with
     *         a warning — the graceful path for the CLI generator)
     */
    static std::vector<std::string> parseFileClasses(const std::string& filename);

    /**
     * @brief Parse one JSON file and extract its class records, fail-fast.
     *
     * The strict counterpart of parseFileClasses(): instead of warning and
     * returning an empty list, it throws std::runtime_error with a diagnostic
     * on a missing file, a malformed document, or a missing "classes" array.
     * This is the contract a server (and the refactored UML generator) wants:
     * a bad input must never be served as an empty page.
     *
     * @param filename Path to a single analyzer JSON file
     * @return Vector of serialized class objects (never empty on success)
     * @throws std::runtime_error on missing file / parse error / no classes
     */
    static std::vector<std::string> parseFileClassesStrict(const std::string& filename);

    /**
     * @brief Collect the distinct source files referenced by the classes in one
     *        analyzer JSON file, in first-seen (document) order.
     *
     * Fail-soft (like parseFileClasses): returns an empty vector on a missing
     * file, a malformed document, or a missing "classes" array — with no
     * warning, since an absent "file" simply means nothing to serve. Each
     * class's non-empty "file" string contributes one entry; duplicates are
     * dropped, first-seen order preserved.
     *
     * @param filename Path to a single analyzer JSON file
     * @return Distinct, non-empty "file" strings in first-seen order (may be empty)
     */
    static std::vector<std::string> parseFileSources(const std::string& filename);

    /**
     * @brief Convert class data to a JSON string for JavaScript
     * @param class_data Vector of class data strings
     * @return JSON string representation
     */
    static std::string getClassesJSON(const std::vector<std::string>& class_data);
};

}  // namespace uml

#endif  // UML_CLASS_LOADER_H
