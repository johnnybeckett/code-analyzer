#ifndef CSHARP_PARSER_H
#define CSHARP_PARSER_H

#include "../core/model.h"
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
};

#endif // CSHARP_PARSER_H