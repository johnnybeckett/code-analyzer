#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

/**
 * @brief Simple code analyzer documentation generator
 *
 * This tool generates HTML documentation describing the structure and
 * functionality of the code analyzer system.
 */
class DocumentationGenerator {
public:
    /**
     * @brief Generate comprehensive documentation for the code analyzer
     * @return True if successful, false otherwise
     */
    static bool generateDocumentation() {
        try {
            std::string html_content = generateHTML();

            std::ofstream output_file("code_analyzer_docs.html");
            if (!output_file.is_open()) {
                std::cerr << "Error: Could not create output file code_analyzer_docs.html\n";
                return false;
            }

            output_file << html_content;
            output_file.close();

            std::cout << "Documentation generated successfully as code_analyzer_docs.html\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error generating documentation: " << e.what() << "\n";
            return false;
        }
    }

private:
    /**
     * @brief Generate complete HTML documentation
     * @return Complete HTML content as string
     */
    static std::string generateHTML() {
        std::ostringstream html;

        html << "<!DOCTYPE html>\n"
             << "<html lang=\"en\">\n"
             << "<head>\n"
             << "    <meta charset=\"UTF-8\">\n"
             << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
             << "    <title>Code Analyzer System Documentation</title>\n"
             << "    <style>\n"
             << "        body { font-family: Arial, sans-serif; line-height: 1.6; margin: 20px; background-color: #f5f5f5; color: #333; }\n"
             << "        .container { max-width: 1200px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }\n"
             << "        h1, h2, h3 { color: #2c3e50; }\n"
             << "        code { background-color: #f8f9fa; padding: 2px 4px; border-radius: 4px; font-family: monospace; }\n"
             << "        .class-diagram { background-color: #f8f9fa; padding: 15px; border-radius: 8px; margin: 20px 0; border-left: 4px solid #3498db; }\n"
             << "        table { width: 100%; border-collapse: collapse; margin: 20px 0; }\n"
             << "        th, td { border: 1px solid #ddd; padding: 12px; text-align: left; }\n"
             << "        th { background-color: #3498db; color: white; }\n"
             << "        tr:nth-child(even) { background-color: #f2f2f2; }\n"
             << "        .highlight { background-color: #fff3cd; padding: 10px; border-radius: 5px; margin: 15px 0; }\n"
             << "        pre { background-color: #f8f9fa; padding: 15px; border-radius: 5px; overflow-x: auto; }\n"
             << "    </style>\n"
             << "</head>\n"
             << "<body>\n"
             << "    <div class=\"container\">\n"
             << "        <h1>Code Analyzer System Documentation</h1>\n"
             << "\n"
             << "        <p>This document describes the structure and functionality of the Code Analyzer system, which is designed to parse source code files and generate structured analysis results.</p>\n"
             << "\n"
             << "        <h2>System Overview</h2>\n"
             << "        <p>The Code Analyzer is a multi-language code parsing system that can analyze C++, C#, and Python source code. It provides:</p>\n"
             << "        <ul>\n"
             << "            <li>Language-specific parsers behind a common <code>IParser</code> interface (Strategy)</li>\n"
             << "            <li>A unified model to represent parsed code elements</li>\n"
             << "            <li>Observer-based progress reporting and a Visitor-driven summary</li>\n"
             << "            <li>A configurable pipeline driven by a typed <code>Config</code> struct</li>\n"
             << "            <li>Support for generating UML visualizations</li>\n"
             << "        </ul>\n"
             << "        <p>All shared code lives in the <code>code_analyzer_core</code> static library; the production binary, the test suite, and the UML tool each link against it as thin entry points.</p>\n"
             << "\n"
             << "        <h2>Core Architecture</h2>\n"
             << "\n"
             << "        <div class=\"class-diagram\">\n"
             << "            <h3>Class Structure Overview</h3>\n"
             << "            <p>The system is built around a core model that represents code elements, plus a set of collaborating components:</p>\n"
             << "\n"
             << "            <table>\n"
             << "                <tr>\n"
             << "                    <th>Class</th>\n"
             << "                    <th>Description</th>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>CodeElement</code></td>\n"
             << "                    <td>Base class for all code elements with common properties like name, namespace, and visibility</td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>Class</code></td>\n"
             << "                    <td>Represents a class in the code with inheritance, methods, and variables</td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>Method</code></td>\n"
             << "                    <td>Represents a method/function in the code with parameters, return type, and called methods</td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>Variable</code></td>\n"
             << "                    <td>Represents a variable with type, mutability, and access level</td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>AnalysisResult</code></td>\n"
             << "                    <td>Container for all parsed classes and their relationships</td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>Analyzer</code></td>\n"
             << "                    <td>Coordinates parsing; picks parsers through <code>ParserRegistry</code> and reports progress to observers</td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>IParser</code></td>\n"
             << "                    <td>Common parser strategy interface: <code>parse_file()</code> returning a list of classes</td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>ParserRegistry</code></td>\n"
             << "                    <td>Maps a file extension to the factory that builds its parser (Factory)</td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>Config</code></td>\n"
             << "                    <td>Typed, value-semantic options (source extensions, skipped directories) with JSON <code>load</code>/<code>save</code></td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>EventDispatcher</code> / <code>AnalysisObserver</code></td>\n"
             << "                    <td>Typed <code>AnalysisEvent</code> broadcast to registered observers (Observer), in <code>src/observers/</code></td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>ConsoleObserver</code></td>\n"
             << "                    <td>Observer (in <code>src/observers/console_observer.cpp/h</code>) that prints the per-file progress line to the console</td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>Visitor</code> / <code>SummaryVisitor</code></td>\n"
             << "                    <td>Reads an <code>AnalysisResult</code> and renders the tail summary (Visitor)</td>\n"
             << "                </tr>\n"
             << "                <tr>\n"
             << "                    <td><code>CppParser</code> / <code>CSharpParser</code> / <code>PythonParser</code></td>\n"
             << "                    <td>Language-specific parsers (regex-based), exposed through thin <code>IParser</code> adapters</td>\n"
             << "                </tr>\n"
             << "            </table>\n"
             << "        </div>\n"
             << "\n"
             << "        <h2>Key Components</h2>\n"
             << "\n"
             << "        <h3>Main Entry Point</h3>\n"
             << "        <p>The application's entry point is in <code>src/main.cpp</code>. It stays thin: it parses command-line options, wires the collaborating objects together, and hands off to the core library:</p>\n"
             << "        <ul>\n"
             << "            <li>Parses command line options (input path, <code>--json</code>, <code>--config</code>, compile commands)</li>\n"
             << "            <li>Constructs <code>Config</code>, <code>ParserRegistry</code>, and <code>EventDispatcher</code>, then an <code>Analyzer</code></li>\n"
             << "            <li>Runs the analysis, prints the <code>SummaryVisitor</code> report, and writes JSON via <code>core/json_serializer</code></li>\n"
             << "        </ul>\n"
             << "\n"
             << "        <h3>Analyzer Class</h3>\n"
             << "        <p>The <code>Analyzer</code> class (in <code>src/core/analyzer.h/cpp</code>) is the central component:</p>\n"
             << "        <ul>\n"
             << "            <li><code>analyze_project()</code>: Analyzes a project directory recursively, pruning <code>Config::skip_dirs</code></li>\n"
             << "            <li><code>analyze_file()</code>: Resolves the parser for a file's extension through <code>ParserRegistry</code></li>\n"
             << "            <li><code>analyze_compile_commands()</code>: Analyzes using compile_commands.json for C++ projects</li>\n"
             << "            <li>Emits <code>FileParsed</code> and <code>AnalysisComplete</code> events to its observers</li>\n"
             << "        </ul>\n"
             << "\n"
             << "        <h3>Parsers</h3>\n"
             << "        <p>Language-specific parsers handle different code formats. Each is wrapped in a thin <code>IParser</code> adapter registered in the <code>ParserRegistry</code>:</p>\n"
             << "\n"
             << "        <div class=\"highlight\">\n"
             << "            <strong>C# Parser:</strong>\n"
             << "            <code>src/parser/csharp_parser.cpp/h</code> - Uses regex-based parsing to extract classes, methods, and variables from C# files.\n"
             << "        </div>\n"
             << "\n"
             << "        <div class=\"highlight\">\n"
             << "            <strong>C++ Parser:</strong>\n"
             << "            <code>src/parser/cpp_parser.cpp/h</code> - Parses C++ classes, structs, namespaces, templates, and inheritance, and reads compile_commands.json.\n"
             << "        </div>\n"
             << "\n"
             << "        <div class=\"highlight\">\n"
             << "            <strong>Python Parser:</strong>\n"
             << "            <code>src/parser/python_parser.cpp/h</code> - Extracts classes, methods, and variables from Python files.\n"
             << "        </div>\n"
             << "\n"
             << "        <h2>Core Model Structure</h2>\n"
             << "\n"
             << "        <p>The model (<code>src/core/model.h/cpp</code>) defines how parsed code elements are structured:</p>\n"
             << "\n"
             << "        <table>\n"
             << "            <tr>\n"
             << "                <th>Element</th>\n"
             << "                <th>Properties</th>\n"
             << "                <th>Description</th>\n"
             << "            </tr>\n"
             << "            <tr>\n"
             << "                <td><code>CodeElement</code></td>\n"
             << "                <td>name, full_namespace, visibility, is_static</td>\n"
             << "                <td>Base class with common code element properties</td>\n"
             << "            </tr>\n"
             << "            <tr>\n"
             << "                <td><code>Class</code></td>\n"
             << "                <td>inheritance_list, methods, variables</td>\n"
             << "                <td>Represents a class with inheritance, methods and variables</td>\n"
             << "            </tr>\n"
             << "            <tr>\n"
             << "                <td><code>Method</code></td>\n"
             << "                <td>parameters, return_type, called_methods, accessed_variables</td>\n"
             << "                <td>Represents a method/function with signature information</td>\n"
             << "            </tr>\n"
             << "            <tr>\n"
             << "                <td><code>Variable</code></td>\n"
             << "                <td>type, mutability</td>\n"
             << "                <td>Represents a variable with type and access level</td>\n"
             << "            </tr>\n"
             << "        </table>\n"
             << "\n"
             << "        <h2>Usage Examples</h2>\n"
             << "\n"
             << "        <p>To run the analyzer on a project:</p>\n"
             << "        <pre><code>./CodeAnalyzer /path/to/project</code></pre>\n"
             << "\n"
             << "        <p>To write JSON output to a file:</p>\n"
             << "        <pre><code>./CodeAnalyzer --json /path/to/output.json /path/to/project</code></pre>\n"
             << "\n"
             << "        <p>To override defaults (source extensions, skipped directories) with a config file:</p>\n"
             << "        <pre><code>./CodeAnalyzer --config config.json /path/to/project</code></pre>\n"
             << "\n"
             << "        <p>To analyze with compile_commands.json (for C++ projects):</p>\n"
             << "        <pre><code>./CodeAnalyzer --compile-commands /path/to/compile_commands.json</code></pre>\n"
             << "\n"
             << "        <h2>Test Structure</h2>\n"
             << "\n"
             << "        <p>The system includes comprehensive tests in the <code>tests/gtest</code> directory, all linking the shared <code>code_analyzer_core</code> library:</p>\n"
             << "        <ul>\n"
             << "            <li><code>main_test.cpp</code>: End-to-end tests for the analysis pipeline</li>\n"
             << "            <li><code>csharp_test.cpp</code>: Tests for C# parsing functionality</li>\n"
             << "            <li><code>cpp_kind_test.cpp</code>: Tests for C++ class/struct/union kind detection</li>\n"
             << "            <li><code>registry_test.cpp</code>: Tests for <code>ParserRegistry</code> extension dispatch and adapters</li>\n"
             << "            <li><code>config_test.cpp</code>: Tests for <code>Config</code> load/save round-trips</li>\n"
             << "            <li><code>observer_test.cpp</code>: Tests for the <code>EventDispatcher</code> / observer contract</li>\n"
             << "            <li><code>uml_test.cpp</code>: Tests for the UML template splice (no token re-scan)</li>\n"
             << "        </ul>\n"
             << "\n"
             << "        <h2>UML Generation</h2>\n"
             << "\n"
             << "        <p>The UML generator (<code>src/uml/</code>) visualizes class relationships and is split into single-responsibility parts:</p>\n"
             << "        <ul>\n"
             << "            <li><code>UmlGenerator</code> - orchestrates input files, diff-mode detection, and HTML output</li>\n"
             << "            <li><code>JsonClassLoader</code> - reads analyzer JSON into per-class records</li>\n"
             << "            <li><code>TemplateRenderer</code> - splices class data into the embedded page with a single-pass substitution that never re-scans emitted values</li>\n"
             << "            <li><code>template.h</code> - the self-contained HTML/CSS/JS page (3D diagram, side panel, diff mode)</li>\n"
             << "        </ul>\n"
             << "\n"
             << "        <div class=\"highlight\">\n"
             << "            <strong>Note:</strong> This documentation is generated by the system itself. It describes the current implementation structure and functionality of the code analyzer.\n"
             << "        </div>\n"
             << "    </div>\n"
             << "</body>\n"
             << "</html>";

        return html.str();
    }
};

/**
 * @brief Main entry point for documentation generator
 * @return Exit status
 */
int main() {
    std::cout << "Generating code analyzer documentation...\n";

    if (DocumentationGenerator::generateDocumentation()) {
        std::cout << "Documentation generated successfully!\n";
        return 0;
    } else {
        std::cerr << "Failed to generate documentation.\n";
        return 1;
    }
}
