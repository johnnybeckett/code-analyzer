#ifndef SERVER_CLASS_INDEX_H
#define SERVER_CLASS_INDEX_H

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <boost/json.hpp>

namespace server {

/** @brief One method's summary as carried in the class index. */
struct MethodSummary {
    std::string name;
    std::string return_type;
    std::string visibility;
    bool is_static = false;
    std::size_t param_count = 0;
    std::vector<std::string> called_methods;
};

/** @brief One variable's summary as carried in the class index. */
struct VariableSummary {
    std::string name;
    std::string type;
    std::string mutability;
    std::string visibility;
};

/**
 * @brief One class's immutable record.
 *
 * The field set mirrors the JSON the analyzer emits
 * (`src/core/json_serializer.cpp`), so a record can be built 1:1 from a
 * serialized class object. `namespace_name` holds the JSON `"namespace"` key
 * (C++ forbids the contextual keyword as an identifier). The 3D position is
 * assigned by the owning `ClassIndex`, not by the caller.
 */
struct ClassRecord {
    std::string name;
    std::string kind;
    std::string namespace_name;
    std::string visibility;
    bool is_static = false;
    std::string file;
    std::vector<std::string> inheritance;
    std::vector<MethodSummary> methods;
    std::vector<VariableSummary> variables;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/** @brief Axis-aligned bounds of the assigned positions (all zero when empty). */
struct ClassIndexBounds {
    double min_x = 0.0, max_x = 0.0;
    double min_y = 0.0, max_y = 0.0;
    double min_z = 0.0, max_z = 0.0;

    [[nodiscard]] constexpr bool empty() const {
        return min_x == 0.0 && max_x == 0.0 && min_y == 0.0 && max_y == 0.0
            && min_z == 0.0 && max_z == 0.0;
    }
};

/**
 * @brief Deterministic, spatially-indexed set of class records for a project.
 *
 * Records are laid out once, at construction, into a deterministic
 * namespace-grouped 3D grid: classes in the same namespace occupy a small
 * cube (groups sorted by namespace name, members sorted by name within a
 * group), and the groups are tiled along +X with a fixed gap. The result is
 * stable across runs and processes, so the server and the viewer never drift
 * apart on coordinates. A uniform grid-bucket index (cell size
 * `kCellSize`) answers `nearest()` in a bounded amount of work instead of a
 * full scan.
 *
 * The index is immutable after construction and safe to share (the server
 * hands one instance to both `/classes` handlers). `nearest()` and `sample()`
 * return a span into an internal scratch buffer, so a caller must consume a
 * span before making the next call on the same instance — the server is
 * single-threaded per connection, so this is the natural usage.
 */
class ClassIndex {
public:
    /** @brief Uniform grid-bucket edge length; assigned positions sit on it. */
    static constexpr double kCellSize = 1.0;

    /** @brief Gap (in cell units) between namespace groups along +X. */
    static constexpr double kGroupGap = 4.0;

    /**
     * @brief Upper bound on the records a single nearest() query may request.
     *
     * The `/classes/near` endpoint clamps its `count` parameter to this so a
     * single response is bounded (a viewer asking for the "visible window"
     * wants tens-to-hundreds, never the whole 10k-class set in one body).
     */
    static constexpr std::size_t kMaxNearest = 256;

    /** @brief Build the index and assign the grid positions (records moved in). */
    explicit ClassIndex(std::vector<ClassRecord> records);

    [[nodiscard]] constexpr std::size_t size() const { return records_.size(); }
    [[nodiscard]] constexpr ClassIndexBounds bounds() const { return bounds_; }
    [[nodiscard]] constexpr double cellSize() const { return kCellSize; }

    /** @brief All records in input order (positions already assigned). */
    [[nodiscard]] std::span<const ClassRecord> records() const;

    /**
     * @brief The `count` records closest to `(x, y, z)`, ascending by distance.
     *
     * `count` is clamped to `size()`; zero (or an empty index) yields an empty
     * span. The answer is correct against the *entire* set — the bucket
     * expansion stops as soon as the remaining rings can no longer contain a
     * closer record.
     */
    [[nodiscard]] std::span<const ClassRecord> nearest(std::size_t count,
                                                       double x, double y, double z) const;

    /**
     * @brief A deterministic downsample of `k` records spread over the layout.
     *
     * Evenly spaced in layout order (first and last record included when
     * `k > 1`), so it is a cheap far-field overview of the whole project.
     * `k` is clamped to `size()`; zero (or an empty index) yields an empty span.
     */
    [[nodiscard]] std::span<const ClassRecord> sample(std::size_t k) const;

    /**
     * @brief Parse one serialized class object (analyzer JSON) into a record.
     *
     * The position fields stay zero — the owning `ClassIndex` assigns them.
     * `std::nullopt` on a parse failure or a non-object document, so a single
     * malformed record never aborts the whole index build.
     */
    [[nodiscard]] static std::optional<ClassRecord> record_from_json(
        const std::string& class_json);

private:
    void assign_layout();
    void build_buckets();

    std::vector<ClassRecord> records_;
    ClassIndexBounds bounds_;
    std::map<std::array<int, 3>, std::vector<std::size_t>> buckets_;
    mutable std::vector<ClassRecord> scratch_;  // backing store for the spans
};

/**
 * @brief Serialize one class record to analyzer-compatible JSON.
 *
 * For every field `record_from_json()` preserves, this is its inverse:
 * `record_from_json(to_json(r))` reproduces `r`'s name, kind, namespace,
 * visibility, static flag, file, inheritance, and method/variable summaries
 * (the parameter count via the `"parameters"` array's size, since
 * `MethodSummary` stores only the count). It also emits the record's assigned
 * `x`/`y`/`z`, which `record_from_json()` ignores but the viewer needs to
 * place the class in the 3D scene.
 */
[[nodiscard]] boost::json::value to_json(const ClassRecord& rec);

}  // namespace server

#endif  // SERVER_CLASS_INDEX_H
