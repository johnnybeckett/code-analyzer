# CMake Fixture

A small project used to exercise the **CMake layout** and the
Markdown *rendering* paths of the viewer.

## Targets

- `core` — static library (`core.cpp`, `core.h`, `common.h`)
- `net` — shared library, links `core`
- `iface` — interface library, links `net`
- `core-alias` — alias of `core`
- `app` — executable, links `net`, `util`, and the C library `z`

```cpp
Core c;
int n = c.add(1, 2);
```

See the [Graphviz diagram](graph.dot) for the shape of the graph.
