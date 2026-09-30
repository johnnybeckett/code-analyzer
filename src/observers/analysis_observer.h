#ifndef ANALYSIS_OBSERVER_H
#define ANALYSIS_OBSERVER_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief A single event emitted by the analyzer while it works.
 *
 * A typed struct (rather than a free-form string) so observers can switch on
 * the event kind and read the fields they need.
 */
struct AnalysisEvent {
    /** @brief The kind of event that occurred. */
    enum class Kind {
        FileParsed,      ///< A source file was processed (see `file`).
        AnalysisComplete ///< The whole analysis finished (see `class_count`).
    };

    Kind kind = Kind::FileParsed;
    std::string file;              ///< FileParsed: path of the processed file
    std::size_t class_count = 0;   ///< AnalysisComplete: number of classes found
};

/**
 * @brief Base interface for observers of analysis events (Observer).
 */
class AnalysisObserver {
public:
    virtual ~AnalysisObserver() = default;

    /**
     * @brief React to an event.
     * @param event The event that occurred
     */
    virtual void update(const AnalysisEvent& event) = 0;
};

/**
 * @brief Delivers analysis events to the observers registered with it.
 *
 * Deliberately NOT a singleton: the composition root constructs one and
 * injects it, so observer registrations cannot leak across analyzer
 * instances or across test cases sharing a process.
 */
class EventDispatcher {
public:
    /**
     * @brief Register an observer (the dispatcher takes ownership).
     * @param observer Observer to notify for every subsequent event
     */
    void add_observer(std::unique_ptr<AnalysisObserver> observer);

    /**
     * @brief Deliver the event to every observer, in registration order.
     * @param event The event to deliver
     */
    void notify(const AnalysisEvent& event) const;

private:
    std::vector<std::unique_ptr<AnalysisObserver>> observers_;
};

#endif // ANALYSIS_OBSERVER_H
