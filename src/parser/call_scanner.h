#ifndef CALL_SCANNER_H
#define CALL_SCANNER_H

#include <string>
#include <unordered_set>
#include <vector>

/**
 * @brief Shared, language-agnostic scanner that extracts the callee names
 *        referenced inside a method body.
 *
 * A "call" is any `(` whose immediately-preceding token (ignoring whitespace)
 * is an identifier. That single rule captures every form the call graph cares
 * about without a per-language grammar:
 *   - bare calls            `foo(`
 *   - member calls          `obj.foo(`   `obj->foo(`
 *   - qualified calls       `A::foo(`    `A::B::foo(`
 *
 * Non-call parentheses are rejected for free: a grouping paren (`(a + b)`),
 * a C cast (`(int)x`), or a template cast (`static_cast<int>(x)`) has a
 * non-identifier immediately before its `(`, so no callee is produced. Keyword
 * call forms (`if (`, `for (`, `sizeof (`) do yield an identifier callee, and
 * those are removed by the per-language `blacklist` each parser supplies.
 *
 * The result is deduplicated while preserving first-seen order, so the
 * serialized `called_methods` array and the client-side call-graph are stable
 * run to run.
 *
 * Pure and dependency-free (no I/O, no language state): each parser feeds it
 * the method body it captured plus its own keyword blacklist, keeping the
 * per-language specifics in the parser and the scanning logic in one place
 * (DRY / single responsibility).
 */
class CallScanner {
public:
    /**
     * @brief Extract the callee names referenced in a method body.
     * @param body Method body text (the enclosing braces excluded).
     * @param blacklist Callee names to drop (control-flow / cast keywords).
     * @return Unique callee names in first-seen order; empty if none survive.
     */
    [[nodiscard]]
    static std::vector<std::string> extract(
        const std::string& body,
        const std::unordered_set<std::string>& blacklist);
};

#endif // CALL_SCANNER_H
