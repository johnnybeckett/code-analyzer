#include "core/model.h"

// CodeElement implementation
CodeElement::CodeElement(const std::string& name, const std::string& full_namespace,
                         Visibility visibility)
    : name(name), full_namespace(full_namespace), visibility(visibility) {}

// Class implementation
Class::Class(const std::string& name, const std::string& full_namespace)
    : CodeElement(name, full_namespace, Visibility::PUBLIC) {}

void Class::add_inheritance(const std::string& base_class) {
    inheritance_list.push_back(base_class);
}

void Class::add_method(std::unique_ptr<Method> method) {
    methods.push_back(std::move(method));
}

void Class::add_variable(std::unique_ptr<Variable> variable) {
    variables.push_back(std::move(variable));
}

// Method implementation
Method::Method(const std::string& name, const std::string& full_namespace,
               Visibility visibility)
    : CodeElement(name, full_namespace, visibility) {}

// Variable implementation
Variable::Variable(const std::string& name, const std::string& full_namespace,
                   const std::string& type, Mutability mutability,
                   Visibility visibility)
    : CodeElement(name, full_namespace, visibility), type(type), mutability(mutability) {}

// AnalysisResult implementation
void AnalysisResult::add_class(std::unique_ptr<Class> class_obj) {
    classes.push_back(std::move(class_obj));
}