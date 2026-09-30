#include "observers/console_observer.h"
#include <iostream>

void ConsoleObserver::update(const AnalysisEvent& event) {
    switch (event.kind) {
        case AnalysisEvent::Kind::FileParsed:
            std::cout << "Processing file: " << event.file << std::endl;
            break;
        case AnalysisEvent::Kind::AnalysisComplete:
            // Silent by design: the end-of-run summary belongs to the report
            // visitor (report_visitor.h), so observers only report progress.
            break;
    }
}
