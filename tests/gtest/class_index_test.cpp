#include <gtest/gtest.h>
#include "server/class_index.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using server::ClassIndex;
using server::ClassRecord;

namespace {

ClassRecord make_rec(const std::string& name, const std::string& ns) {
    ClassRecord r;
    r.name = name;
    r.namespace_name = ns;
    return r;
}

// Five classes in two namespaces — a small, hand-checkable layout.
std::vector<ClassRecord> sample_records() {
    return {
        make_rec("Alpha", "A"),
        make_rec("Beta", "A"),
        make_rec("Gamma", "A"),
        make_rec("Delta", "B"),
        make_rec("Epsilon", "B"),
    };
}

const ClassRecord* by_name(const std::vector<ClassRecord>& recs, const std::string& name) {
    for (const auto& r : recs)
        if (r.name == name) return &r;
    return nullptr;
}

double dist(const ClassRecord& a, double x, double y, double z) {
    const double dx = a.x - x, dy = a.y - y, dz = a.z - z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

// ---------------------------------------------------------------------------
// Layout: deterministic, namespace-grouped 3D grid.
// ---------------------------------------------------------------------------

TEST(ClassIndexLayoutTest, PositionsAreDeterministic) {
    ClassIndex a(sample_records());
    ClassIndex b(sample_records());
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a.records()[i].x, b.records()[i].x);
        EXPECT_EQ(a.records()[i].y, b.records()[i].y);
        EXPECT_EQ(a.records()[i].z, b.records()[i].z);
        EXPECT_EQ(a.records()[i].name, b.records()[i].name);
    }
}

TEST(ClassIndexLayoutTest, NamespacesGroupAlongX) {
    ClassIndex idx(sample_records());
    std::vector<ClassRecord> recs(idx.records().begin(), idx.records().end());

    const ClassRecord* alpha = by_name(recs, "Alpha");
    const ClassRecord* delta = by_name(recs, "Delta");
    ASSERT_NE(alpha, nullptr);
    ASSERT_NE(delta, nullptr);
    // Group "B" is tiled to the right of group "A" (positive gap along +X).
    EXPECT_GT(delta->x, alpha->x);
}

TEST(ClassIndexLayoutTest, BoundsMatchExtents) {
    ClassIndex idx(sample_records());
    const auto b = idx.bounds();
    // Recompute from the assigned positions.
    std::vector<ClassRecord> recs(idx.records().begin(), idx.records().end());
    double minx = recs[0].x, maxx = recs[0].x, miny = recs[0].y, maxy = recs[0].y;
    double minz = recs[0].z, maxz = recs[0].z;
    for (const auto& r : recs) {
        minx = std::min(minx, r.x); maxx = std::max(maxx, r.x);
        miny = std::min(miny, r.y); maxy = std::max(maxy, r.y);
        minz = std::min(minz, r.z); maxz = std::max(maxz, r.z);
    }
    EXPECT_DOUBLE_EQ(b.min_x, minx);
    EXPECT_DOUBLE_EQ(b.max_x, maxx);
    EXPECT_DOUBLE_EQ(b.min_y, miny);
    EXPECT_DOUBLE_EQ(b.max_y, maxy);
    EXPECT_DOUBLE_EQ(b.min_z, minz);
    EXPECT_DOUBLE_EQ(b.max_z, maxz);
    EXPECT_DOUBLE_EQ(idx.cellSize(), 1.0);
}

// ---------------------------------------------------------------------------
// nearest(): exactly the N closest, ascending by distance, correct vs brute force.
// ---------------------------------------------------------------------------

TEST(ClassIndexNearestTest, ReturnsNClosestInAscendingOrder) {
    ClassIndex idx(sample_records());
    std::vector<ClassRecord> recs(idx.records().begin(), idx.records().end());

    const double x = 0.1, y = 0.0, z = 0.0;
    for (std::size_t k = 1; k <= recs.size(); ++k) {
        auto near = idx.nearest(k, x, y, z);
        ASSERT_EQ(near.size(), k);

        // Ascending by distance.
        for (std::size_t i = 1; i < near.size(); ++i)
            EXPECT_LE(dist(near[i - 1], x, y, z), dist(near[i], x, y, z) + 1e-9);

        // Must equal the brute-force k closest (as a set of names).
        std::vector<ClassRecord> all(recs);
        std::sort(all.begin(), all.end(),
                  [&](const ClassRecord& a, const ClassRecord& b) {
                      const double da = dist(a, x, y, z), db = dist(b, x, y, z);
                      if (da != db) return da < db;
                      return a.name < b.name;
                  });
        for (std::size_t i = 0; i < k; ++i)
            EXPECT_EQ(near[i].name, all[i].name) << "k=" << k << " i=" << i;
    }
}

