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
     * @return Vector of serialized class objects
     */
    static std::vector<std::string> parseFileClasses(const std::string& filename);

    /**
     * @brief Convert class data to a JSON string for JavaScript
     * @param class_data Vector of class data strings
     * @return JSON string representation
     */
    static std::string getClassesJSON(const std::vector<std::string>& class_data);
};

}  // namespace uml

#endif  // UML_CLASS_LOADER_H
