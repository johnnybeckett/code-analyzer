#include "core/parser_registry.h"

#include "core/parsers.h"

void ParserRegistry::register_parser(const std::string& extension, ParserFactory factory) {
    parsers_[extension] = std::move(factory);
}

std::unique_ptr<IParser> ParserRegistry::create(const std::string& extension) const {
    auto it = parsers_.find(extension);
    if (it == parsers_.end()) return nullptr;
    return it->second();
}

bool ParserRegistry::has_parser(const std::string& extension) const {
    return parsers_.find(extension) != parsers_.end();
}

ParserRegistry ParserRegistry::standard() {
    ParserRegistry registry;

    // C++ source and headers, plus out-of-line template definitions. Note
    // ".hxx": it is part of the set the analyzer has always parsed.
    const char* const cpp_extensions[] = {
        ".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hxx", ".hh", ".tpp", ".tcc"
    };
    for (const auto* ext : cpp_extensions) {
        registry.register_parser(ext, [] { return std::make_unique<CppFileAdapter>(); });
    }

    registry.register_parser(".cs", [] { return std::make_unique<CSharpFileAdapter>(); });
    registry.register_parser(".py", [] { return std::make_unique<PythonFileAdapter>(); });

    return registry;
}

ParserRegistry ParserRegistry::standard(const std::vector<std::string>& extensions) {
    const ParserRegistry all = standard();
    ParserRegistry subset;
    for (const auto& ext : extensions) {
        auto it = all.parsers_.find(ext);
        if (it != all.parsers_.end()) {
            subset.parsers_[ext] = it->second;
        }
    }
    return subset;
}
