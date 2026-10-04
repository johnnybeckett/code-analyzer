#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "core/cmake_model.h"

// Forward declarations
class Class;
class Method;
class Variable;

/**
 * @brief Represents a code element's visibility
 */
enum class Visibility {
    PUBLIC,
    PRIVATE,
    PROTECTED
};

/**
 * @brief Represents a code element's mutability
 */
enum class Mutability {
    READ_WRITE,
    READ_ONLY,
    CONST
};

/**
 * @brief Base class for all code elements
 */
class CodeElement {
public:
    std::string name;
    std::string full_namespace;
    Visibility visibility;
    bool is_static = false;

    CodeElement(const std::string& name, const std::string& full_namespace,
                Visibility visibility = Visibility::PUBLIC);
    virtual ~CodeElement() = default;
};

/**
 * @brief Represents a class in the code
 */
class Class : public CodeElement {
public:
    /**
     * @brief Declaration kind: "class", "struct", or "union"
     *        (defaults to "class")
     */
    std::string kind = "class";
    /**
     * @brief Source file this class was parsed from (empty when unknown).
     *        Populated by the parsers; used by the UML viewer to fetch source.
     */
    std::string file;
    std::vector<std::string> inheritance_list;
    std::vector<std::unique_ptr<Method>> methods;
    std::vector<std::unique_ptr<Variable>> variables;

    Class(const std::string& name, const std::string& full_namespace);

    void add_inheritance(const std::string& base_class);
    void add_method(std::unique_ptr<Method> method);
    void add_variable(std::unique_ptr<Variable> variable);
};

/**
 * @brief Represents a method in the code
 */
class Method : public CodeElement {
public:
    std::vector<std::string> parameters;
    std::string return_type;
    std::vector<std::string> called_methods;
    std::vector<std::string> accessed_variables;

    Method(const std::string& name, const std::string& full_namespace,
           Visibility visibility = Visibility::PUBLIC);
};

/**
 * @brief Represents a variable in the code
 */
class Variable : public CodeElement {
public:
    std::string type;
    Mutability mutability;

    Variable(const std::string& name, const std::string& full_namespace,
             const std::string& type, Mutability mutability = Mutability::READ_WRITE,
             Visibility visibility = Visibility::PUBLIC);
};

/**
 * @brief Main analysis result structure
 */
class AnalysisResult {
public:
    std::vector<std::unique_ptr<Class>> classes;

    /**
     * @brief CMake targets discovered in the analyzed directory (empty for
     *        non-directory inputs, or a directory with no CMake files). The
     *        "CMake" layout renders this as a library dependency graph.
     */
    CMakeGraph cmake;

    /**
     * @brief Renderable non-code files found in the analyzed directory
     *        (Markdown, Graphviz .dot, Draw.io), for the file-viewer to
     *        render instead of showing raw text.
     */
    std::vector<std::string> sources;

    void add_class(std::unique_ptr<Class> class_obj);
};

#endif // MODEL_H