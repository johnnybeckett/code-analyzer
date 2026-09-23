#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

#include <string>
#include <memory>

/**
 * @brief Base event dispatcher interface
 */
class EventDispatcher {
public:
    virtual ~EventDispatcher() = default;

    /**
     * @brief Dispatch an event
     * @param event_name Name of the event
     * @param event_data Data associated with the event
     */
    virtual void dispatch(const std::string& event_name, const std::string& event_data) = 0;
};

#endif // EVENT_DISPATCHER_H