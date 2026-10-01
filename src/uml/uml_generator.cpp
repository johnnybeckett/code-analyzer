#include "uml/uml_generator.h"

#include <fstream>
#include <iostream>

namespace uml {

UmlGenerator::UmlGenerator(const std::vector<std::string>& files,
                           const std::vector<std::string>& hide_patterns)
    : model_(files, hide_patterns) {}

bool UmlGenerator::generate() {
    try {
        // Build the page through the shared UmlModel facade.
        std::string html_content = generateHTML();

        // Write to output file
        std::ofstream output_file("uml_diagram.html");
        if (!output_file.is_open()) {
            std::cerr << "Error: Could not create output file uml_diagram.html\n";
            return false;
        }

        output_file << html_content;
        output_file.close();

        std::cout << "UML diagram generated successfully as uml_diagram.html\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error generating UML diagram: " << e.what() << "\n";
        return false;
    }
}

std::string UmlGenerator::generateHTML() const {
    return model_.build_html();
}

}  // namespace uml
