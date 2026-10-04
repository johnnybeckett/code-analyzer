#include "core/analysis_reactor.h"

#include <filesystem>

#include "core/thread_pool.h"

AnalysisReactor::AnalysisReactor(const ParserRegistry& registry,
                                 unsigned worker_count, Builder builder)
    : registry_(registry),
      worker_count_(worker_count == 0 ? 1 : worker_count),
      builder_(std::move(builder)) {}

void AnalysisReactor::parse_one(std::size_t index, const std::string& file) {
    ParseOutcome outcome;
    outcome.index = index;
    outcome.file = file;
    try {
        const std::string extension =
            std::filesystem::path(file).extension().string();
        if (auto parser = registry_.create(extension); parser) {
            for (auto& parsed_class : parser->parse_file(file)) {
                outcome.classes.push_back(std::move(parsed_class));
            }
        }
        outcome.ok = true;
    } catch (const std::exception&) {
        outcome.ok = false;
        outcome.error = std::current_exception();
    } catch (...) {
        outcome.ok = false;
        outcome.error = std::current_exception();
    }
    outcome.ready = true;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        slots_[index] = std::move(outcome);
    }
    cv_.notify_all();
}

void AnalysisReactor::run(const std::vector<std::string>& files) {
    const std::size_t n = files.size();
    if (n == 0) {
        return;  // nothing to parse; no pool, no outcomes
    }

    slots_.resize(n);  // n default-empty slots (ParseOutcome is not copyable)
    for (std::size_t i = 0; i < n; ++i) {
        slots_[i].index = i;
        slots_[i].file = files[i];  // so a worker crash still names the file
        slots_[i].ready = false;
    }

    // Scope the pool: it is joined (all workers stopped) when this block ends,
    // which only happens after every outcome below has been consumed.
    {
        ThreadPool pool(worker_count_);
        // files is a caller-owned, read-only list for the whole run; workers
        // only ever read files[i]. The lambda captures its index by value.
        for (std::size_t i = 0; i < n; ++i) {
            pool.submit([this, i, &files] { parse_one(i, files[i]); });
        }

        // Builder (this thread): consume strictly in index order.
        for (std::size_t i = 0; i < n; ++i) {
            ParseOutcome outcome;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                // Every task was submitted above, so slots_[i] is guaranteed to
                // become ready; this wait always terminates.
                cv_.wait(lock, [this, i] { return slots_[i].ready; });
                outcome = std::move(slots_[i]);
            }
            builder_(std::move(outcome));
        }
    }  // pool destroyed here: joins all workers before we return
}
