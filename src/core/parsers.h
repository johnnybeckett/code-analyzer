#ifndef PARSERS_H
#define PARSERS_H

#include "core/iparser.h"
#include "parser/cpp_parser.h"
#include "parser/csharp_parser.h"
#include "parser/python_parser.h"
#include <string>
#include <memory>
#include <vector>

/**
 * @brief Adapt the C++ parser to IParser.
 *
 * CppParser::parse_file already returns a vector (a file may declare several
 * classes), so the adapter forwards the result unchanged.
 */
class CppFileAdapter : public IParser {
public:
    std::vector<std::unique_ptr<Class>> parse_file(const std::string& file_path) override {
        return CppParser::parse_file(file_path);
    }
};

/**
 * @brief Adapt the C# parser to IParser.
 *
 * CSharpParser::parse_file already returns a vector (a file may declare
 * several types), so the adapter forwards the result unchanged.
 */
class CSharpFileAdapter : public IParser {
public:
    std::vector<std::unique_ptr<Class>> parse_file(const std::string& file_path) override {
        return CSharpParser::parse_file(file_path);
    }
};

/**
 * @brief Adapt the Python parser to IParser.
 *
 * PythonParser::parse_file already returns a vector (a module may declare
 * several classes), so the adapter forwards the result unchanged.
 */
class PythonFileAdapter : public IParser {
public:
    std::vector<std::unique_ptr<Class>> parse_file(const std::string& file_path) override {
        return PythonParser::parse_file(file_path);
    }
};

#endif // PARSERS_H
