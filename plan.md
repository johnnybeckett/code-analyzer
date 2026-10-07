# C++23 Project Plan — superseded

This was the original implementation plan for the code analyzer (parse
C++/C#/Python projects into a structured JSON class model, for a difference
mode and a UML visualizer). **It no longer matches the code — do not follow
its layout, phases, or dependency list.**

Why it is stale (verified against the tree):
- The prescribed tree does not exist: no `src/utils/`, no
  `tests/integration/`, no `cmake/` directory (CMake lives at the repo root);
  `core/visitor.cpp` / `core/factory.cpp` were never created (the real pieces
  are `src/core/report_visitor.*`, `src/core/parser_registry.*`,
  `src/core/provider_registry.*`).
- The Boost list (Spirit, Graph, PropertyTree, Algorithm) is not what the
  project uses: **Boost.JSON (linked) + header-only Boost.Beast/Asio, floor
  1.71** (`CMakeLists.txt:17`). The parsers are line/regex-based
  (`src/parser/`), not grammar-library based.
- The plan's goals (inheritance / call / visibility tracking, a JSON schema
  for diff mode + UML, white- and black-box gtests) are met by the built
  system.

**Live tracking docs: [`next.md`](next.md) (steps + status) and
[`progress.md`](progress.md) (done vs outstanding).** Current layout:
[`agents/Claude.md`](agents/Claude.md). Coding standard: [`AGENTS.md`](AGENTS.md).
