#ifndef IPARSER_H
#define IPARSER_H

#include "core/model.h"
#include <string>
#include <memory>
#include <vector>

/**
 * @brief Common interface for language-specific source parsers (Strategy).
 *
 * Each language parser is adapted to this single shape (see parsers.h) so the
 * registry and the analyzer depend only on the abstraction and never on a
 * concrete parser. The static parser APIs are left untouched; thin adapters
 * bridge them to this interface.
 *
 * A parser defines how ONE source file is read and decomposed into classes.
 * It deliberately does not define project- or compile-database operations:
 * those act on a collection of files rather than a single file, and forcing
 * them onto this interface would require adapters that cannot honour them to
 * throw (an LSP violation).
 */
class IParser {
public:
    virtual ~IParser() = default;

    /**
     * @brief Parse a single source file into the classes it declares.
     * @param file_path Path to the source file
     * @return The classes found (possibly empty); a failed read yields an
     *         empty vector rather than an exception, so one unreadable file
     *         cannot abort a directory walk
     */
    virtual std::vector<std::unique_ptr<Class>> parse_file(const std::string& file_path) = 0;
};

#endif // IPARSER_H
