#include "analysis_observer.h"
#include <iostream>

EventDispatcher& EventDispatcher::getInstance() {
    static EventDispatcher instance;
    return instance;
}

void EventDispatcher::addObserver(std::unique_ptr<AnalysisObserver> observer) {
    observers_.push_back(std::move(observer));
}

void EventDispatcher::notify(const std::string& event) {
    for (auto& observer : observers_) {
        observer->update(event);
    }
}