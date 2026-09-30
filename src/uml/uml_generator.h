#ifndef UML_GENERATOR_H
#define UML_GENERATOR_H

#include <string>
#include <vector>
#include "uml/template_renderer.h"

namespace uml {

/**
 * @brief UML Class Diagram Generator
 *
 * This tool converts JSON output from the code analyzer into a self-contained
 * documentation page that supports:
 * - A 3D UML class diagram: class boxes with name/attribute/method
 *   compartments and generalization arrows between related classes
 * - Correct UML notation: visibility glyphs (+ - #), underlined static
 *   members, italic virtual members, italic abstract class names
 * - 3D rotation, zooming, and panning of the class diagram
 * - Filtering classes by name or regex pattern
 * - A side panel listing every class with its methods and member variables
 */
class UmlGenerator {
public:
    /**
     * @brief Constructor
     * @param files List of JSON input files to process
     * @param hide_patterns Regex patterns for classes to hide
     *
     * One file  -> a single combined diagram.
     * Two files -> diff mode: files[0] is the older baseline, files[1] the
     *              newer state (added members in green, removed in red).
     */
    UmlGenerator(const std::vector<std::string>& files,
                 const std::vector<std::string>& hide_patterns);

    /**
     * @brief Process all input files and generate HTML output
     * @return True if successful, false otherwise
     */
    bool generate();

private:
    /**
     * @brief Generate the self-contained documentation page
     * @return Complete HTML content with the class data embedded
     */
    std::string generateHTML() const;

    std::vector<std::string> input_files;
    std::vector<std::string> hidden_classes_regex;
    // Diff mode: when two files are given (older.json newer.json) the first
    // is the baseline and the second the new state; the diagram highlights
    // what was added (green) and removed (red).
    bool diff_mode;

    TemplateRenderer renderer;
};

}  // namespace uml

#endif  // UML_GENERATOR_H
