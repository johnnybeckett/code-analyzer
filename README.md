# Code Analyzer

A C++23 tool that parses C++/C#/Python projects into a JSON class model
(classes, methods, fields, inheritance, call-graph edges, CMake layout,
renderable sources) and serves it as an interactive three.js 3D class-diagram
viewer over HTTP. Two executables, both in `build/bin/` after building.

## 1. `CodeAnalyzer` — the analyzer (`src/main.cpp`)

```
Usage: CodeAnalyzer [options] <input_path>
```

| Flag | Contract |
|---|---|
| `<input_path>` (positional), or `--directory <root>` | Recursively analyze the project rooted at `<root>`: every registered source file it finds |
| `--compile-commands <path>` | Analyze only the `.cpp` translation units listed in `compile_commands.json`; does not follow `#include`d headers |
| `--commit <repo[@ref]>` | Analyze the repository from git objects at `ref` (default `HEAD`); no checkout needed |
| `--staging <dir>` | With `--commit`: materialize the commit's files into `<dir>` and keep them (otherwise a temp dir is removed) |
| `--json <file>` | Write the full analysis as JSON to `<file>` |
| `--config <file>` | Read `source_extensions` / `skip_dirs` from a JSON config file |
| `-h, --help` | Print usage |

Directory mode additionally parses `CMakeLists.txt` / `*.cmake` (the viewer's
CMake layout) and collects renderable non-code files — Markdown (`.md`,
`.markdown`), Graphviz (`.dot`), Draw.io (`.drawio`, `.draw.io`) — into the
JSON's `sources` array for the server to serve and render
(`src/main.cpp:167-171`, `src/core/renderable_sources.cpp:18-24`).

## 2. `UmlServer` — the HTTP viewer server (`src/server/main.cpp`)

```
Usage: UmlServer [options] <input_json_file>...
```

| Argument | Contract |
|---|---|
| `<input_json_file>` ×1 | A single combined diagram |
| `<input_json_file>` ×2 | Diff mode, in `older newer` order: added green, removed red |
| × more than 2 | Diff uses the first two; the rest are ignored (a warning is printed) |
| `--port <n>` | TCP port to listen on (default `8000`; must be `1..65535`) |
| `--hide <regex>` | Hide classes matching the pattern; **repeatable** — one pattern per occurrence |
| `-h, --help` | Print usage |

The analyzer has a different CLI (it takes source input, not JSON, and has no
`--hide`). The server's full route list, allowlist rules, and review-comment
semantics are in [README.server.md](README.server.md).

## Supported languages

Extension → parser, registered in one place
(`src/core/parser_registry.cpp:31`, defaults in `src/core/config.h:28-31`):

- **C++**: `.cpp .cc .cxx .c .h .hpp .hxx .hh .tpp .tcc`
- **C#**: `.cs` — parser contract and limitations in
  [README_CSHARP_PARSER.md](README_CSHARP_PARSER.md)
- **Python**: `.py`

## Build & test

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Green build + fully green `ctest` is the completion signal; the Graphviz
`/render` E2E test **skips** when `dot` is absent, which is expected.

## Where to look

| Doc | Contents |
|---|---|
| [README.server.md](README.server.md) | `UmlServer`: CLI, HTTP routes, source/render allowlist, review comments |
| [README.uml.md](README.uml.md) | The viewer: 3D navigation, diff mode, source pane, filters |
| [README_CSHARP_PARSER.md](README_CSHARP_PARSER.md) | The C# parser: what it extracts, what it does not record |
| [PROJECT_DIAGRAM.md](PROJECT_DIAGRAM.md) | Project structure |
| [SOLUTION_SUMMARY.md](SOLUTION_SUMMARY.md) | Design rationale |
| [AGENTS.md](AGENTS.md) | The coding standard (authoritative) |
| [agents/Claude.md](agents/Claude.md) | File → role map |

A third executable, `DocumentationGenerator` (`tools/self_documenter.cpp`),
writes `code_analyzer_docs.html` on demand — a **runtime output** (gitignored
under "Generated analyzer / docs outputs", `.gitignore:30-33`), not a
repository artifact.
