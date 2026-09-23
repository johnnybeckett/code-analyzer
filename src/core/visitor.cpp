#include "visitor.h"
#include <iostream>

void AnalysisVisitor::visit(const Class& class_obj) {
    // Collect class information
    std::cout << "Visiting class: " << class_obj.name << std::endl;
    // In a real implementation, this would add the class to result_
}

void AnalysisVisitor::visit(const Method& method) {
    // Collect method information
    std::cout << "Visiting method: " << method.name << std::endl;
    // In a real implementation, this would process method calls and variables
}

void AnalysisVisitor::visit(const Variable& variable) {
    // Collect variable information
    std::cout << "Visiting variable: " << variable.name << std::endl;
    // In a real implementation, this would track access patterns
}