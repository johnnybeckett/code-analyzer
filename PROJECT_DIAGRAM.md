# Code Analyzer — project diagram

## What it is

A C++23 tool that parses C++ / C# / Python projects into a JSON class model,
serves it over HTTP (Boost.Beast/Asio), and renders it as a self-contained
three.js 3D viewer. Parsers are hand-written line/regex scanners — no grammar
or AST libraries (no Clang, Spirit, or ANTLR).

## Executables & libraries (from `CMakeLists.txt`)

| Target | Kind | Entry | Role |
|---|---|---|---|
| `CodeAnalyzer` | exe | `src/main.cpp` | Analyze a directory, `compile_commands.json`, or a git commit → JSON model |
| `UmlServer` | exe | `src/server/main.cpp` | Boost.Beast/Asio server: viewer page, sources, class index, rendering, review |
| `DocumentationGenerator` | exe | `tools/self_documenter.cpp` | Self-documenting HTML generator |
| `code_analyzer_core` | static lib | `src/core/`, `src/parser/`, `src/observers/` | Parsers, providers, model, JSON serialization |
| `code_analyzer_uml` | static lib | `src/uml/` | Class loading + template splicing + `UmlModel` facade |
| `code_analyzer_tests` | exe (test) | `tests/gtest/` + server sources | White-box and black-box gtests (single `add_test`) |

## File → role map

One class per file (exceptions: test files, and the one-line `IParser` adapters
in `src/core/parsers.h`). `.h/.cpp` paired on one line.