TEST(ClassIndexNearestTest, IncludesOwnCell) {
    ClassIndex idx(sample_records());
    // Query sits in the cell that holds the closest record.
    auto near = idx.nearest(1, 0.1, 0.0, 0.0);
    ASSERT_EQ(near.size(), 1u);
    EXPECT_EQ(near[0].name, "Alpha");
    EXPECT_NEAR(near[0].x, 0.0, 1e-9);
    EXPECT_NEAR(near[0].y, 0.0, 1e-9);
}

TEST(ClassIndexNearestTest, CountClampedToSize) {
    ClassIndex idx(sample_records());
    auto near = idx.nearest(1000, 0.0, 0.0, 0.0);
    EXPECT_EQ(near.size(), idx.size());
}

TEST(ClassIndexNearestTest, ZeroCountIsEmpty) {
    ClassIndex idx(sample_records());
    EXPECT_TRUE(idx.nearest(0, 0.0, 0.0, 0.0).empty());
}

// ---------------------------------------------------------------------------
// sample(): deterministic downsample, within bounds, clamped.
// ---------------------------------------------------------------------------

TEST(ClassIndexSampleTest, AtMostKAndWithinBounds) {
    ClassIndex idx(sample_records());
    const auto b = idx.bounds();
    for (std::size_t k : {std::size_t{1}, std::size_t{3}, std::size_t{5}, std::size_t{50}}) {
        auto s = idx.sample(k);
        ASSERT_LE(s.size(), k);
        ASSERT_LE(s.size(), idx.size());
        for (const auto& r : s) {
            EXPECT_GE(r.x, b.min_x - 1e-9);
            EXPECT_LE(r.x, b.max_x + 1e-9);
            EXPECT_GE(r.y, b.min_y - 1e-9);
            EXPECT_LE(r.y, b.max_y + 1e-9);
        }
    }
}

TEST(ClassIndexSampleTest, SpansFirstAndLast) {
    ClassIndex idx(sample_records());
    std::vector<ClassRecord> recs(idx.records().begin(), idx.records().end());
    auto s = idx.sample(2);
    ASSERT_EQ(s.size(), 2u);
    // Two evenly-spaced picks span the whole layout order.
    EXPECT_EQ(s[0].name, recs.front().name);
    EXPECT_EQ(s[1].name, recs.back().name);
}

// ---------------------------------------------------------------------------
// record_from_json(): 1:1 with the analyzer's serialized class object.
// ---------------------------------------------------------------------------

TEST(ClassIndexJsonTest, ParsesFullRecord) {
    const std::string json = R"({
      "name": "Calc",
      "kind": "class",
      "namespace": "App::Math",
      "visibility": "public",
      "static": false,
      "file": "/tmp/proj/calc.cpp",
      "inheritance": ["Base", "Util"],
      "methods": [
        {"name": "add", "return_type": "int", "visibility": "public", "static": false,
         "parameters": ["a", "b"], "called_methods": ["helper", "log"]}
      ],
      "variables": [
        {"name": "n", "type": "int", "mutability": "read_write", "visibility": "private"}
      ]
    })";

    auto r = ClassIndex::record_from_json(json);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->name, "Calc");
    EXPECT_EQ(r->kind, "class");
    EXPECT_EQ(r->namespace_name, "App::Math");
    EXPECT_EQ(r->visibility, "public");
    EXPECT_FALSE(r->is_static);
    EXPECT_EQ(r->file, "/tmp/proj/calc.cpp");
    ASSERT_EQ(r->inheritance.size(), 2u);
    EXPECT_EQ(r->inheritance[0], "Base");
    EXPECT_EQ(r->inheritance[1], "Util");

    ASSERT_EQ(r->methods.size(), 1u);
    EXPECT_EQ(r->methods[0].name, "add");
    EXPECT_EQ(r->methods[0].return_type, "int");
    EXPECT_EQ(r->methods[0].param_count, 2u);
    ASSERT_EQ(r->methods[0].called_methods.size(), 2u);
    EXPECT_EQ(r->methods[0].called_methods[0], "helper");

    ASSERT_EQ(r->variables.size(), 1u);
    EXPECT_EQ(r->variables[0].name, "n");
    EXPECT_EQ(r->variables[0].mutability, "read_write");
}

TEST(ClassIndexJsonTest, RejectsGarbage) {
    EXPECT_FALSE(ClassIndex::record_from_json("not json").has_value());
    EXPECT_FALSE(ClassIndex::record_from_json("[1,2,3]").has_value());
}

// ---------------------------------------------------------------------------
// Empty index: clean edges.
// ---------------------------------------------------------------------------

TEST(ClassIndexEmptyTest, CleanEdges) {
    ClassIndex idx(std::vector<ClassRecord>{});
    EXPECT_EQ(idx.size(), 0u);
    EXPECT_TRUE(idx.bounds().empty());
    EXPECT_TRUE(idx.records().empty());
    EXPECT_TRUE(idx.nearest(5, 1.0, 2.0, 3.0).empty());
    EXPECT_TRUE(idx.sample(3).empty());
}
