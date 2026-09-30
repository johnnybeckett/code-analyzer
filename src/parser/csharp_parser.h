#ifndef CSHARP_PARSER_H
#define CSHARP_PARSER_H

#include "core/model.h"
#include <string>
#include <memory>

/**
 * @brief C# parser implementation
 */
class CSharpParser {
public:
    /**
     * @brief Parse a C# file
     * @param file_path Path to the C# file
     * @return Parsed Class object or nullptr if error
     */
    static std::unique_ptr<Class> parse_file(const std::string& file_path);

    /**
     * @brief Parse C# project files
     * @param project_path Path to the C# project directory
     * @return AnalysisResult containing parsed information
     */
    static AnalysisResult parse_project(const std::string& project_path);

private:
    // Private constructor to prevent instantiation
    CSharpParser() = default;

    /**
     * @brief Parse the content of a class to extract methods and variables
     * @param content Full file content
     * @param start_pos Position where the class starts
     * @param class_obj Class object to populate with extracted data
     */
    static void parse_class_content(const std::string& content, size_t start_pos, Class* class_obj);

    /**
     * @brief Parse methods from class content
     * @param content Content of the class
     * @param class_obj Class object to populate with methods
     */
    static void parse_methods(const std::string& content, Class* class_obj);

    /**
     * @brief Parse fields/variables from class content
     * @param content Content of the class
     * @param class_obj Class object to populate with variables
     */
    static void parse_fields(const std::string& content, Class* class_obj);
};

#endif // CSHARP_PARSER_H