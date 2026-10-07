# Project Status — Code Analyzer

Short, verified status. Depth lives elsewhere — [`progress.md`](progress.md)
(mandate tracking: done vs outstanding), [`agents/Claude.md`](agents/Claude.md)
(module map, build, invariants), [`AGENTS.md`](AGENTS.md) (the standard).
This file is a pointer page, not a second tracker.

## What is built
- **Executables** — `CodeAnalyzer` (C++/C#/Python project → JSON class model),
  `UmlServer` (Boost.Beast/Asio REST server over that model),
  `DocumentationGenerator` (`CMakeLists.txt:44,61,73`).
- **Parsers** — all three languages implemented and tested:
  `src/parser/{cpp,csharp,python}_parser.*` plus the shared
  `src/parser/call_scanner.*`; per-language suites under `tests/gtest/`.
- **Server routes** — `/`, `/api`, `/openapi.json`, `/source`,
  `/classes/index`, `/classes/near`, `/render`, `/comments` (GET+POST),
  `/export/review` (registered in `src/server/rest_handlers.cpp:362-368` +
  `src/server/openapi.cpp`).
- **Viewer** — self-contained three.js page (`src/uml/template.h`): 3D classes,
  diff mode, call graph, source pane, spatial streaming, invert toggles,
  Ctrl+wheel content zoom.
- **Tests** — gtest white-box (per class/method) and black-box (raw-socket
  HTTP, JSON round-trips) in `tests/gtest/`; one `ctest` entry.

## Current state
- **Green** — `ctest --test-dir build` → `100% tests passed` (verified
  2026-10-07; the current build carries 141 cases — 139 at `608267b` plus the
  in-progress touch-model pair).
- **Dependencies** — Boost.JSON (linked) + header-only Boost.Beast/Asio;
  floor Boost 1.71 (`CMakeLists.txt:17`). No other Boost components.
- No version is declared (`project(CodeAnalyzer LANGUAGES CXX`,
  `CMakeLists.txt:2`) — none is claimed here.

## Outstanding
- Tracked in `progress.md` → "Outstanding": the black-box wiring tests for
  the call-graph / invert / content-zoom page features (open, this change),
  and the user-facing documentation-audit pass (in progress, this change).
