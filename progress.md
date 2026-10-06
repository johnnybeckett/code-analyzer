# Progress

> Snapshot of the 5-part mandate. **Last verified: 139/139 gtest cases, single
> `ctest` entry green** (`ctest --test-dir build --output-on-failure` →
> `100% tests passed`). Graphviz `/render` E2E skips when `dot` is absent
> (expected).

## The mandate (5 parts)

1. **Streaming / nearest-N** — a 10k-class project eats client RAM. Provide a
   server API for the *nearest N objects to a coordinate*; the page streams the
   visible window and caches neighbouring grid regions, staying smooth.
2. **Invert toggles** — two independent toggles to invert horizontal and
   vertical mouse movement, separately.
3. **Ctrl+wheel content zoom** — when viewing source / diagrams / the call
   tree, `Ctrl`+mouse-wheel zooms / resizes the content. (Call tree does not
   exist yet → **build a whole-project call-graph view**.)
4. **Source-file bug** — "usually there is no source file registered when
   setting the focus" — must be fixed.
5. **`AGENTS.md` coding standard** — one class per file (except tests);
   SOLID + design patterns; modern C++23; white-box gtests for all classes /
   methods; black-box gtests for features; **completion proven by tests,
   never manual curl**.

**Locked decisions (prior session):** Task 1 → *server computes* nearest-N
(client streams + grid-caches); Task 3 → *+ build a call-graph view*; Task 3
scope → *all three languages* + *whole-project* call graph.

**Approved implementation order (build green each step):**
1. Task 4 → 2. Task 5 → 3. Task 3a → 4. **Task 1** → 5. Task 3b/3c →
6. Task 2 → 7. Docs + final `ctest`.

---

## Performed (DONE, verified green)

### ✅ Task 4 — source-file registration bug
- **Absolute `file` paths at parse time** — `src/core/directory_provider.cpp:36`
  anchors each class's `file` to `std::filesystem::absolute(...).lexically_normal()`
  so JSON `file` fields are CWD-independent (kills the analyzer-vs-server CWD
  mismatch that produced an empty allowlist).
- **Robust server resolution** — `src/server/source_resolver.cpp` (helper
  `server::resolve_source`) tries the raw path, then CWD-relative, then
  relative to the JSON file's directory; `main.cpp` logs an
  `Loaded N of M source file(s) for /source` summary so an empty allowlist is
  loud, not silent.
- **Multi-class files** — C# / Python parsers record `file` on **every** emitted
  class (removed the first-class-only `break`/limit), so per-class source +
  call data is no longer dropped.
- **Tests** — `tests/gtest/source_file_fix_test.cpp` (white-box: provider yields
  absolute paths; 2-class C#/Python file → both classes carry `file` +
  `called_methods`; black-box: server built from JSON with relative-to-JSON-dir
  paths, run from a different CWD → `/source` returns 200, non-listed still 404).

### ✅ Task 5 — `AGENTS.md` coding standard
- `AGENTS.md` at repo root states: one class per file (except tests, with the
  adapter/facade exception); SOLID + named patterns (Facade `UmlModel`,
  Registry `Router`/`ParserRegistry`, Strategy `RestHandler`/`IParser`);
  C++23 (move over copy, `std::print`/`std::format`, `std::span`/`std::expected`,
  `[[nodiscard]]`/`constexpr`, RAII); exceptions never cross the HTTP boundary;
  security invariants (allowlist-only `/source`, fixed `popen` literals,
  escape-first markdown, `Connection: close` + explicit `Content-Length` +
  `Server: umlsrv`); Boost floor 1.71 (do not raise), no new third-party libs;
  white-box + black-box gtest rule; "a change without white- and black-box
  tests is not finished"; run `cmake`/`ctest`, keep build green.

### ✅ Task 3a — call-edge extraction (analyzer side)
- **New shared pure util** `src/parser/call_scanner.{h,cpp}` —
  `CallScanner::extract(body, blacklist) → std::vector<std::string>`; detects
  `identifier(` call tokens (incl. `obj.`, `this->`, `A::`), strips
  blacklisted keywords/casts, dedups preserving first-seen order. Language
  agnostic; per-language blacklists live in each parser.
- **Each parser captures each method's body** and stores results in
  `Method::called_methods` (`core/model.h`):
  - C++ `cpp_parser.cpp` — brace-matches the body, strips cast/control keywords.
  - C# `csharp_parser.cpp:289-303` — brace-matches body; **fixed the
    2nd+ type mis-anchoring** by computing `class_pos` absolutely
    (`std::distance(...) + matches.position(0)`, `#include <iterator>`);
    blacklist `csharp_call_blacklist()` (`:113-120`).
  - Python `python_parser.cpp` — captures body by strict indentation
    (`indent < body_indent` end, `:163`; `bindent <= indent` leave-method, `:243`);
    blacklist `python_call_blacklist()` (`:37-44`).
- **Serializer** `src/core/json_serializer.cpp` — emits `called_methods`
  (array of strings) per method, after `parameters`.
