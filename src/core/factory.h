#ifndef FACTORY_H
#define FACTORY_H

#include "model.h"
#include <memory>
#include <string>

/**
 * @brief Base parser factory interface
 */
class ParserFactory {
public:
    virtual ~ParserFactory() = default;

    /**
     * @brief Create a parser for the specified file type
     * @param file_extension File extension to determine parser type
     * @return Unique pointer to created parser
     */
    virtual std::unique_ptr<Class> create_parser(const std::string& file_extension) = 0;
};

/**
 * @brief Concrete factory for language-specific parsers
 */
class LanguageParserFactory : public ParserFactory {
public:
    std::unique_ptr<Class> create_parser(const std::string& file_extension) override;
};

#endif // FACTORY_H