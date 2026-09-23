#ifndef PYTHON_PARSER_H
#define PYTHON_PARSER_H

#include "../core/model.h"
#include <string>
#include <memory>

/**
 * @brief Python parser implementation
 */
class PythonParser {
public:
    /**
     * @brief Parse a Python file
     * @param file_path Path to the Python file
     * @return Parsed Class object or nullptr if error
     */
    static std::unique_ptr<Class> parse_file(const std::string& file_path);

    /**
     * @brief Parse Python project files
     * @param project_path Path to the Python project directory
     * @return AnalysisResult containing parsed information
     */
    static AnalysisResult parse_project(const std::string& project_path);

private:
    // Private constructor to prevent instantiation
    PythonParser() = default;
};

#endif // PYTHON_PARSER_H