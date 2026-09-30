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
 * CSharpParser::parse_file returns 0-or-1 classes; the adapter lifts that
 * into a vector so callers see the unified IParser shape.
 */
class CSharpFileAdapter : public IParser {
public:
    std::vector<std::unique_ptr<Class>> parse_file(const std::string& file_path) override {
        auto parsed = CSharpParser::parse_file(file_path);
        if (!parsed) return {};
        std::vector<std::unique_ptr<Class>> result;
        result.emplace_back(std::move(parsed));
        return result;
    }
};

/**
 * @brief Adapt the Python parser to IParser.
 *
 * PythonParser::parse_file returns 0-or-1 classes; the adapter lifts that
 * into a vector so callers see the unified IParser shape.
 */
class PythonFileAdapter : public IParser {
public:
    std::vector<std::unique_ptr<Class>> parse_file(const std::string& file_path) override {
        auto parsed = PythonParser::parse_file(file_path);
        if (!parsed) return {};
        std::vector<std::unique_ptr<Class>> result;
        result.emplace_back(std::move(parsed));
        return result;
    }
};

#endif // PARSERS_H
