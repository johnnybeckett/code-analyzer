# Code Analyzer — project map

What the project is and where things live. The **authoritative standard**
(coding rules, testing, documentation, definition of done) is
[`AGENTS.md`](../AGENTS.md); where this file disagrees, AGENTS.md wins.

## What it is

A C++23 tool that parses C++ / C# / Python projects into a JSON class model,
then serves an interactive 3D viewer (three.js, self-contained page) over HTTP.
Two executables:

- **`CodeAnalyzer`** (`src/main.cpp`) — analyze a directory, a
  `compile_commands.json`, or a git commit → JSON model.
- **`UmlServer`** (`src/server/main.cpp`) — Boost.Beast/Asio server: the viewer
  page plus allowlisted source, a spatial class index, diagram rendering, and
  review comments (see `README.server.md` / `README.uml.md`).

## Module map

`file → role`. Headers are self-contained; one class per file (test code and
the one-line `IParser` adapters in `core/parsers.h` are the only exceptions).

| Path | Role |
|---|---|
| `src/core/model.h` | Data model: `Class` / `Method` / `Call` / `Variable`; the shared vocabulary |
| `src/core/analyzer.*` | Orchestration: provider → parsers (parallel) → merged model |
| `src/core/iparser.h` | Strategy contract: one source file → `vector<unique_ptr<Class>>`; failure = empty, no exception |
| `src/core/parser_registry.*`, `core/parsers.h` | Registry: extension → parser (`cpp` / `.cs` / `.py`); thin adapters bridge the static parser APIs to `IParser` |
| `src/core/{directory,compile_commands,commit}_provider.*` | Input sources (`SourceFileProvider`): directory walk, compile database, `git show` of a commit (submodule-aware) |
| `src/core/provider_registry.*` | Factory registry mapping provider kind → provider (mirrors `ParserRegistry`) |
| `src/core/config.*` | Typed value-semantic config (replaces an old singleton); JSON-loadable |
| `src/core/thread_pool.*`, `analysis_reactor.*` | Bounded worker pool; the reactor that parses a file batch and hands results to observers lock-safely |
| `src/core/json_serializer.*` | Model → JSON via `Boost.JSON` |
| `src/core/cmake_analyzer.*`, `cmake_model.h` | Pure CMakeLists.txt parser → `CMakeGraph` targets |
| `src/core/renderable_sources.*` | Discover `.dot` / `.drawio` files for the `/render` route |
| `src/core/report_visitor.*`, `src/observers/` | Visitor over the (plain-data) model; console summary + analysis observers |
| `src/parser/{cpp,csharp,python}_parser.*` | Line/regex-based per-language parsers (no grammar libraries) |
| `src/parser/call_scanner.*` | Language-agnostic extraction of callee names from a method body → call edges |
| `src/uml/uml_model.*` | Facade over the parsed model used by server and viewer |
| `src/uml/template.h`, `template_renderer.*`, `class_loader.*` | The three.js viewer page: 3D classes, diff mode, call graph, source pane, spatial streaming |
| `src/server/uml_server.*` | Beast server; constructed with its data (body, sources, review, title) — never reads a path or global itself |
| `src/server/rest_handler.h` | `RestHandler` strategy + `Router` registry (`(verb, path)` → handler) |
| `src/server/rest_handlers.*` | Routes: `/`, `/api`, `/openapi.json`, `/source`, `/classes/index`, `/classes/near`, `/render`, `/comments` (GET+POST), `/export/review` |
| `src/server/source_resolver.*` | Pure allowlist resolution for `/source`; **never** builds a filesystem path from request input |
| `src/server/class_index.*` | Spatial index powering `/classes/near` streaming |
| `src/server/review_store.*` | In-memory review comments + Markdown export |
| `src/server/openapi.*` | OpenAPI document derived from the registered routes |
| `tools/self_documenter.cpp` | `DocumentationGenerator` target (self-documenting HTML) |
| `tests/gtest/` | White-box (per class/method) **and** black-box (raw-socket HTTP against the real server; JSON round-trips) |

## Build & test (the only completion signal)

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
```

Boost floor **1.71** (JSON linked; Beast/Asio header-only). Targets:
`CodeAnalyzer`, `UmlServer`, `DocumentationGenerator`, plus the
`code_analyzer_core` / `code_analyzer_uml` static libraries. A new `.cpp` must
be added to **both** its target and the test target in `CMakeLists.txt` /
`tests/CMakeLists.txt` — unlisted files are not built.

## Invariants (do not weaken)

- `/source` serves only preloaded allowlisted paths; the query is never used to build a path.
- Shell commands (`popen`) are fixed server-side literals (e.g. `dot`), never built from request data.
- Markdown/diagram output is escaped before rendering (XSS-safe).
- Exceptions never cross the HTTP boundary; fallible work returns empty/`nullopt` + logs to `stderr`.
- Responses keep `Connection: close`, explicit `Content-Length`, `Server: umlsrv`.
- `curl`/browser checks are for investigation only — **tests** are the proof.

## Extending

- **New language**: `src/parser/<lang>_parser.{h,cpp}` + adapter in
  `core/parsers.h` + `ParserRegistry` entry + white- and black-box gtests +
  both CMakeLists entries.
- **New route**: one `RestHandler` subclass (one verb+path per class) in
  `rest_handlers.cpp`; it automatically appears in `/api` and `/openapi.json`.
- **Any user-facing change**: update the matching doc
  (`README.server.md`, `README.uml.md`, `PROJECT_DIAGRAM.md`) in the same
  change, per AGENTS.md §5.
