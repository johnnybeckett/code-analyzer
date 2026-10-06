# Coding Standard (AGENTS.md)

This is the **authoritative coding standard** for this project. Higher-level
background lives in [`agents/Claude.md`](agents/Claude.md); where the two
disagree, **this file wins**. Every change — including AI-assisted ones — must
follow it.

The tool is a C++23 analyzer that turns C++/C#/Python projects into JSON, a
single-threaded Boost.Beast/Asio server that serves that JSON (plus source,
diagrams, and API metadata), and a three.js viewer (`src/uml/template.h`).

## 1. Code organization

- **One class per file.** A single logical type owns its own `.h`/`.cpp` pair.
  The **only** exception is **test code**, where many test cases may share one
  file. (Adapter/facade types that are a one-line forward into another class
  may share a header, as the `IParser` adapters in `core/parsers.h` do — but
  each still gets its own `TEST` coverage.)
- Mirror the module under test. New source lives under `src/<module>/`; new
  tests under `tests/gtest/`.
- **A new `.cpp` is added to both its build target and the test target.**
  `CMakeLists.txt` lists sources explicitly — a file that is not listed is not
  built. For test files, add them to `tests/CMakeLists.txt`.
- Keep translation units small and headers light. Prefer `#pragma`-free,
  self-contained headers with correct include guards.

## 2. Design for maintainability

Implement so a reader understands *what* first and *why* second; favour
explicit, boring, testable code over clever code.

- **SOLID.** Single Responsibility (each `RestHandler` does one verb+path; each
  `IParser` does one language); Open/Closed (add a language by adding a parser
  and a `ParserRegistry` entry, not by editing the others); Liskov / Interface
  Segregation (`IParser` is the narrow contract adapters implement); Dependency
  Inversion (`UmlServer` is constructed with its data — the serialized `body`,
  `sources`, `review`, `title` — and never reads a path or a global itself).
- **Name the patterns in use and reuse them** instead of inventing parallel
  mechanisms:
  - **Facade** — `UmlModel` (`src/uml/uml_model.h`) is the single entry point
    the server and viewer use to reach the parsed model.
  - **Registry** — `Router` (`src/server/rest_handler.h`) maps `(verb, path)`
    to a handler; `ParserRegistry` maps file extension to parser.
  - **Strategy** — `RestHandler` and `IParser` are swappable strategies.
- **Pure, dependency-free logic in unit-testable functions.** Anything that can
  be expressed without I/O (e.g. `server::resolve_source`, a spatial index,
  call-edge extraction) should be a free function or a small class with pure
  methods so it is testable without sockets, disk, or a browser.

## 3. C++23 / modern-practice baseline

- **Move over copy** for non-trivial values (`std::move` at ownership transfer,
  `auto&&` forwarding where appropriate); avoid needless `const&` copies of large
  temporaries.
- **`std::print` / `std::format`** for output and string building; avoid
  `printf`-style and ad-hoc `std::ostringstream` where `std::format` fits.
- **`std::span` / `std::expected`** where they narrow an interface
  (a `span<const T>` over a buffer; `expected`/`optional` for fallible
  results) rather than raw pointers + out-params.
- **`[[nodiscard]]`** on functions whose result matters; **`constexpr`** on
  functions that can be evaluated at compile time.
- **RAII only** — no raw `new`/`delete`; `std::unique_ptr` for sole ownership,
  `std::shared_ptr` only for the genuinely shared cases (e.g. the in-server
  `UmlModel`).
- **Exceptions do not cross the HTTP boundary.** The async server must never
  let an exception escape a handler; fallible parsing returns an empty
  vector / `std::nullopt` and logs to `std::cerr` with a meaningful message.
- **Security invariants (do not weaken):**
  - `/source` only ever serves paths present in a **preloaded allowlist**; the
    filesystem path is **never** constructed from query input. The allowlist
    key is the exact recorded `file` string.
  - Any shell command (`popen`) is a **fixed server-side literal**, never built
    from request data.
  - Markdown/diagram output is **escaped before** rendering (XSS-safe).
  - Responses keep `Connection: close`, an explicit `Content-Length`, and
    `Server: umlsrv`.
- **Dependencies:** keep the Boost floor at **1.71** (do not raise it); do not
  add new third-party libraries without a stated need.

## 4. Testing (the definition of done)

A feature is **complete only when its tests pass** — proven by the build and
`ctest`, **never by a manual `curl`**. `curl`/manual browser checks are for
**investigation and debugging only**.

- **White-box:** a gtest for **every class and method** (including the pure
  helpers). Prefer small, named assertions and fixtures written to a temp
  directory (`std::filesystem::temp_directory_path()`). Reuse a `find(...)`
  helper when asserting over a `std::vector<std::unique_ptr<Class>>`.
- **Black-box:** a gtest for **every feature**, driven against the real
  boundary:
  - HTTP features: a raw-socket client (see the `raw_request`/`status_of`/
    `body_of`/`wait_ready` helpers in `tests/gtest/uml_server_test.cpp`) that
    asserts status code, headers, and body — and that new routes appear in
    `GET /api` and `GET /openapi.json`.
  - Analyzer features: an end-to-end round-trip that runs the parser/serializer
    on a fixture and asserts on the emitted JSON.
- **Every task ships both.** A change that adds a code path without a
  white-box or black-box test for it is not finished.

## 5. Documentation (complete, accurate, read-back-optimised)

Every project document (`README.md`, `README.server.md`, `README.uml.md`,
`PROJECT_DIAGRAM.md`, `AGENTS.md`, `agents/Claude.md`) is part of the
deliverable and is judged by the same bar as code:

- **Complete and accurate.** Every claim must hold against the code. A stale
  doc is a **defect**, not a footnote — fix the text, not the memory it
  relies on.
- **Optimised for reading back.** Dense, factual, scannable: `file → role`
  and `function → one-line contract` (signature, what it returns, key
  invariant) beat prose. The goal is that a reader — human or model — learns
  a file's contents and a method's behaviour in the **fewest tokens**.
- **No duplication, no filler.** If two sections say the same thing, keep the
  one a reader reaches first and delete the other. Cut marketing language and
  anything the code or this file already says.
- **Lockstep with code.** A code change updates the affected docs in the same
  change; a doc edit is verified against the code (spot-checked at
  `file:line`), never written from memory.

## 6. Build & verify (run these, not a browser)

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

- Green build + all tests passing is the only accepted completion signal.
- The Graphviz `/render` E2E test **skips** when `dot` is absent — that is
  expected and not a failure.
- Keep the build green **at every step** of a multi-part change; land each
  task (build + tests) before starting the next.

## 7. Definition of done (per task)

1. Code follows §1–§3.
2. New `.cpp`/test files are wired into the correct `CMakeLists.txt`.
3. White-box **and** black-box gtests exist and pass.
4. `cmake` build is green and `ctest` is fully green.
5. If user-facing, the relevant doc (`README.uml.md`, `README.server.md`,
   `PROJECT_DIAGRAM.md`) is updated to match.
6. All affected documents follow §5: complete, accurate, duplication-free,
   and verified against the code before the task is closed.
