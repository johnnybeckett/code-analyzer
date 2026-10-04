#include <gtest/gtest.h>

#include "core/analysis_reactor.h"
#include "core/iparser.h"
#include "core/parser_registry.h"
#include "core/thread_pool.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

/** @brief A parser whose parse_file always throws — the reactor's error seam. */
class ThrowingParser : public IParser {
public:
    std::vector<std::unique_ptr<Class>> parse_file(const std::string&) override {
        throw std::runtime_error("deliberate parse failure (test fixture)");
    }
};

void write_file(const fs::path& path, const std::string& body) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    out << body;
}

}  // namespace

// ---------------------------------------------------------------------------
// ThreadPool (white-box)
// ---------------------------------------------------------------------------

TEST(ThreadPoolTest, WorkerCountZeroMeansOne) {
    EXPECT_EQ(ThreadPool(0).worker_count(), 1u);
    EXPECT_EQ(ThreadPool(1).worker_count(), 1u);
    EXPECT_EQ(ThreadPool(8).worker_count(), 8u);
}

TEST(ThreadPoolTest, AllSubmittedTasksComplete) {
    ThreadPool pool(4);
    constexpr int kTasks = 1000;

    std::atomic<int> done{0};
    std::vector<std::future<void>> futures;
    futures.reserve(kTasks);
    for (int i = 0; i < kTasks; ++i) {
        futures.push_back(pool.submit([&done] { done.fetch_add(1, std::memory_order_relaxed); }));
    }

    for (auto& f : futures) {
        f.get();  // no exception expected
    }
    EXPECT_EQ(done.load(), kTasks);

    pool.stop();  // must join cleanly
}

TEST(ThreadPoolTest, ThrowingTaskIsIsolated) {
    ThreadPool pool(3);
    std::atomic<int> ok_tasks{0};

    // One bad task among good ones: the pool must survive it.
    auto bad = pool.submit([] { throw std::logic_error("boom"); });
    std::vector<std::future<void>> good;
    good.reserve(50);
    for (int i = 0; i < 50; ++i) {
        good.push_back(pool.submit([&ok_tasks] { ok_tasks.fetch_add(1); }));
    }

    // Wait for every good task, then confirm they all ran despite the bad one.
    for (auto& f : good) {
        f.get();
    }
    EXPECT_EQ(ok_tasks.load(), 50);

    // The exception is delivered to the bad task's own future, and only there.
    EXPECT_THROW(bad.get(), std::logic_error);

    pool.stop();
}

TEST(ThreadPoolTest, SubmitAfterStopThrows) {
    ThreadPool pool(2);
    pool.stop();
    EXPECT_THROW(pool.submit([] {}), std::runtime_error);
    // And again (idempotent stop, still refused).
    pool.stop();
    EXPECT_THROW(pool.submit([] {}), std::runtime_error);
}

TEST(ThreadPoolTest, DestructorJoinsWithoutExplicitStop) {
    // The destructor is the other shutdown path: it must drain and join.
    {
        ThreadPool pool(4);
        std::atomic<int> done{0};
        for (int i = 0; i < 100; ++i) {
            pool.submit([&done] { done.fetch_add(1); });
        }
        // scope ends here: ~ThreadPool() drains the queue and joins workers
    }
    SUCCEED();  // reaching here without a hang is the assertion
}

// ---------------------------------------------------------------------------
// AnalysisReactor (white-box): parallel parse, single-writer in-order fold
// ---------------------------------------------------------------------------

struct Folded {
    std::size_t index;
    std::string file;
    bool ok;
    std::vector<std::string> class_names;
    bool had_error;
};

