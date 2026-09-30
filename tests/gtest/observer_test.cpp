#include <gtest/gtest.h>
#include "observers/analysis_observer.h"
#include "observers/console_observer.h"
#include <sstream>
#include <vector>

namespace {

// Records every event it is given, so tests can assert on the delivery.
class RecordingObserver : public AnalysisObserver {
public:
    std::vector<AnalysisEvent> events;
    void update(const AnalysisEvent& event) override {
        events.push_back(event);
    }
};

AnalysisEvent file_parsed_event(const std::string& path) {
    AnalysisEvent e;
    e.kind = AnalysisEvent::Kind::FileParsed;
    e.file = path;
    return e;
}

AnalysisEvent complete_event(std::size_t count) {
    AnalysisEvent e;
    e.kind = AnalysisEvent::Kind::AnalysisComplete;
    e.class_count = count;
    return e;
}

} // namespace

// Events reach every registered observer, in order.
TEST(EventDispatcherTest, NotifiesAllObserversInOrder) {
    EventDispatcher dispatcher;
    auto first = std::make_unique<RecordingObserver>();
    auto second = std::make_unique<RecordingObserver>();
    RecordingObserver* f = first.get();
    RecordingObserver* s = second.get();
    dispatcher.add_observer(std::move(first));
    dispatcher.add_observer(std::move(second));

    dispatcher.notify(file_parsed_event("a.cpp"));
    dispatcher.notify(complete_event(3));

    ASSERT_EQ(f->events.size(), 2u);
    ASSERT_EQ(s->events.size(), 2u);
    EXPECT_EQ(f->events[0].kind, AnalysisEvent::Kind::FileParsed);
    EXPECT_EQ(f->events[0].file, "a.cpp");
    EXPECT_EQ(f->events[1].kind, AnalysisEvent::Kind::AnalysisComplete);
    EXPECT_EQ(f->events[1].class_count, 3u);
    // Both observers got the same sequence
    EXPECT_EQ(s->events[0].file, "a.cpp");
    EXPECT_EQ(s->events[1].class_count, 3u);
}

// A dispatcher with no observers is a harmless no-op.
TEST(EventDispatcherTest, NotifyWithNoObserversIsNoop) {
    EventDispatcher dispatcher;
    dispatcher.notify(file_parsed_event("a.cpp"));  // must not crash
    SUCCEED();
}

// The console observer prints exactly the historic progress line for a
// FileParsed event, and nothing for AnalysisComplete.
TEST(ConsoleObserverTest, PrintsExactlyProcessingFileLine) {
    ConsoleObserver observer;
    std::ostringstream captured;
    auto* real = std::cout.rdbuf();
    std::cout.rdbuf(captured.rdbuf());
    observer.update(file_parsed_event("src/foo.cpp"));
    observer.update(complete_event(7));  // must stay silent
    std::cout.rdbuf(real);

    EXPECT_EQ(captured.str(), "Processing file: src/foo.cpp\n");
}
