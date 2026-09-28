#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include <json/json.h> // Assuming we have JSON library for parsing

/**
 * @brief UML Class Diagram Generator
 *
 * This tool converts JSON output from the code analyzer into a self-contained
 * documentation page that supports:
 * - 3D rotation, zooming, and panning of the class diagram
 * - Filtering classes by name or regex pattern
 * - A side panel listing every class with its methods and member variables
 */
class UMLGenerator {
private:
    std::vector<std::string> input_files;
    std::vector<std::string> hidden_classes_regex;

public:
    /**
     * @brief Constructor
     * @param files List of JSON input files to process
     * @param hide_patterns Regex patterns for classes to hide
     */
    UMLGenerator(const std::vector<std::string>& files,
                 const std::vector<std::string>& hide_patterns)
        : input_files(files), hidden_classes_regex(hide_patterns) {}

    /**
     * @brief Process all input files and generate HTML output
     * @return True if successful, false otherwise
     */
    bool generate() {
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

private:
    /**
     * @brief Generate the self-contained documentation page
     * @return Complete HTML content with the class data embedded
     */
    std::string generateHTML() {
        // The class data is spliced into the template below; a raw string keeps
        // the JavaScript/HTML free of C++ escaping (it only ends at )HTMLDOC").
        std::string html = R"HTMLDOC(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Class Documentation</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
    <style>
        * { box-sizing: border-box; }
        body {
            margin: 0;
            background: #0f172a;
            color: #e2e8f0;
            font-family: Arial, Helvetica, sans-serif;
        }
        #scene {
            position: absolute;
            top: 0; left: 0; bottom: 0;
            right: 380px;
        }
        #sidebar {
            position: absolute;
            top: 0; right: 0; bottom: 0;
            width: 380px;
            overflow-y: auto;
            background: #1e293b;
            border-left: 1px solid #334155;
            padding: 16px;
        }
        #sidebar h2 { margin: 0 0 12px; font-size: 16px; }
        #controls {
            position: absolute;
            top: 12px; left: 12px;
            background: rgba(15, 23, 42, 0.88);
            border: 1px solid #334155;
            padding: 12px;
            border-radius: 8px;
            z-index: 10;
        }
        #controls h3 { margin: 0 0 4px; font-size: 14px; }
        #controls input {
            width: 230px;
            padding: 6px 8px;
            margin: 6px 0;
            border-radius: 6px;
            border: 1px solid #475569;
            background: #0f172a;
            color: #e2e8f0;
        }
        #controls button {
            padding: 6px 12px;
            margin-right: 6px;
            border: 0;
            border-radius: 6px;
            background: #4285F4;
            color: #fff;
            cursor: pointer;
        }
        #controls button.secondary { background: #475569; }
        #stats { margin-top: 8px; font-size: 12px; color: #94a3b8; }
        .classCard {
            background: #0f172a;
            border: 1px solid #334155;
            border-radius: 8px;
            padding: 12px;
            margin-bottom: 12px;
        }
        .classCard h3 { margin: 0 0 4px; font-size: 15px; color: #93c5fd; }
        .classCard .bases { font-size: 12px; color: #94a3b8; margin-bottom: 6px; }
        .classCard h4 {
            margin: 10px 0 4px;
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: #64748b;
        }
        .classCard ul {
            margin: 0;
            padding-left: 18px;
            font-family: "SF Mono", Consolas, monospace;
            font-size: 12px;
        }
        .classCard li { margin: 3px 0; }
        .classCard .ret { color: #7dd3fc; }
        .classCard .nm { color: #fbbf24; }
        .classCard .params { color: #94a3b8; }
        .badge {
            display: inline-block;
            font-size: 10px;
            padding: 1px 6px;
            border-radius: 8px;
            margin-left: 6px;
            vertical-align: middle;
        }
        .badge.static { background: #7c3aed; color: #fff; }
        .badge.vis { background: #334155; color: #cbd5e1; }
        .none { color: #64748b; font-size: 12px; }
    </style>
</head>
<body>
    <div id="scene"></div>
    <div id="controls">
        <h3>Filter classes</h3>
        <input type="text" id="filterInput" placeholder="regex, e.g. ^Parser|Generator$"
               onkeydown="if (event.key === 'Enter') applyFilter()">
        <button onclick="applyFilter()">Apply Filter</button>
        <button class="secondary" onclick="resetFilter()">Reset</button>
        <div id="stats"></div>
    </div>
    <div id="sidebar"><h2>Classes</h2></div>

    <script>
        // Class data produced by the code analyzer
        const classes = __CLASSES_JSON__;

        const esc = s => String(s).replace(/[&<>"]/g,
            ch => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[ch]));

        function signature(m) {
            const ret = (m.return_type || '').trim();
            const params = (m.parameters || []).map(p => p.trim()).filter(Boolean).join(', ');
            let html = '<span class="ret">' + esc(ret) + '</span> <span class="nm">'
                + esc(m.name) + '</span><span class="params">(' + esc(params) + ')</span>';
            if (m['static']) html += '<span class="badge static">static</span>';
            return html;
        }

        function buildSidebar() {
            const sidebar = document.getElementById('sidebar');
            classes.forEach(c => {
                const card = document.createElement('div');
                card.className = 'classCard';
                card.dataset.name = c.name;

                let html = '<h3>' + esc(c.name) + '</h3>';
                const bases = c.inheritance || [];
                if (bases.length) html += '<div class="bases">extends ' + bases.map(esc).join(', ') + '</div>';

                const methods = c.methods || [];
                const variables = c.variables || [];
                if (methods.length) {
                    html += '<h4>Methods (' + methods.length + ')</h4><ul>'
                        + methods.map(m => '<li>' + signature(m) + '</li>').join('')
                        + '</ul>';
                }
                if (variables.length) {
                    html += '<h4>Members (' + variables.length + ')</h4><ul>'
                        + variables.map(v =>
                            '<li><span class="ret">' + esc((v.type || '').trim())
                            + '</span> <span class="nm">' + esc(v.name) + '</span></li>'
                        ).join('')
                        + '</ul>';
                }
                if (!methods.length && !variables.length) {
                    html += '<div class="none">no members detected</div>';
                }
                card.innerHTML = html;
                sidebar.appendChild(card);
            });
            updateStats();
        }

        function updateStats() {
            const shown = document.querySelectorAll('.classCard').length
                - document.querySelectorAll('.classCard[style*="none"]').length;
            document.getElementById('stats').textContent
                = shown + ' of ' + classes.length + ' classes shown';
        }

        // --- 3D scene: one labelled block per class ---
        const sceneObjects = [];

        function makeLabel(text) {
            const pad = 14;
            const canvas = document.createElement('canvas');
            let ctx = canvas.getContext('2d');
            const font = '32px Arial';
            ctx.font = font;
            const width = Math.ceil(ctx.measureText(text).width) + pad * 2;
            const height = 52;
            canvas.width = width;
            canvas.height = height;
            ctx = canvas.getContext('2d');
            ctx.font = font;
            ctx.fillStyle = 'rgba(15, 23, 42, 0.92)';
            ctx.fillRect(0, 0, width, height);
            ctx.strokeStyle = '#4285F4';
            ctx.lineWidth = 2;
            ctx.strokeRect(1, 1, width - 2, height - 2);
            ctx.fillStyle = '#ffffff';
            ctx.textBaseline = 'middle';
            ctx.fillText(text, pad, height / 2 + 1);
            const texture = new THREE.CanvasTexture(canvas);
            const sprite = new THREE.Sprite(new THREE.SpriteMaterial({ map: texture, depthTest: false }));
            sprite.scale.set(width / 50, height / 50, 1);
            return sprite;
        }

        function initScene() {
            const container = document.getElementById('scene');
            const scene = new THREE.Scene();
            scene.background = new THREE.Color(0x0f172a);
            const camera = new THREE.PerspectiveCamera(
                60, container.clientWidth / container.clientHeight, 0.1, 1000);
            const renderer = new THREE.WebGLRenderer({ antialias: true });
            renderer.setSize(container.clientWidth, container.clientHeight);
            container.appendChild(renderer.domElement);

            scene.add(new THREE.AmbientLight(0xffffff, 0.7));
            const dir = new THREE.DirectionalLight(0xffffff, 0.7);
            dir.position.set(1, 2, 2);
            scene.add(dir);

            classes.forEach((c, i) => {
                const cube = new THREE.Mesh(
                    new THREE.BoxGeometry(2, 1.2, 1.2),
                    new THREE.MeshPhongMaterial({ color: 0x4285F4 }));
                cube.position.set((i % 4) * 3.2 - 4.8, Math.floor(i / 4) * 2.2 - 2, 0);
                const label = makeLabel(c.name);
                label.position.copy(cube.position);
                label.position.y += 1.1;
                scene.add(cube);
                scene.add(label);
                sceneObjects.push({ cube, label, name: c.name });
            });

            camera.position.set(0, 0, 14);

            // Drag to rotate, wheel to zoom
            let dragging = false, lastX = 0;
            container.addEventListener('mousedown', e => { dragging = true; lastX = e.clientX; });
            window.addEventListener('mouseup', () => { dragging = false; });
            window.addEventListener('mousemove', e => {
                if (dragging) { scene.rotation.y += (e.clientX - lastX) * 0.005; lastX = e.clientX; }
            });
            container.addEventListener('wheel', e => {
                e.preventDefault();
                camera.position.z = Math.min(40, Math.max(4, camera.position.z + e.deltaY * 0.01));
            }, { passive: false });

            (function animate() {
                requestAnimationFrame(animate);
                if (!dragging) scene.rotation.y += 0.003;
                renderer.render(scene, camera);
            })();

            window.addEventListener('resize', () => {
                camera.aspect = container.clientWidth / container.clientHeight;
                camera.updateProjectionMatrix();
                renderer.setSize(container.clientWidth, container.clientHeight);
            });
        }

        // --- Filtering (classes are hidden in both the 3D scene and the panel) ---
        function setFilter(re) {
            sceneObjects.forEach(o => {
                const visible = !re || re.test(o.name);
                o.cube.visible = visible;
                o.label.visible = visible;
            });
            document.querySelectorAll('.classCard').forEach(card => {
                card.style.display = (!re || re.test(card.dataset.name)) ? '' : 'none';
            });
            updateStats();
        }

        function applyFilter() {
            const value = document.getElementById('filterInput').value.trim();
            if (!value) { setFilter(null); return; }
            let re;
            try { re = new RegExp(value); }
            catch (err) { alert('Invalid regex: ' + err.message); return; }
            setFilter(re);
        }

        function resetFilter() {
            document.getElementById('filterInput').value = '';
            setFilter(null);
        }

        buildSidebar();
        initScene();
    </script>
</body>
</html>)HTMLDOC";

        // Splice the real class data into the template
        const std::string placeholder = "__CLASSES_JSON__";
        const std::string classes_json = getClassesJSON(parseInputFiles());
        const size_t pos = html.find(placeholder);
        if (pos != std::string::npos) {
            html.replace(pos, placeholder.size(), classes_json);
        }

        return html;
    }

    /**
     * @brief Parse input JSON files and extract class data
     * @return Vector of class data strings
     */
    std::vector<std::string> parseInputFiles() {
        std::vector<std::string> class_data;

        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";

        for (const auto& filename : input_files) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Warning: Could not open file " << filename << std::endl;
                continue;
            }

            Json::CharReaderBuilder reader_builder;
            Json::Value root;
            std::string errors;
            if (!Json::parseFromStream(reader_builder, file, &root, &errors)) {
                std::cerr << "Warning: failed to parse " << filename << ": " << errors << std::endl;
                continue;
            }

            const Json::Value& classes = root["classes"];
            if (!classes.isArray()) {
                std::cerr << "Warning: no 'classes' array in " << filename << std::endl;
                continue;
            }

            // Serialize each class object so the browser receives the real data
            for (const auto& cls : classes) {
                class_data.push_back(Json::writeString(writer, cls));
            }
        }

        return class_data;
    }

    /**
     * @brief Convert class data to JSON string for JavaScript
     * @param class_data Vector of class data strings
     * @return JSON string representation
     */
    std::string getClassesJSON(const std::vector<std::string>& class_data) {
        std::ostringstream json;
        json << "[";

        for (size_t i = 0; i < class_data.size(); ++i) {
            if (i > 0) json << ",";
            json << class_data[i];
        }

        json << "]";
        return json.str();
    }
};

/**
 * @brief Print usage information for the UML generator
 */
void print_usage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [options] <input_json_file>...\n";
    std::cout << "Options:\n";
    std::cout << "  --hide <regex>             Hide classes matching regex pattern\n";
    std::cout << "  -h, --help                 Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " data1.json data2.json\n";
    std::cout << "  " << program_name << " --hide \"^std::|Test$\" data.json\n";
}

/**
 * @brief Main entry point for the UML generator
 * @param argc Number of command line arguments
 * @param argv Command line arguments
 * @return Exit status
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Error: No input specified\n";
        print_usage(argv[0]);
        return 1;
    }

    std::vector<std::string> input_files;
    std::vector<std::string> hide_patterns;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--hide" && i + 1 < argc) {
            hide_patterns.push_back(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            input_files.push_back(arg);
        }
    }

    // Create and run the UML generator
    UMLGenerator generator(input_files, hide_patterns);

    if (generator.generate()) {
        std::cout << "UML diagram generated successfully!\n";
        return 0;
    } else {
        std::cerr << "Failed to generate UML diagram.\n";
        return 1;
    }
}
