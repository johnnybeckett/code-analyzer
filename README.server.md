# UML Model Server

An HTTP server that serves the UML "model" — the same self-contained, interactive 3D
class-diagram page the generator produces — over HTTP, instead of writing it to a file.
It is written with **Boost.Beast** and takes the same input arguments as the generator,
plus a `--port`.

## Features

- **Serves the UML model over HTTP**: any `GET` request returns the full, self-contained
  viewer page (embedded three.js, no external assets)
- **Serves class source on demand**: `GET /source?path=...` returns the raw source of an
  allowlisted file, which powers the viewer's source pane (tabs + syntax highlighting)
- **Diff mode**: pass two JSON files to serve the older/newer comparison (added in green,
  removed in red) — exactly as the generator renders it
- **Same CLI as the generator**: `--hide <regex>` and the positional input files work
  identically, plus a new `--port`
- **Binds all interfaces** (`0.0.0.0`) and prints a LAN URL on startup
- **Fail-fast**: the page is built *before* the port is bound, so a bad input file never
  leaves a half-configured server listening
- **Safe source access**: the `/source` route is an allowlist, not a filesystem — only the
  exact set of class source files found in the input JSON can be returned, and the query is
  never used to build a path

## Building

The server is built as part of the main project. To build everything:

```bash
mkdir build && cd build
cmake ..
make
```

This creates the UML model server (`UmlServer`) alongside the code analyzer (`CodeAnalyzer`).

## Usage

### Basic Usage

```bash
# Serve a single analyzer JSON file on the default port (8000)
./UmlServer data.json

# Choose a port
./UmlServer --port 9000 data.json

# Serve a diff (older newer) on a chosen port
./UmlServer --port 9000 older.json newer.json
```

### Options

- `--port <n>`: TCP port to listen on (default `8000`; must be `1..65535`)
- `--hide <regex>`: Hide classes matching the specified regex pattern
- `-h, --help`: Show help message

### Input files

- **One file** → a single combined diagram.
- **Two files** → diff mode, in `older newer` order (added green, removed red).
- **More than two** → diff uses the first two; the rest are ignored (a warning is printed).

## Accessing

On startup the server prints the URLs to open:

```
Serving the UML model on port 8000 (all interfaces).
  Local:    http://localhost:8000/
  Network:  http://192.168.1.42:8000/
Press Ctrl-C to stop.
```

Open either URL in a browser, or fetch the page directly:

```bash
curl -sI http://localhost:8000/
# HTTP/1.1 200 OK
# Content-Type: text/html; charset=utf-8
# Content-Length: ...

# Diff mode: the served page contains the spliced old/new class data
curl -s http://localhost:9000/ | grep -o 'const DIFF_MODE = [a-z]*;'
# const DIFF_MODE = true;
```

Every path is served the same page (`GET /anything` also returns the model), **except the
`/source` route**. Non-`GET` methods (e.g. `POST`, `HEAD`) are rejected with `405`.

### The `/source` route

`GET /source?path=<file>` returns the raw source of a class file, `text/plain; charset=utf-8`.
The viewer's source pane uses it to populate its tabs on demand — a file is fetched only when
its tab is opened.

At startup the server collects every source file the classes in the input JSON reference
(the analyzer records each class's `file`) and **preloads those exact files into an
allowlist**. A request is answered only by exact string match against that map:

```bash
# An allowlisted file -> 200, the source itself
curl -sI 'http://localhost:8000/source?path=src/widget.cpp'
# HTTP/1.1 200 OK
# Content-Type: text/plain; charset=utf-8

# Anything not in the allowlist -> 404 (nothing is read from disk)
curl -sI 'http://localhost:8000/source?path=/etc/passwd'
# HTTP/1.1 404 Not Found

# No path= parameter -> 404
curl -sI 'http://localhost:8000/source'
# HTTP/1.1 404 Not Found
```

Because the `path` value is percent-decoded and then matched exactly — never joined into a
filesystem path — the query cannot escape the set of files the server was given (no path
traversal). A source file that is missing from disk is skipped at startup with a warning,
and the route 404s for it.

## Visualization Features

The served page is the same interactive viewer described in [README.uml.md](README.uml.md):
3D navigation (rotate/zoom/pan) with **w/a/s/d** movement keys and keyboard next/previous/center,
**Ctrl-Z** focus history (backwards, up to 30), name and namespace regex filters, closest-N
level-of-detail rendering so large diagrams stay responsive, double-click focus (a class, a
member's type, or an inheritance line's far end), a legend for classes, inheritance, and
method/variable connections — and a **resizable source pane** at the bottom showing the focused
class's source (with tabs for related classes' files and theme-matched syntax highlighting),
which is what the `/source` route feeds.

## Example

```bash
# 1. Run the code analyzer to produce JSON output
./CodeAnalyzer --json analysis.json /path/to/project

# 2. Serve the UML model over HTTP
./UmlServer --port 8000 analysis.json

# 3. Open http://localhost:8000/ in a browser
```
