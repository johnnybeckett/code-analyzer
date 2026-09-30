#ifndef REPORT_VISITOR_H
#define REPORT_VISITOR_H

#include "core/model.h"

/**
 * @brief A visitor over analysis-model elements (Visitor).
 *
 * The model classes (Class/Method/Variable) stay plain data — they implement
 * no accept(), so the visitor is *told* what to look at instead of being
 * called from the model. render() drives the walk over an AnalysisResult.
 */
class Visitor {
public:
    virtual ~Visitor() = default;

    virtual void visit(const Class& cls) = 0;
    virtual void visit(const Method& method) = 0;
    virtual void visit(const Variable& variable) = 0;
};

/**
 * @brief Renders the end-of-run console summary (Visitor).
 *
 * Produces exactly the tail the CLI used to print inline:
 *   Analysis complete.
 *   Classes found: <N>
 *   No classes were found in the analyzed files.   (when N is 0)
 *   Found class: <name>                            (one per class, otherwise)
 */
class SummaryVisitor : public Visitor {
public:
    /**
     * @brief Print the full summary for an analysis result.
     * @param result The analysis result to summarize
     */
    void render(const AnalysisResult& result);

    void visit(const Class& cls) override;
    void visit(const Method&) override {}
    void visit(const Variable&) override {}
};

#endif // REPORT_VISITOR_H
