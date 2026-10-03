#ifndef REVIEW_STORE_H
#define REVIEW_STORE_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace server {

/**
 * @brief One review comment anchored to a diff row.
 *
 * `id` is server-assigned (`c1`, `c2`, ...) and stable for the life of the file;
 * `created` is the UTC ISO-8601 stamp (`YYYY-MM-DDTHH:MM:SSZ`) set when the
 * comment was added. `old_line`/`new_line` are 1-based line numbers on each
 * side of the diff, and `0` means "that line does not exist on that side" (e.g.
 * a comment on a purely added line has `old_line == 0`).
 */
struct ReviewComment {
    std::string id;
    std::string file;
    long long old_line = 0;
    long long new_line = 0;
    std::string text;
    std::string created;
};

/**
 * @brief A persisted collection of review comments, backed by one JSON file.
 *
 * The file is the single source of truth: it is loaded (fail-soft) on
 * construction and re-saved atomically on every `add()`, so comments survive a
 * server restart. The store is deliberately self-contained — it knows nothing
 * about the HTTP layer or the model; it only owns the load/validate/serialize/
 * persist logic, which is what lets it be exercised directly in the tests.
 *
 * Thread-safety: every public method takes `mu_`. Today all calls come from the
 * single-threaded io_context, but the lock is cheap future-proofing and keeps
 * the store usable from the tests' worker thread without extra machinery.
 */
class ReviewStore {
public:
    /**
     * @brief Load the comment file at `path` if it exists.
     *
     * Missing file -> an empty store (first run). Unreadable or malformed JSON
     * -> a stderr warning and an empty store; a bad file must never prevent the
     * server from starting.
     */
    explicit ReviewStore(std::string path);

    /** @brief A snapshot of all comments, in stored (first-seen) order. */
    std::vector<ReviewComment> list() const;

    /**
     * @brief Assign an `id` and `created` stamp, append, and persist atomically.
     *
     * The comment is appended to the in-memory list regardless of whether the
     * write succeeds; a failed write throws (see below) and the in-memory state
     * is left intact so the next `add()` retries against the same base.
     * @throws std::runtime_error if the file cannot be written (the in-memory
     *         list still contains the comment).
     * @return The stored comment (with `id` and `created` filled in).
     */
    ReviewComment add(ReviewComment comment);

    /** @brief The absolute/relative path of the backing file. */
    std::string path() const;

    /**
     * @brief The store serialized as JSON.
     *
     * Format: `{"version":1,"comments":[{id,file,oldLine,newLine,text,created}]}`
     * (camelCase wire keys, matching the client's POST/GET payloads).
     */
    std::string to_json() const;

private:
    // Serializes comments_ to JSON without taking the lock; callers
    // (add()/to_json()) are responsible for holding mu_ first.
    std::string to_json_locked() const;

    std::string path_;
    mutable std::mutex mu_;  // guards comments_ (see class doc)
    std::vector<ReviewComment> comments_;
};

}  // namespace server

#endif  // REVIEW_STORE_H
