#include "core/report_visitor.h"
#include <iostream>

void SummaryVisitor::render(const AnalysisResult& result) {
    std::cout << "Analysis complete.\n";
    std::cout << "Classes found: " << result.classes.size() << "\n";

    if (result.classes.empty()) {
        std::cout << "No classes were found in the analyzed files.\n";
        return;
    }

    for (const auto& class_obj : result.classes) {
        visit(*class_obj);
    }
}

void SummaryVisitor::visit(const Class& cls) {
    std::cout << "Found class: " << cls.name << "\n";
}