TEST(AnalysisReactorTest, BuilderSeesOutcomesInInputOrder) {
    // Files mix: parseable, throwing, and unregistered extensions. A throwing
    // parse must yield ok=false for its own slot without aborting the batch,
    // and the builder must see every outcome in the original input order.
    auto root = fs::temp_directory_path() / "thread_pool_test";
    fs::remove_all(root);

    const fs::path a = root / "a.cpp";
    write_file(a, "class Alpha {};\n");
    const fs::path bad = root / "bad.txt";  // ".txt" is unregistered: ok empty
    write_file(bad, "not code");

    ParserRegistry registry = ParserRegistry::standard();
    registry.register_parser(".boom", [] { return std::make_unique<ThrowingParser>(); });

    // files[1] and files[3] use the ".boom" extension whose parser always
    // throws — exercising the ok=false outcome path; files[2] uses an
    // unregistered extension (a successful empty outcome, not an error).
    const std::vector<std::string> files{
        std::string(a.string()),
        std::string(root / "a.boom"),
        std::string(bad.string()),
        std::string(root / "missing.boom"),
    };

    // Builder: record each fold in the order it arrived.
    std::vector<std::size_t> order;
    std::vector<Folded> folded;
    AnalysisReactor reactor(registry, 4, [&](ParseOutcome o) {
        order.push_back(o.index);
        Folded f;
        f.index = o.index;
        f.file = o.file;
        f.ok = o.ok;
        f.had_error = (o.error != nullptr);
        for (const auto& c : o.classes) {
            f.class_names.push_back(c->name);
        }
        folded.push_back(std::move(f));
    });
    reactor.run(files);

    // Every file produced exactly one outcome, in input order 0,1,2,3.
    ASSERT_EQ(order.size(), 4u);
    for (std::size_t i = 0; i < order.size(); ++i) {
        EXPECT_EQ(order[i], i);
    }
    ASSERT_EQ(folded.size(), 4u);

    EXPECT_TRUE(folded[0].ok);             // a.cpp parsed
    ASSERT_EQ(folded[0].class_names.size(), 1u);
    EXPECT_EQ(folded[0].class_names[0], "Alpha");

    EXPECT_FALSE(folded[1].ok);            // ".boom" parser threw
    EXPECT_TRUE(folded[1].had_error);
    EXPECT_TRUE(folded[1].class_names.empty());

    EXPECT_TRUE(folded[2].ok);             // unregistered extension: empty, not an error
    EXPECT_TRUE(folded[2].class_names.empty());
    EXPECT_FALSE(folded[2].had_error);

    EXPECT_FALSE(folded[3].ok);            // missing.boom also threw
    EXPECT_TRUE(folded[3].had_error);

    fs::remove_all(root);
}

TEST(AnalysisReactorTest, ZeroWorkersStillRuns) {
    auto root = fs::temp_directory_path() / "thread_pool_test_zero";
    fs::remove_all(root);
    const fs::path a = root / "a.cpp";
    write_file(a, "class Solo {};\n");

    ParserRegistry registry = ParserRegistry::standard();

    std::vector<std::string> class_names;
    AnalysisReactor reactor(registry, 0,
                            [&](ParseOutcome o) {
                                for (const auto& c : o.classes) {
                                    class_names.push_back(c->name);
                                }
                            });
    reactor.run({std::string(a.string())});

    ASSERT_EQ(class_names.size(), 1u);
    EXPECT_EQ(class_names[0], "Solo");

    fs::remove_all(root);
}

TEST(AnalysisReactorTest, EmptyFileListNoOutcomes) {
    ParserRegistry registry = ParserRegistry::standard();
    int folds = 0;
    AnalysisReactor reactor(registry, 4, [&](ParseOutcome) { ++folds; });
    reactor.run({});
    EXPECT_EQ(folds, 0);
}

// ---------------------------------------------------------------------------
// Parity / determinism (black-box): the parallel pipeline must produce the
// same class set as a serial one over the same tree.
// ---------------------------------------------------------------------------

TEST(AnalysisReactorParityTest, ParallelReactorMatchesSerialClassSet) {
    // Same tree, same registry: fold the parallel reactor's outcomes and a
    // strictly-serial fold of the same files; the resulting class sets must be
    // identical. (Ordering may legitimately differ across runs, so compare
    // sorted.)
    const std::string root = std::string(SOURCE_DIR) + "/test_files/cmake";
    ParserRegistry registry = ParserRegistry::standard();

    // Gather the source files the same way a directory provider would.
    std::vector<std::string> files;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied);
    fs::recursive_directory_iterator end;
    for (std::error_code ec; it != end && !ec; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file()) continue;
        const std::string ext = it->path().extension().string();
        if (registry.has_parser(ext)) {
            files.push_back(it->path().string());
        }
    }
    ASSERT_FALSE(files.empty());

    auto collect = [&registry](const std::vector<std::string>& files) {
        std::vector<std::string> names;
        for (const std::string& f : files) {
            if (auto p = registry.create(fs::path(f).extension().string())) {
                for (auto& c : p->parse_file(f)) {
                    names.push_back(c->name);
                }
            }
        }
        std::sort(names.begin(), names.end());
        return names;
    };

    std::vector<std::string> parallel_names;
    AnalysisReactor reactor(registry, 8, [&](ParseOutcome o) {
        for (const auto& c : o.classes) {
            parallel_names.push_back(c->name);
        }
    });
    reactor.run(files);
    std::sort(parallel_names.begin(), parallel_names.end());

    EXPECT_EQ(parallel_names, collect(files));
}