| Path | Role |
|---|---|
| `src/main.cpp` | `CodeAnalyzer` entry: CLI parsing → provider → analysis → JSON output |
| `src/core/model.h/.cpp` | Data model: `CodeElement` base, `Class`, `Method` (with `called_methods`, `accessed_variables`), `Variable`; `Visibility` / `Mutability` enums; `AnalysisResult` (classes + `CMakeGraph` + renderable sources) |
| `src/core/analyzer.h/.cpp` | Orchestration: provider → parsers (parallel via the reactor) → merged `AnalysisResult` |
| `src/core/iparser.h` | Strategy contract: one source file → `vector<unique_ptr<Class>>`; failure = empty, no exception |
| `src/core/parsers.h` | Thin `IParser` adapters bridging the static parser APIs to the contract |
| `src/core/parser_registry.h/.cpp` | Registry: file extension → parser (`cpp` / `.cs` / `.py`) |
| `src/core/source_file_provider.h` | `SourceFileProvider` interface (input source → file list) |
| `src/core/directory_provider.h/.cpp` | Provider: recursive directory walk for C++/C#/Python sources |
| `src/core/compile_commands_provider.h/.cpp` | Provider: files listed in `compile_commands.json` |
| `src/core/commit_provider.h/.cpp` | Provider: `git show` of a commit (submodule-aware) |
| `src/core/provider_registry.h/.cpp` | Factory registry: provider kind → provider |
| `src/core/config.h/.cpp` | Typed, value-semantic config (JSON-loadable) |
| `src/core/thread_pool.h/.cpp` | Bounded worker pool for parallel parsing |
| `src/core/analysis_reactor.h/.cpp` | Parses a file batch on the pool, hands results to observers lock-safely |
| `src/core/json_serializer.h/.cpp` | `AnalysisResult` → JSON via Boost.JSON |
| `src/core/cmake_analyzer.h/.cpp`, `cmake_model.h` | Pure CMakeLists.txt parser → `CMakeGraph` targets |
| `src/core/renderable_sources.h/.cpp` | Discover `.dot` / `.drawio` files (exposed to the `/render` route) |
| `src/core/report_visitor.h/.cpp` | `Visitor` / `SummaryVisitor` over the model |
| `src/observers/analysis_observer.h/.cpp` | Observer: collects analysis events |
| `src/observers/console_observer.h/.cpp` | Observer: console summary output |
| `src/parser/cpp_parser.h/.cpp` | C++ line/regex parser (classes, methods, visibility, static/virtual, call edges) |
| `src/parser/csharp_parser.h/.cpp` | C# line/regex parser |
| `src/parser/python_parser.h/.cpp` | Python line/regex parser |
| `src/parser/call_scanner.h/.cpp` | Language-agnostic callee-name extraction from a method body → call edges |
| `src/uml/uml_model.h/.cpp` | Facade: load analyzer JSON, expose the model to the server and viewer |
| `src/uml/class_loader.h/.cpp` | `JsonClassLoader`: read analyzer JSON, extract class records |
| `src/uml/template.h` | The viewer page (three.js 3D scene, diff mode, call graph, source pane, spatial streaming); three `__*__` tokens left for splicing |
| `src/uml/template_renderer.h/.cpp` | `TemplateRenderer::spliceAll`: single left-to-right pass that splices class data into the `__*__` tokens |
| `src/server/uml_server.h/.cpp` | Beast server: constructed with its data (body, sources, review, title); unmatched GET falls back to the spliced viewer page |
| `src/server/rest_handler.h` | `RestHandler` strategy + `Router` registry (`(verb, path)` → handler) |
| `src/server/rest_handlers.h/.cpp` | Domain routes: `/source`, `/classes/index`, `/classes/near`, `/render`, `/comments` (GET+POST), `/export/review` |
| `src/server/source_resolver.h/.cpp` | Pure allowlist resolution for `/source`; never builds a filesystem path from request input |
| `src/server/class_index.h/.cpp` | Spatial index powering `/classes/index` and `/classes/near` (`kMaxNearest = 256`) |
| `src/server/review_store.h/.cpp` | In-memory review comments + Markdown export |
| `src/server/openapi.h/.cpp` | `/api` and `/openapi.json` (OpenAPI 3.0.3) derived from registered routes |
| `src/server/http_util.h/.cpp` | HTTP helpers: percent-decode, renderer availability check, fixed-literal `popen` renderer run |
| `src/server/main.cpp` | `UmlServer` entry: load JSON + sources + review → run server |
| `tools/self_documenter.cpp` | `DocumentationGenerator` source |
| `tests/gtest/main_test.cpp` | gtest entry point; `AnalyzerTest` + `CppParserTest` |
| `tests/gtest/registry_test.cpp` | `ParserRegistry` white-box |
| `tests/gtest/provider_test.cpp` | Directory / compile-commands / commit providers |
| `tests/gtest/observer_test.cpp` | `EventDispatcher` + `ConsoleObserver` |
| `tests/gtest/config_test.cpp` | `Config` load/typed access |
| `tests/gtest/thread_pool_test.cpp` | `ThreadPool` + `AnalysisReactor` (incl. parity) |
| `tests/gtest/csharp_test.cpp` | C# parser white-box |
| `tests/gtest/cpp_kind_test.cpp` | Parser kind/visibility across C++ / C# / Python |
| `tests/gtest/call_graph_test.cpp` | Call-edge extraction (scanner, C++, C#) + serialization |
| `tests/gtest/source_file_fix_test.cpp` | Source-file resolution fix |
| `tests/gtest/cmake_analyzer_test.cpp` | CMake parser |
| `tests/gtest/class_index_test.cpp` | Spatial index (layout, JSON, empty) |
| `tests/gtest/uml_test.cpp` | `TemplateRenderer` splicing |
| `tests/gtest/uml_model_test.cpp` | `UmlModel` facade |
| `tests/gtest/uml_server_test.cpp` | Black-box: raw-socket HTTP against the real server |
| `tests/gtest/render_route_test.cpp` | Black-box: `/render` route (skips when `dot` absent) |
| `tests/gtest/openapi_spec_test.cpp` | `/api` + `/openapi.json` content |

## Server routes (verified against `rest_handlers.cpp` / `openapi.cpp`)

| Route | Contract |
|---|---|
| `GET /` | Spliced viewer page — the fallback for any unmatched GET (`uml_server.cpp`), not a registered handler |
| `GET /source?path=` | One allowlisted source file by recorded path (percent-decoded); `404` otherwise |
| `GET /classes/index` | Class count, layout bounds, cell size, 64-class sample; `501` if no index loaded |
| `GET /classes/near?x&y&z&count=` | The `count` nearest classes, closest first; `count` clamped to `[1, 256]`; `400` on bad params, `501` if no index |
| `GET /render?path=` | Render an allowlisted `.dot` / `.drawio` to SVG (fixed-literal `dot -Tsvg` / `drawio -x -f svg`); `400` unsupported format, `502`/`503` renderer failure |
| `GET /comments` | Review comments as JSON (empty list when review disabled) |
| `POST /comments` | Add a review comment (`text` required; one of `oldLine`/`newLine` positive) → `201` with the stored comment |
| `GET /export/review` | The review as a Markdown download (`Content-Disposition: attachment`) |
| `GET /api` | Compact description of every registered endpoint |
| `GET /openapi.json` | The OpenAPI 3.0.3 document describing this API |

## Data flow

providers (`directory` / `compile_commands` / `commit`) → `Analyzer` →
parsers in parallel (`thread_pool` + `analysis_reactor`) → merged
`AnalysisResult` (`src/core/model.h`) → JSON (`src/core/json_serializer.cpp`,
Boost.JSON) → `UmlServer` loads it (`class_loader`), splices the viewer page
(`template_renderer`) → served over HTTP → three.js viewer.

## Dependencies

- **C++23**, CMake ≥ 3.22.
- **Boost floor 1.71** — `find_package(Boost 1.71 REQUIRED COMPONENTS json)`;
  `Boost::json` linked into both static libs; **Beast/Asio header-only**
  (reach the server transitively via Boost's include dir).
- **three.js r128** — loaded from a CDN `<script>` tag in the viewer page
  (`src/uml/template.h`); not bundled.
- **GTest** — test suite only.
- **Doxygen** — optional `doc` target.
