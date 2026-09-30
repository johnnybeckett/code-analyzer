#ifndef CONSOLE_OBSERVER_H
#define CONSOLE_OBSERVER_H

#include "observers/analysis_observer.h"

/**
 * @brief Observer that reports analysis progress on the console.
 *
 * `FileParsed` is printed as the `Processing file: <path>` progress line the
 * analyzer has always shown. `AnalysisComplete` is intentionally silent: the
 * final summary is rendered by the report visitor, not by an observer.
 */
class ConsoleObserver : public AnalysisObserver {
public:
    void update(const AnalysisEvent& event) override;
};

#endif // CONSOLE_OBSERVER_H
