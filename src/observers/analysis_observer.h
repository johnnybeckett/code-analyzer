#ifndef ANALYSIS_OBSERVER_H
#define ANALYSIS_OBSERVER_H

#include <string>
#include <memory>
#include <vector>

/**
 * @brief Base observer interface for analysis events
 */
class AnalysisObserver {
public:
    virtual ~AnalysisObserver() = default;

    /**
     * @brief Notify observer of analysis event
     * @param event Description of the event
     */
    virtual void update(const std::string& event) = 0;
};

/**
 * @brief Analysis event dispatcher using observer pattern
 */
class EventDispatcher {
public:
    static EventDispatcher& getInstance();

    /**
     * @brief Register an observer
     * @param observer Observer to register
     */
    void addObserver(std::unique_ptr<AnalysisObserver> observer);

    /**
     * @brief Notify all observers of an event
     * @param event Description of the event
     */
    void notify(const std::string& event);

private:
    EventDispatcher() = default;
    std::vector<std::unique_ptr<AnalysisObserver>> observers_;
};

#endif // ANALYSIS_OBSERVER_H