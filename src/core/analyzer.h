#ifndef ANALYZER_H
#define ANALYZER_H

#include "model.h"
#include <string>
#include <memory>

/**
 * @brief Main analyzer class for code analysis
 */
class Analyzer {
public:
    /**
     * @brief Analyze a project directory
     * @param project_path Path to the project directory
     * @return AnalysisResult containing the parsed information
     */
    static AnalysisResult analyze_project(const std::string& project_path);

    /**
     * @brief Analyze compile_commands.json file
     * @param compile_commands_path Path to compile_commands.json
     * @return AnalysisResult containing the parsed information
     */
    static AnalysisResult analyze_compile_commands(const std::string& compile_commands_path);

private:
    // Private constructor to prevent instantiation
    Analyzer() = default;
};

#endif // ANALYZER_H