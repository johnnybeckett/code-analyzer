#ifndef ANALYSIS_REACTOR_H
#define ANALYSIS_REACTOR_H

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>

#include "core/model.h"
#include "core/parser_registry.h"

/**
 * @brief The finished parse of a single file, produced by exactly one worker.
 *
 * Workers only ever write their own `index` slot and never read each other's,
 * so the payload needs no internal locking: the mutex in AnalysisReactor is
 * the sole synchronisation point, guarding the hand-off to the builder.
 *
 * `ok` is false (and `error` set) only when parsing threw; a file with no
 * parser or no classes is a *successful* empty outcome, not an error.
 */
struct ParseOutcome {
    std::size_t index = 0;
    std::string file;
    bool ok = true;
    std::vector<std::unique_ptr<Class>> classes;
    std::exception_ptr error;   // set only when ok == false
    bool ready = false;         // set by the worker once the outcome is final
};

/**
 * @brief Parses a batch of files on a worker pool and hands the results to a
 *        single builder, in input order.
 *
 * This is the analyzer's "reactor": many producers (the pool) parse files in
 * parallel; one consumer (the builder) folds them into the shared result. The
 * builder callback is the only thread that touches the analysis result and the
 * only place any `EventDispatcher` notification originates from, so the result
 * needs no locking and observers see a single, deterministic, in-order event
 * stream even though parsing was parallel.
 *
 * Ownership of the pooled threads is internal and temporary: `run()` starts a
 * pool, submits every file, consumes all outcomes in index order, and joins the
 * pool before returning. Not copyable; safe to use once and discard.
 */
class AnalysisReactor {
public:
    /**
     * @brief The single-writer fold. Invoked (on the thread that called
     *        `run()`) once per file, in index order, with that file's outcome.
     *
     * `outcome` is moved in, so the callback may move its `classes` into the
     * shared result. It runs to completion between consecutive outcomes.
     */
    using Builder = std::function<void(ParseOutcome)>;

    /**
     * @param registry      Where to obtain a parser for a file's extension.
     * @param worker_count  Pool size; 0 is treated as 1.
     * @param builder       The single consumer that folds each outcome.
     */
    AnalysisReactor(const ParserRegistry& registry, unsigned worker_count,
                    Builder builder);

    AnalysisReactor(const AnalysisReactor&) = delete;
    AnalysisReactor& operator=(const AnalysisReactor&) = delete;

    /**
     * @brief Parse every file in `files` (in parallel) and fold each result
     *        into the builder, in the order of `files`.
     *
     * Blocks until every outcome has been consumed. A file that fails to parse
     * still yields an outcome (ok=false) and never aborts the batch. Returns
     * after the pool has joined — no worker outlives this call.
     */
    void run(const std::vector<std::string>& files);

private:
    // Worker body for file `index`: parse it into a ParseOutcome and publish
    // it into slots_[index]. Never throws (all exceptions are captured into
    // the outcome), so a bad file can't take a worker down.
    void parse_one(std::size_t index, const std::string& file);

    const ParserRegistry& registry_;
    unsigned worker_count_;
    Builder builder_;

    // Hand-off state: one slot per file. The builder consumes them strictly in
    // index order; a slot is consumed only after its worker set ready=true.
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<ParseOutcome> slots_;
};

#endif  // ANALYSIS_REACTOR_H
