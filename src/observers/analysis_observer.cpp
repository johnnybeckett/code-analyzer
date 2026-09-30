#include "observers/analysis_observer.h"

void EventDispatcher::add_observer(std::unique_ptr<AnalysisObserver> observer) {
    observers_.push_back(std::move(observer));
}

void EventDispatcher::notify(const AnalysisEvent& event) const {
    for (const auto& observer : observers_) {
        observer->update(event);
    }
}
