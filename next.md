# Next — execution plan for the remaining work

Sized to fit a single bounded context pass per step. Each step ends with a
**green-check** (`cmake -S . -B build && cmake --build build -j && ctest
--test-dir build --output-on-failure`). Do not start a step until the previous
one is green. Keep re-reads minimal — the exact contract lines are already
recorded below; open a file only when about to edit it.

**Context-budget rule:** one feature per step; read the *specific* file/lines
listed, edit, wire CMake, add tests, then run the single `ctest` gate. Do not
re-read large files (parsers, `template.h`) unless the step names them.

Order (build green each step):
**S1 Task 1 core → S2 Task 1 endpoints/wiring → S3 Task 1 client streaming →
S4 Task 3b/3c → S5 Task 2 → S6 Docs + final gate.**

---

## S1 — `ClassIndex` core (white-box green)
Goal: a testable spatial index with no server/sockets.

1. Create `src/server/class_index.h` + `src/server/class_index.cpp`:
   - `struct ClassRecord` — immutable; fields mirror `json_serializer.cpp`
     names: `name`, `kind`, `namespace`, `visibility`, `static`, `file`,
     `inheritance` (array), `methods` (summary), `variables` (summary). Add a
     3D position `float x,y,z`.
   - `class ClassIndex` (one class per file, `server` namespace):
     - ctor takes `std::vector<ClassRecord>` (moved); assigns a **deterministic
       namespace-grouped 3D grid** position to each (group by `namespace`,
       sub-grid within group, groups tiled); builds a **uniform grid-bucket**
       spatial index (`std::map<key, std::vector<size_t>>` or flat buckets).
     - `[[nodiscard]]` pure methods: `nearest(count,x,y,z) →
       std::span<const ClassRecord>` (expand buckets outward by distance until
       `count` gathered, then sort + truncate — correct N-closest + order);
       `bounds()`, `cellSize()`, `size()`, `sample(k)` (≤ k, within bounds).
     - `size()==0` handled cleanly (empty `span`, zeroed bounds).
2. CMake: add `src/server/class_index.cpp` to the **`UmlServer`** target
   (`CMakeLists.txt:61-69`) **and** to `code_analyzer_tests` server-source list
   (`tests/CMakeLists.txt:4-27`). Add `gtest/class_index_test.cpp` to the test
   `.cpp` list.
3. `tests/gtest/class_index_test.cpp`: determinism (same input → same positions
   across two `ClassIndex` builds); `nearest()` returns exactly N closest,
   sorted ascending by distance, and includes the point's own cell; `bounds` /
   `cellSize` consistency; `sample(k)` ≤ k and within bounds; empty-index edge
   (size 0, `nearest` → empty).
4. **Green-check.** Expected: 121 + (new class_index cases) all pass.

## S2 — endpoints + `UmlServer`/`main` wiring (black-box green)
1. `src/server/rest_handlers.h` — add `IndexHandler` + `NearestHandler` mirroring
   `SourceHandler` (inline ctor taking `const ClassIndex* index_`, `handle`,
   `describe`); forward-declare / include `server/class_index.h`. Extend
   `register_domain_handlers(...)` declaration (`:113-116`) to accept the index.
2. `src/server/rest_handlers.cpp` — implement `handle`/`describe`:
   - `GET /classes/index` → `{ count, bounds, cellSize, sample:[...] }`
     (boost::json). Missing index → `json_error(501, ...)`.
   - `GET /classes/near?x=&y=&z=&count=N` — parse floats via `first_query`
     (`http_util.h`); any missing/invalid → `json_error(400,...)`; clamp `count`
     to `[1, kMax]`; null index → 501.
   - In `register_domain_handlers` body (`:247-256`) register both **before**
     the meta handlers so they appear in `GET /api` + `GET /openapi.json`.
3. `src/server/uml_server.h` — add optional ctor param
   `std::shared_ptr<const ClassIndex> classIndex = nullptr` after `title`;
   add `std::shared_ptr<const ClassIndex> classIndex_;` member **before `io_`**
   (after `title_`, `:94-105`). Existing call sites still compile.
4. `src/server/uml_server.cpp` — init-list: `classIndex_(std::move(classIndex))`;
   pass it into `register_domain_handlers(...)` (`:172`).
5. `src/server/main.cpp` — build the `ClassIndex` from the parsed classes (reuse
   `JsonClassLoader` statics over `input_files`, mapping each class JSON element
   to a `ClassRecord`) and pass as the 6th ctor arg (`:173-175`).
