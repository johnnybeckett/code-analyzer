#include "server/review_store.h"

#include <boost/json.hpp>

#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace server {

namespace json = boost::json;

namespace {

/** @brief A UTC `YYYY-MM-DDTHH:MM:SSZ` timestamp for the current instant. */
std::string utc_stamp() {
    const std::time_t now = std::time(nullptr);
    const std::tm* gm = std::gmtime(&now);
    if (gm == nullptr) {
        return {};
    }
    char buf[32] = {0};
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gm) == 0) {
        return {};
    }
    return buf;
}

/** @brief True when `c` is ASCII whitespace. */
bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/** @brief `true` when `s` is empty or whitespace-only. */
bool blank(const std::string& s) {
    for (char c : s) {
        if (!is_space(c)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief The next `c<N>` id: one past the largest numeric suffix already in use.
 *
 * `c1` is the first id; `c10` and `c2` both exist -> next is `c11`. A malformed
 * entry (no digit) is ignored so one hand-edited line can't stall id allocation.
 */
std::string next_id(const std::vector<ReviewComment>& comments) {
    long long max_n = 0;
    for (const auto& c : comments) {
        const std::string& id = c.id;
        if (id.size() < 2 || id[0] != 'c') {
            continue;
        }
        long long n = 0;
        bool all_digits = true;
        for (std::size_t i = 1; i < id.size(); ++i) {
            const char ch = id[i];
            if (ch < '0' || ch > '9') {
                all_digits = false;
                break;
            }
            n = n * 10 + (ch - '0');
        }
        if (all_digits && n > max_n) {
            max_n = n;
        }
    }
    return "c" + std::to_string(max_n + 1);
}

/**
 * @brief Build a single `json::object` from a comment (camelCase wire keys).
 */
json::object to_obj(const ReviewComment& c) {
    json::object obj;
    obj["id"] = c.id;
    obj["file"] = c.file;
    obj["oldLine"] = static_cast<std::int64_t>(c.old_line);
    obj["newLine"] = static_cast<std::int64_t>(c.new_line);
    obj["text"] = c.text;
    obj["created"] = c.created;
    return obj;
}

/**
 * @brief Parse one raw comment object, skipping it (with a warning) if a field
 *        is malformed. Tolerant on purpose: the file may have been hand-edited.
 */
bool parse_entry(const json::value& v, std::size_t idx, ReviewComment& out) {
    if (!v.is_object()) {
        std::cerr << "ReviewStore: comment #" << idx << " is not an object; skipped\n";
        return false;
    }
    const auto& obj = v.as_object();

    auto get_str = [&](const char* key) -> std::string {
        const auto it = obj.find(key);
        if (it != obj.end() && it->value().is_string()) {
            // Direct-initialize: as_string() yields boost::json::string, which
            // converts to std::string_view (not std::string), so copy-init of a
            // std::string from it would be a two-step user conversion (invalid).
            return std::string(it->value().as_string());
        }
        return {};
    };

    // text is the one required field; everything else falls back to a default.
    const std::string text = get_str("text");
    if (blank(text)) {
        std::cerr << "ReviewStore: comment #" << idx << " has no text; skipped\n";
        return false;
    }

    auto get_line = [&](const char* key) -> long long {
        const auto it = obj.find(key);
        if (it != obj.end() && it->value().is_int64()) {
            const long long n = it->value().as_int64();
            return n < 0 ? 0 : n;  // negative -> treat as "absent" rather than corrupt
        }
        return 0;
    };

    out.id = get_str("id");
    out.file = get_str("file");
    out.old_line = get_line("oldLine");
    out.new_line = get_line("newLine");
    out.text = text;
    out.created = get_str("created");
    return true;
}

/**
 * @brief Write `contents` to `path` atomically (temp file + rename).
 *
 * The temp file lives beside the target so the rename is on the same filesystem
 * and thus atomic. Any step failing removes the temp file and reports false;
 * the target is never left truncated.
 */
bool write_atomic(const std::string& path, const std::string& contents) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "ReviewStore: cannot open " << tmp << " for writing\n";
            return false;
        }
        out << contents;
        out.flush();
        if (!out) {
            std::cerr << "ReviewStore: write failed for " << tmp << "\n";
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::error_code rm;
        std::filesystem::remove(tmp, rm);  // best-effort cleanup
        std::cerr << "ReviewStore: cannot move " << tmp << " into place: " << ec.message() << "\n";
        return false;
    }
    return true;
}

}  // namespace

ReviewStore::ReviewStore(std::string path) : path_(std::move(path)) {
    std::ifstream in(path_, std::ios::binary);
    if (!in) {
        return;  // first run: nothing to load
    }
    std::ostringstream data;
    data << in.rdbuf();
    const std::string contents = data.str();

    json::value parsed;
    try {
        parsed = json::parse(contents);
    } catch (const std::exception& e) {
        std::cerr << "ReviewStore: " << path_ << " is not valid JSON (" << e.what()
                  << "); starting with no comments\n";
        return;
    }

    if (!parsed.is_object()) {
        std::cerr << "ReviewStore: " << path_ << " has a non-object root; ignored\n";
        return;
    }
    const auto it = parsed.as_object().find("comments");
    if (it == parsed.as_object().end() || !it->value().is_array()) {
        return;  // no comments array -> empty store
    }

    std::vector<ReviewComment> loaded;
    std::size_t idx = 0;
    for (const auto& entry : it->value().as_array()) {
        ++idx;
        ReviewComment c;
        if (parse_entry(entry, idx, c)) {
            loaded.push_back(std::move(c));
        }
    }
    comments_ = std::move(loaded);
}

std::vector<ReviewComment> ReviewStore::list() const {
    std::lock_guard lock(mu_);
    return comments_;
}

ReviewComment ReviewStore::add(ReviewComment comment) {
    std::lock_guard lock(mu_);
    comment.id = next_id(comments_);
    comment.created = utc_stamp();
    const ReviewComment stored = comment;

    // Append first so to_json_locked() includes the new comment, then persist.
    // If the write/rename fails we roll the in-memory list back to the pre-add
    // state (the doc contract) and throw; retrying add() then re-derives the
    // same id and tries again against the same base.
    comments_.push_back(std::move(comment));
    const std::string snapshot = to_json_locked();

    const bool ok = write_atomic(path_, snapshot);
    if (!ok) {
        comments_.pop_back();
        throw std::runtime_error("ReviewStore: failed to persist " + path_);
    }
    return stored;
}

std::string ReviewStore::path() const {
    std::lock_guard lock(mu_);
    return path_;
}

std::string ReviewStore::to_json() const {
    std::lock_guard lock(mu_);
    return to_json_locked();
}

// Serializes without taking the lock; callers (add()/to_json()) already hold it.
std::string ReviewStore::to_json_locked() const {
    json::object root;
    root["version"] = std::int64_t{1};
    json::array arr;
    for (const auto& c : comments_) {
        arr.push_back(to_obj(c));
    }
    root["comments"] = std::move(arr);
    return json::serialize(root);
}

}  // namespace server
