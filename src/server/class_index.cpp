#include "server/class_index.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <boost/json.hpp>

namespace server {

namespace {

/**
 * @brief Minimum distance from point `(x, y, z)` to any point in a grid cell
 * whose Chebyshev cell-offset from the query cell `(qx, qy, qz)` is exactly
 * `r` (with `r >= 1`).
 *
 * A cell in that ring has at least one axis offset by `r`; the closest such
 * cell puts exactly one axis at offset `r` and the other two at offset 0, so
 * the ring's minimum is the nearest "face" among those six axis-offset cells.
 * Used as the proven lower bound that lets `nearest()` stop expanding rings as
 * soon as the next ring can no longer contain a closer record.
 */
double ring_min(double x, double y, double z, int qx, int qy, int qz, int r, double cell) {
    double m = 1e300;
    m = std::min(m, (qx + r) * cell - x);
    m = std::min(m, x - (qx - r + 1) * cell);
    m = std::min(m, (qy + r) * cell - y);
    m = std::min(m, y - (qy - r + 1) * cell);
    m = std::min(m, (qz + r) * cell - z);
    m = std::min(m, z - (qz - r + 1) * cell);
    return m;
}

}  // namespace

ClassIndex::ClassIndex(std::vector<ClassRecord> records)
    : records_(std::move(records)) {
    assign_layout();
    build_buckets();
}

std::span<const ClassRecord> ClassIndex::records() const {
    return {records_.data(), records_.size()};
}

std::span<const ClassRecord> ClassIndex::nearest(std::size_t count,
                                                 double x, double y, double z) const {
    if (records_.empty()) {
        scratch_.clear();
        return {};
    }
    count = std::min(count, records_.size());
    if (count == 0) {
        scratch_.clear();
        return {};
    }

    const double cell = kCellSize;
    const int qx = static_cast<int>(std::floor(x / cell));
    const int qy = static_cast<int>(std::floor(y / cell));
    const int qz = static_cast<int>(std::floor(z / cell));

    struct Cand {
        double d2;
        std::size_t i;
    };
    auto dist2 = [this, x, y, z](std::size_t i) {
        const ClassRecord& r = records_[i];
        const double dx = r.x - x, dy = r.y - y, dz = r.z - z;
        return dx * dx + dy * dy + dz * dz;
    };

    // Farthest bucket (Chebyshev) from the query cell — bounds the expansion so
    // the loop always terminates; at that radius every bucket has been gathered.
    int max_r = 0;
    for (const auto& [key, idxs] : buckets_) {
        const int m = std::max({std::abs(key[0] - qx),
                                std::abs(key[1] - qy),
                                std::abs(key[2] - qz)});
        max_r = std::max(max_r, m);
    }

    std::vector<Cand> cand;
    int r = 0;
    for (;; ++r) {
        // Gather exactly the buckets whose Chebyshev offset from the query cell
        // is `r` (each bucket is added once, at its own radius).
        for (int dx = -r; dx <= r; ++dx)
            for (int dy = -r; dy <= r; ++dy)
                for (int dz = -r; dz <= r; ++dz) {
                    const int m = std::max({std::abs(dx), std::abs(dy), std::abs(dz)});
                    if (m != r) continue;
                    const auto it = buckets_.find(std::array<int, 3>{qx + dx, qy + dy, qz + dz});
                    if (it == buckets_.end()) continue;
                    for (std::size_t i : it->second) cand.push_back({dist2(i), i});
                }

        if (cand.size() >= count) {
            // The N-th best distance: the largest among the `count` closest
            // gathered so far. If the next ring's proven lower bound is at least
            // that, no un-gathered record can be closer, so the answer is final.
            std::nth_element(cand.begin(), cand.begin() + count, cand.end(),
                             [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
            double nth_d2 = 0.0;
            for (std::size_t i = 0; i < count; ++i) nth_d2 = std::max(nth_d2, cand[i].d2);
            const double next_min = ring_min(x, y, z, qx, qy, qz, r + 1, cell);
            if (next_min >= std::sqrt(nth_d2) || r == max_r) break;
        }
    }

    std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) {
        if (a.d2 != b.d2) return a.d2 < b.d2;
        return a.i < b.i;  // deterministic tiebreak on input order
    });

    scratch_.clear();
    scratch_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) scratch_.push_back(records_[cand[i].i]);
    return {scratch_.data(), scratch_.size()};
}

