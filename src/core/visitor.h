#ifndef VISITOR_H
#define VISITOR_H

#include "model.h"
#include <memory>

/**
 * @brief Base visitor interface for AST traversal
 */
class Visitor {
public:
    virtual ~Visitor() = default;

    virtual void visit(const Class& class_obj) = 0;
    virtual void visit(const Method& method) = 0;
    virtual void visit(const Variable& variable) = 0;
};

/**
 * @brief Analysis visitor that collects code information
 */
class AnalysisVisitor : public Visitor {
public:
    void visit(const Class& class_obj) override;
    void visit(const Method& method) override;
    void visit(const Variable& variable) override;

private:
    AnalysisResult& result_;
};

#endif // VISITOR_H