6. Extend `tests/gtest/uml_server_test.cpp` (raw-socket helpers already there):
   - server **with** an index → `/classes/index` 200 (has `count`/`bounds`/
     `sample`); `/classes/near` valid → 200, `count` clamped; missing/invalid
     → 400; **both routes present in `GET /api` and `GET /openapi.json`**.
   - server **without** an index (existing `UmlServer(body, {}, 0)` shape) →
     both routes return 501.
7. **Green-check** — full `ctest` passes (white + black for Task 1 server side).

## S3 — client streaming (`template.h`) + black-box
1. Read only the streaming-relevant regions: `:1198-1200`, `:1458`, `:1475`,
   `:1543-1546`, `:1635-1655`, `:1717-1724`.
2. Replace the at-load all-box build with a streaming module:
   - on load: `GET /classes/index` → cheap overview (sample); then `GET
     /classes/near` around the current view target.
   - on camera move: **debounce + one-in-flight + sequence-token stale-ignore**.
   - **materialize** a box only when a class enters the window; **dispose**
     (`.dispose()`) when it leaves — the bounded live set *is* the RAM fix.
   - **grid-cell cache** of the last N cells for instant pan-back.
   - small projects (index `count` < threshold) render the whole set at once.
3. Black-box: the served page (raw socket) references the streaming endpoints
   (`/classes/index`, `/classes/near`) and the materialize/dispose + grid-cache
   wiring. **Green-check.**

## S4 — call-graph view + Ctrl+wheel zoom (black-box)
1. `template.h`: add a whole-project call-graph view (toolbar toggle/tab):
   method nodes + directed edges from `called_methods`; resolve edges by name —
   prefer same-class target, then global, flag ambiguity. Reuse existing
   theme / render helpers; scale-invariant for clean Ctrl+wheel zoom.
2. `template.h:1721-1724` wheel handler: `if (e.ctrlKey) { adjust shared
   contentZoom; e.preventDefault(); return; }` — else existing `view.dist`
   zoom. Apply `contentZoom` to source pane + rendered diagrams + call-graph
   (CSS transform / font-size).
3. Black-box: served page contains the call-graph wiring **and** the
   `e.ctrlKey` zoom branch. **Green-check.**

## S5 — invert horizontal / vertical toggles (black-box)
1. `template.h`: two independent toolbar checkboxes ("Invert horizontal" /
   "Invert vertical"), state in the existing view/config object.
2. Orbit handler `template.h:1717-1718`:
   `view.yaw += dx * (invertH ? -1 : 1) * 0.005 * fine;`
   `view.pitch = clamp(view.pitch + dy * (invertV ? -1 : 1) * 0.005 * fine, -1.4, 1.4);`
3. Black-box: served page contains both toggle controls **and** the two
   sign-multiplier sites. **Green-check.**

## S6 — Docs + final full gate
1. Update `README.uml.md`, `README.server.md`, `PROJECT_DIAGRAM.md`: new
   `/classes` endpoints, call-graph view, invert toggles, Ctrl+wheel zoom,
   streaming/grid-caching model; point at `AGENTS.md`.
2. Final: `cmake -S . -B build && cmake --build build -j && ctest --test-dir
   build --output-on-failure` — all pass. **Feature completion = the black-box
   gtests, never manual curl.**

---

## Contract quick-reference (verified, to avoid re-reading)
- `http_util.h`: `percent_decode`, `parse_query`, `first_query`, `blank`,
  `json_error` (in `rest_handlers.cpp`, namespace-local), `first_query(req,name)`
  → `const std::string*`.
- `SourceHandler` is the mirror template (inline ctor + `private:` pointer
  member). `register_domain_handlers` body at `rest_handlers.cpp:247-256`.
- `UmlServer` ctor: `(body, sources, port, review=nullptr, title={})`; members
  ordered `body_ sources_ review_ title_ io_ acceptor_ assigned_port_ router_`
  (Router destroyed first — a new index member must sit **before `io_`**).
- `JsonClassLoader` statics (`class_loader.h`): `parseFileClassesStrict`,
  `parseFileSources`, `getClassesJSON`, `parseCmakeMerged`, … each vector
  element is one class object's serialized JSON → the reuse point for
  `ClassRecord`.
- `json_serializer.cpp:39-88` = the exact field names `ClassRecord` mirrors.
- `main.cpp:173-175` = the `UmlServer` call site to extend (6th arg).
- `main.cpp:122` builds `UmlModel`; `:137-152` preloads the allowlist.
- **Not a git repo** — no commits; the two `.md` files are the deliverables
  for this pass, S1–S6 the implementation.