- **Tests** — `tests/gtest/call_graph_test.cpp` (16 cases): `CallScannerTest`
  ×10 (detection, qualified/`this->`/`A::`, keyword/cast stripping, dedup +
  order, empty body); `CppCallGraphTest.InlineMethodCapturesCallees`;
  `CSharpCallGraphTest.MethodCapturesCallees`;
  `CSharpCallGraphTest.MultiClassFileAllMethodsCarryCallees` (regression for the
  2nd+ type anchor bug); `PythonCallGraphTest.MethodCapturesCallees`;
  2 `CallGraphSerializeTest` (round-trip: `called_methods` present, empty
  arrays serialized as `[]`).

---

### ✅ S1 — Task 1 core: `ClassIndex` (white-box green)
- **`src/server/class_index.{h,cpp}`** — new `ClassIndex` (one class per file):
  immutable `ClassRecord`s (mirroring the JSON field names in
  `json_serializer.cpp`) with `record_from_json` (1:1 parse, `nullopt` on
  garbage); **deterministic namespace-grouped 3D grid layout** (groups sorted
  by namespace, members by name within a group, smallest 3D cube per group,
  groups tiled along +X with `kGroupGap`) — stable across runs; **uniform
  grid-bucket spatial index**; `nearest(count,x,y,z)→std::span<const
  ClassRecord>` (ring expansion with a proven lower-bound stop — exact
  N-closest, ascending distance, deterministic tiebreak); `bounds()`,
  `cellSize()`, `size()`, `records()`, `sample(k)`.
- **CMake** — `class_index.cpp` added to the `UmlServer` target **and** the
  `code_analyzer_tests` server-source list (AGENTS.md §1: both targets).
- **Tests** — `tests/gtest/class_index_test.cpp` (12 cases): layout
  determinism, namespace grouping, bounds/`cellSize` consistency, `nearest`
  vs brute force for every k, own-cell inclusion, count clamp, zero count,
  `sample` ≤ k + within bounds + first/last span, `record_from_json` full
  parse + garbage rejection, empty-index edges.
- **Green-check** — 133/133 pass (121 baseline + 12 new).

---

### ✅ S2 — Task 1 server: `/classes` endpoints (black-box green)
- **Two endpoints** in `src/server/rest_handlers.{h,cpp}` (Strategy, mirroring
  `SourceHandler`): `IndexHandler` → `GET /classes/index` (200 JSON
  `{count, bounds, cellSize, sample}`; `sample` = `index_->sample(64)`) and
  `NearestHandler` → `GET /classes/near?x=&y=&z=&count=N` (200 JSON array of
  full records, closest first). Both 501 `json_error` when the index is null.
- **Validation (before the 501 check):** `x`/`y`/`z` parsed with
  `std::stod` + `std::isfinite` — missing/non-numeric → 400; `count` required
  positive integer (missing / non-numeric → 400), then clamped to
  `[1, ClassIndex::kMaxNearest=256]`.
- **`UmlServer` wiring** — optional `std::shared_ptr<const ClassIndex>
  classIndex` ctor param after `title` (existing call sites keep compiling);
  `classIndex_` declared before `io_`/`router_` (router last → destroyed first,
  so no handler outlives state); passed into `register_domain_handlers`, which
  registers `IndexHandler` + `NearestHandler` **first**, so the two routes lead
  `GET /api` and `GET /openapi.json`.
- **`main.cpp`** — builds the `ClassIndex` from the CURRENT revision's classes
  (newer file in diff mode, single file otherwise) via
  `JsonClassLoader::parseFileClasses` + `record_from_json` (warns on unparsed),
  and passes it as the 6th ctor arg.
- **Tests** — 5 new black-box cases in `tests/gtest/uml_server_test.cpp`
  (raw-socket, `wait_ready`/`raw_request`/`status_of`/`body_of`):
  `ClassesIndex_ServesSpatialOverview` (200, `count`/`bounds`/`cellSize`/
  `sample`); `ClassesNear_ReturnsNearestFirstAndClampsCount` (nearest-first
  ordering — group-A class before group-B, `count` clamp to index size);
  `ClassesNear_ValidatesParameters` (missing coord, non-numeric coord, missing
  / non-numeric `count` → 400); `ClassesRoutes_AppearInApiAndOpenApi`;
  `ClassesRoutes_NullIndexIs501` (+ regression: `/comments` 200, unmatched GET
  200).
