#include "core/analyzer.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "core/analysis_reactor.h"
#include "core/commit_provider.h"
#include "core/compile_commands_provider.h"
#include "core/directory_provider.h"

Analyzer::Analyzer(const Config& config, const ParserRegistry& registry,
                   EventDispatcher* dispatcher)
    : config_(config), registry_(registry), dispatcher_(dispatcher) {}

AnalysisResult Analyzer::analyze(const SourceFileProvider& provider) {
    AnalysisResult result;

    std::cout << provider.banner() << std::endl;

    // Materialize the file list once so the pool and the builder agree on the
    // same order. Parsing is parallel (the reactor's workers); folding is not:
    // this thread is the single builder, and it is the only place events are
    // emitted, so EventDispatcher/observers stay single-threaded and see a
    // deterministic, in-order stream.
    const std::vector<std::string> files = provider.files();
    const std::size_t n = files.size();

    if (n > 0) {
        // The builder: the single writer. It emits each file's progress event
        // and folds its classes into the shared result; a failed file is
        // reported (and skipped) exactly as the old serial loop did.
        auto builder = [this, &result](ParseOutcome out) {
            emit(AnalysisEvent{AnalysisEvent::Kind::FileParsed, out.file, 0});
            if (out.ok) {
                for (auto& parsed_class : out.classes) {
                    result.add_class(std::move(parsed_class));
                }
                return;
            }
            // A single unreadable/malformed file must not abort the run.
            try {
                if (out.error) {
                    std::rethrow_exception(out.error);
                }
                throw std::runtime_error("unknown parse error");
            } catch (const std::exception& e) {
                std::cerr << "Warning: skipping file " << out.file
                          << " (" << e.what() << ")" << std::endl;
            } catch (...) {
                std::cerr << "Warning: skipping file " << out.file
                          << " (unknown error)" << std::endl;
            }
        };

        // Size the pool to the machine, capped so a huge tree doesn't oversub-
        // scribe, and never larger than the work.
        const std::size_t hw = std::thread::hardware_concurrency();
        std::size_t workers = std::min<std::size_t>(n, (hw > 0 ? hw : 1));
        workers = std::min<std::size_t>(workers, 32);
        if (workers < 1) {
            workers = 1;
        }

        AnalysisReactor reactor(registry_, static_cast<unsigned>(workers),
                                std::move(builder));
        reactor.run(files);
    }

    AnalysisEvent complete;
    complete.kind = AnalysisEvent::Kind::AnalysisComplete;
    complete.class_count = result.classes.size();
    emit(complete);

    return result;
}

AnalysisResult Analyzer::analyze_project(const std::string& project_path) {
    // The directory walk (and its pruning of the configured skip directories)
    // is a provider; the analysis itself is shared with every input source.
    DirectoryFileProvider provider(project_path, config_.skip_dirs);
    return analyze(provider);
}

AnalysisResult Analyzer::analyze_compile_commands(const std::string& compile_commands_path) {
    // A compile database is just another file source: read the listed
    // translation units, then parse them through the standard pipeline.
    CompileCommandsFileProvider provider(compile_commands_path);
    return analyze(provider);
}

AnalysisResult Analyzer::analyze_commit(const std::string& repo,
                                        const std::string& ref,
                                        const std::string& staging) {
    // A commit-pinned source is just another file source: materialize the
    // git-show content (submodules at their parent-pinned SHAs), then parse
    // it through the standard pipeline.
    CommitFileProvider provider(repo, ref, staging);
    return analyze(provider);
}

void Analyzer::emit(const AnalysisEvent& event) const {
    if (dispatcher_) {
        dispatcher_->notify(event);
    }
}
