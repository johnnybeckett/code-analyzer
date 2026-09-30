#ifndef COMPILE_COMMANDS_PROVIDER_H
#define COMPILE_COMMANDS_PROVIDER_H

#include <string>
#include <vector>

#include "core/source_file_provider.h"

/**
 * @brief Discovers source files listed in a CMake compile_commands.json database.
 *
 * Reads the JSON array and returns each entry's "file" value, in order. The
 * validation and error messages match the previous CppParser::parse_compile_commands
 * behavior: a missing or malformed database reports on std::cerr and yields an
 * empty list (graceful degradation, as before). Parsing is left to the
 * Analyzer, which now routes every file through the IParser/ParserRegistry
 * pipeline — the same path as the directory walk.
 */
class CompileCommandsFileProvider final : public SourceFileProvider {
public:
    explicit CompileCommandsFileProvider(std::string path);

    std::vector<std::string> files() const override;
    std::string banner() const override;

private:
    std::string path_;
};

#endif // COMPILE_COMMANDS_PROVIDER_H
