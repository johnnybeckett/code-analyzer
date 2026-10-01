#ifndef UML_GENERATOR_H
#define UML_GENERATOR_H

#include <string>
#include <vector>

#include "uml/uml_model.h"

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
 *
 * The page itself is built by the shared UmlModel facade (the same one the
 * HTTP server serves); this class only writes it to a file.
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

    UmlModel model_;
};

}  // namespace uml

#endif  // UML_GENERATOR_H