std::span<const ClassRecord> ClassIndex::sample(std::size_t k) const {
    if (records_.empty()) {
        scratch_.clear();
        return {};
    }
    k = std::min(k, records_.size());
    if (k == 0) {
        scratch_.clear();
        return {};
    }

    scratch_.clear();
    scratch_.reserve(k);
    const std::size_t n = records_.size();
    if (k == 1) {
        scratch_.push_back(records_.front());
    } else {
        // Evenly spaced across layout order, first and last record included.
        for (std::size_t j = 0; j < k; ++j) {
            const std::size_t idx = (j * (n - 1)) / (k - 1);
            scratch_.push_back(records_[idx]);
        }
    }
    return {scratch_.data(), scratch_.size()};
}

void ClassIndex::assign_layout() {
    if (records_.empty()) {
        bounds_ = ClassIndexBounds{};
        return;
    }

    // Group by namespace (map → sorted group order), then sort each group's
    // members by name (index as a deterministic tiebreak). Pure function of the
    // input, so two builds of the same records yield identical positions.
    std::map<std::string, std::vector<std::size_t>> groups;
    for (std::size_t i = 0; i < records_.size(); ++i)
        groups[records_[i].namespace_name].push_back(i);

    const double cell = kCellSize;
    double group_x = 0.0;
    for (auto& [ns, idxs] : groups) {
        std::sort(idxs.begin(), idxs.end(), [this](std::size_t a, std::size_t b) {
            const std::string& na = records_[a].name;
            const std::string& nb = records_[b].name;
            if (na != nb) return na < nb;
            return a < b;
        });

        const std::size_t n = idxs.size();
        std::size_t side = 1;
        while (side * side * side < n) ++side;  // smallest 3D cube holding n

        for (std::size_t k = 0; k < n; ++k) {
            const std::size_t ix = k % side;
            const std::size_t iy = (k / side) % side;
            const std::size_t iz = k / (side * side);
            ClassRecord& rec = records_[idxs[k]];
            rec.x = (group_x + static_cast<double>(ix)) * cell;
            rec.y = static_cast<double>(iy) * cell;
            rec.z = static_cast<double>(iz) * cell;
        }

        group_x += static_cast<double>(side) * cell + kGroupGap;
    }

    const ClassRecord& first = records_.front();
    bounds_ = ClassIndexBounds{first.x, first.x, first.y, first.y, first.z, first.z};
    for (const ClassRecord& rec : records_) {
        bounds_.min_x = std::min(bounds_.min_x, rec.x);
        bounds_.max_x = std::max(bounds_.max_x, rec.x);
        bounds_.min_y = std::min(bounds_.min_y, rec.y);
        bounds_.max_y = std::max(bounds_.max_y, rec.y);
        bounds_.min_z = std::min(bounds_.min_z, rec.z);
        bounds_.max_z = std::max(bounds_.max_z, rec.z);
    }
}

void ClassIndex::build_buckets() {
    buckets_.clear();
    const double cell = kCellSize;
    for (std::size_t i = 0; i < records_.size(); ++i) {
        std::array<int, 3> key{
            static_cast<int>(std::floor(records_[i].x / cell)),
            static_cast<int>(std::floor(records_[i].y / cell)),
            static_cast<int>(std::floor(records_[i].z / cell))};
        buckets_[key].push_back(i);
    }
}

