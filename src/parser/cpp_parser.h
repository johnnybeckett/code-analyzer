#ifndef CPP_PARSER_H
#define CPP_PARSER_H

#include "core/model.h"
#include <string>
#include <memory>
#include <vector>

/**
 * @brief C++ parser implementation
 */
class CppParser {
public:
    /**
     * @brief Parse a C++ file
     * @param file_path Path to the C++ file
     * @return All classes found in the file (empty on error)
     */
    static std::vector<std::unique_ptr<Class>> parse_file(const std::string& file_path);

private:
    // Private constructor to prevent instantiation
    CppParser() = default;
};

#endif // CPP_PARSER_H