- **Boost.JSON API notes (this system's Boost 1.90):** `json::array` has no
  `append()` and `json` has no `null()` sentinel — arrays are filled with
  `emplace_back(value)` (a default-constructed `json::value{}` is the null
  slot); `as_string()` yields `boost::json::string` (converts only to
  `string_view`) → construct `std::string` by direct-initialization.
- **Green-check** — 138/138 pass (133 baseline + 5 new); single `ctest` entry
  `100% tests passed`.

---

### ✅ S3 — Task 1 client: streaming in `template.h` (black-box green)
- **Self-contained `streaming` manager** in `src/uml/template.h` (an IIFE,
  defined right after `setLodLimit` so it closes over `nodes`/`diagram`/
  `makeClassBox`/`view`/`camera`/`isAbstract`/`applyLod`, all already in scope).
  The eager all-box build stays the **no-regression default**; streaming only
  engages when `/classes/index` reports a project past the `SMALL` (2000)
  threshold — a small project, an offline page, or a missing index all leave the
  eager build in place (the `.catch` is the fallback path).
- **On-enable** (`maybeEnable`): wraps each real box in a cheap placeholder
  `THREE.Group` holder (so `n.mesh` stays valid for picking / layout / LOD),
  tags `n.mesh.userData.node = n`, then seeds the first window via
  `requestNear()`.
- **Streaming loop**: on camera move (`onMove`, wired into the `mousemove` and
  `wheel` handlers), **debounce** (120 ms) → `requestNear()` around the current
  view target: a **grid-cell cache** (`gridCache`, LRU-capped to 24 cells) makes
  a pan-back instant (reconcile from memory, no round-trip); otherwise
  `GET /classes/near?x=&y=&z=&count=WINDOW` (one-in-flight + `seq` sequence-token
  stale-ignore). `reconcile(names)` **materializes** a box (via `makeClassBox`)
  when a class enters the window and **disposes** it (`.geometry.dispose()` +
  material/`.map.dispose()`) when it leaves — the bounded live set *is* the RAM
  fix; external/CMake stubs never materialize.
- **Picking** — the dblclick handler now resolves the hit node via
  `hitObj.userData.node` (a streaming box is a holder child) with a fallback to
  the original `nodeList.find(nd => nd.mesh === hitObj)` match (eager path).
  **Trigger** — `streaming.maybeEnable()` is called once, right after
  `applyLayout(layoutSelect.value)` in init.
- **Tests** — new black-box case in `tests/gtest/uml_server_test.cpp`:
  `ServesPageWithStreamingWiring` builds the **real spliced page** (new
  `real_page()` helper → `uml::UmlModel({json},{}).build_html()`, same path as
  `main.cpp`), serves it via a raw socket, and asserts the page contains
  `/classes/index`, `/classes/near`, `materialize`, `dispose`, `gridCache`,
  `onMove`, `maybeEnable`, `requestNear`. `UmlModel` include + `write_json`
  helper added to the test TUs; `template_renderer`/`template.h` unchanged.
- **Green-check** — **139/139** pass (138 baseline + 1 new); single `ctest`
  entry `100% tests passed`. JS syntax verified via `node --check` on the
  extracted `<script>` block (debugging aid only — not a completion claim).

---

## Outstanding (IN PROGRESS / not started)

### ⬜ Task 3b/3c — call-graph view + Ctrl+wheel zoom
- **3b** — whole-project call-graph view in `template.h` (method nodes +
  directed edges from `called_methods`; resolve by name preferring same-class
  then global, flag ambiguity).
- **3c** — `Ctrl`+wheel `contentZoom` branch in the wheel handler
  (`template.h:1721-1724`) with `preventDefault`, applied to the source pane,
  rendered diagrams, and the call-graph.
- **Black-box** — served page contains the call-graph wiring + the
  `e.ctrlKey` zoom branch.

### ⬜ Task 2 — invert horizontal / vertical toggles
- Two independent toolbar checkboxes; state in the view/config object; orbit
  handler `template.h:1717` (yaw `dx`) / `:1718` (pitch `dy`) multiplied by
  −1 when the corresponding toggle is on. No server change.
- **Black-box** — served page contains both toggle controls + the two
  sign-multiplier sites.

### ⬜ Docs + final full `ctest`
- Update `README.uml.md`, `README.server.md`, `PROJECT_DIAGRAM.md` for the new
  `/classes` endpoints, the call-graph view, the invert toggles, Ctrl+wheel
  zoom, and the streaming/grid-caching model; point at `AGENTS.md`.
- Final full `cmake -S . -B build && cmake --build build -j && ctest
  --test-dir build --output-on-failure`.

---

## Standing constraints (preserve)
- Server security: preloaded allowlist (never build fs paths from query;
  allowlist key = exact recorded `file` string); `popen` cmds are fixed
  server-side literals; escape-first markdown (XSS-safe); `Connection: close`,
  explicit `Content-Length`, `Server: umlsrv`; exceptions never cross the HTTP
  boundary (catch → `json_error` / log to `std::cerr`).
- **No manual curl to declare completion** — black-box gtests only; curl for
  investigation/debugging only.
- One class per file (except tests); C++23 best practices; SOLID + design
  patterns.
- Boost min 1.71 — **do not raise**; no new third-party libraries.
- New `.cpp` files added to **both** the build target and `tests/CMakeLists.txt`.
- Keep the build green at every step; a task is complete only when its
  white-box **and** black-box tests pass under `ctest`.
