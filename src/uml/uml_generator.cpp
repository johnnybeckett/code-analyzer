#include "uml/uml_generator.h"

#include "uml/class_loader.h"
#include <fstream>
#include <iostream>
#include <utility>

namespace uml {

UmlGenerator::UmlGenerator(const std::vector<std::string>& files,
                           const std::vector<std::string>& hide_patterns)
    : input_files(files), hidden_classes_regex(hide_patterns),
      diff_mode(files.size() >= 2) {}

bool UmlGenerator::generate() {
    try {
        // Generate HTML with 3D visualization
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
    // Splice the real class data into the template.
    //   - single file: one merged class set, diff off
    //   - two files:   old (files[0]) and new (files[1]), diff on
    std::string old_json, new_json;
    const bool diff = diff_mode && input_files.size() >= 2;
    if (diff) {
        old_json = JsonClassLoader::getClassesJSON(JsonClassLoader::parseFileClasses(input_files[0]));
        new_json = JsonClassLoader::getClassesJSON(JsonClassLoader::parseFileClasses(input_files[1]));
    } else {
        std::vector<std::string> all;
        for (const auto& f : input_files) {
            const auto c = JsonClassLoader::parseFileClasses(f);
            all.insert(all.end(), c.begin(), c.end());
        }
        old_json = "[]";
        new_json = JsonClassLoader::getClassesJSON(all);
    }

    return renderer.render({
        { "__DIFF_MODE__",       diff ? "true" : "false" },
        { "__OLD_CLASSES_JSON__", old_json },
        { "__NEW_CLASSES_JSON__", new_json },
    });
}

}  // namespace uml