std::optional<ClassRecord> ClassIndex::record_from_json(const std::string& class_json) {
    namespace json = boost::json;
    std::error_code ec;
    const json::value doc = json::parse(class_json, ec);
    if (ec || !doc.is_object()) return std::nullopt;
    const auto& o = doc.as_object();

    auto get_str = [&o](const char* key) -> std::string {
        if (const auto it = o.find(key); it != o.end() && it->value().is_string())
            return std::string(it->value().as_string().c_str());
        return {};
    };
    auto get_bool = [&o](const char* key) -> bool {
        if (const auto it = o.find(key); it != o.end() && it->value().is_bool())
            return it->value().as_bool();
        return false;
    };
    auto get_str_array = [&o](const char* key) -> std::vector<std::string> {
        std::vector<std::string> out;
        if (const auto it = o.find(key); it != o.end() && it->value().is_array())
            for (const auto& e : it->value().as_array())
                if (e.is_string()) out.emplace_back(e.as_string().c_str());
        return out;
    };

    ClassRecord r;
    r.name = get_str("name");
    r.kind = get_str("kind");
    r.namespace_name = get_str("namespace");
    r.visibility = get_str("visibility");
    r.is_static = get_bool("static");
    r.file = get_str("file");
    r.inheritance = get_str_array("inheritance");

    if (const auto it = o.find("methods"); it != o.end() && it->value().is_array()) {
        for (const auto& e : it->value().as_array()) {
            if (!e.is_object()) continue;
            const auto& mo = e.as_object();
            auto mstr = [&mo](const char* key) -> std::string {
                if (const auto m = mo.find(key); m != mo.end() && m->value().is_string())
                    return std::string(m->value().as_string().c_str());
                return {};
            };
            auto mbool = [&mo](const char* key) -> bool {
                if (const auto m = mo.find(key); m != mo.end() && m->value().is_bool())
                    return m->value().as_bool();
                return false;
            };
            MethodSummary ms;
            ms.name = mstr("name");
            ms.return_type = mstr("return_type");
            ms.visibility = mstr("visibility");
            ms.is_static = mbool("static");
            if (const auto p = mo.find("parameters"); p != mo.end() && p->value().is_array())
                ms.param_count = p->value().as_array().size();
            if (const auto c = mo.find("called_methods");
                c != mo.end() && c->value().is_array())
                for (const auto& ce : c->value().as_array())
                    if (ce.is_string()) ms.called_methods.emplace_back(ce.as_string().c_str());
            r.methods.push_back(std::move(ms));
        }
    }

    if (const auto it = o.find("variables"); it != o.end() && it->value().is_array()) {
        for (const auto& e : it->value().as_array()) {
            if (!e.is_object()) continue;
            const auto& vo = e.as_object();
            auto vstr = [&vo](const char* key) -> std::string {
                if (const auto v = vo.find(key); v != vo.end() && v->value().is_string())
                    return std::string(v->value().as_string().c_str());
                return {};
            };
            VariableSummary vs;
            vs.name = vstr("name");
            vs.type = vstr("type");
            vs.mutability = vstr("mutability");
            vs.visibility = vstr("visibility");
            r.variables.push_back(std::move(vs));
        }
    }

    return r;
}

boost::json::value to_json(const ClassRecord& rec) {
    namespace json = boost::json;
    json::object o;
    o["name"] = rec.name;
    o["kind"] = rec.kind;
    o["namespace"] = rec.namespace_name;
    o["visibility"] = rec.visibility;
    o["static"] = rec.is_static;
    o["file"] = rec.file;

    json::array inheritance;
    for (const auto& base : rec.inheritance) inheritance.emplace_back(base);
    o["inheritance"] = std::move(inheritance);

    json::array methods;
    for (const auto& m : rec.methods) {
        json::object mo;
        mo["name"] = m.name;
        mo["return_type"] = m.return_type;
        mo["visibility"] = m.visibility;
        mo["static"] = m.is_static;
        // MethodSummary stores only the count; record_from_json() reads the
        // count back as the "parameters" array's size, so emit that many slots.
        json::array params;
        // A default-constructed value is null — one null slot per parameter.
        for (std::size_t i = 0; i < m.param_count; ++i) {
            params.emplace_back(json::value{});
        }
        mo["parameters"] = std::move(params);
        json::array called;
        for (const auto& c : m.called_methods) called.emplace_back(c);
        mo["called_methods"] = std::move(called);
        methods.emplace_back(std::move(mo));
    }
    o["methods"] = std::move(methods);

    json::array variables;
    for (const auto& v : rec.variables) {
        json::object vo;
        vo["name"] = v.name;
        vo["type"] = v.type;
        vo["mutability"] = v.mutability;
        vo["visibility"] = v.visibility;
        variables.emplace_back(std::move(vo));
    }
    o["variables"] = std::move(variables);

    o["x"] = rec.x;
    o["y"] = rec.y;
    o["z"] = rec.z;
    return o;
}

}  // namespace server
