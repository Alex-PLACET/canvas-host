# canvas-host

A backend-agnostic canvas host for **xeus-cpp / JupyterLite**.

`canvas-host` owns the browser-side canvas lifecycle and exposes a small C17
and C++17 API to notebook cells. It does not initialize SDL, sokol, bgfx,
raylib, ImGui, or any other renderer. Applications receive a canvas, a WebGL2
context, and normalized input events, then drive their own renderer.

## What it provides

- Emits a `<canvas>` in notebook output
- Fixed CSS size with an optional HiDPI backing store
- Unique canvas IDs for multiple canvases
- Worker `OffscreenCanvas` transfer
- Main-thread application hosting
- Emscripten canvas registration
- WebGL2 context creation
- Normalized pointer, wheel, keyboard, focus, and blur events
- Ready/error status and cleanup

## Requirements

- Emscripten, xeus, and nlohmann_json to build `canvas_host_c`
- A C++23 compiler for the implementation; public headers remain C++17-compatible
- xeus-cpp with OffscreenCanvas/WebGL support for worker GL
- JupyterLite + jupyterlite-xeus to run the example
- micromamba and git to build the example kernel
- Node.js 20 or newer and TypeScript 7.0.2 (`tsc`) to compile the browser runtime
- Catch2 3 for the host C++ tests
- An external JavaScript minifier when `CANVAS_HOST_MINIFIER` is not `OFF`
  (default `auto`)
- A kernel that installs the `include/canvas_host/` headers and links
  `canvas_host_c`

## Development environment

For host development and tests, create the conda-forge environment:

```bash
mamba env create -f environment-dev.yml
mamba activate canvas-host-dev
(cd js && npm ci)

cmake -S . -B build-dev \
  -DCANVAS_HOST_BUILD_TESTS=ON \
  -DCANVAS_HOST_BUILD_C_API=OFF \
  -DCANVAS_HOST_BUILD_JS=ON \
  -DCANVAS_HOST_MINIFIER=OFF
cmake --build build-dev
ctest --test-dir build-dev --output-on-failure
```

The environment includes Catch2, nlohmann_json, Node.js, esbuild, and
clang-tools. `CANVAS_HOST_BUILD_C_API=OFF` is for the host build; the C ABI
library itself is built with the Emscripten toolchain.

`CANVAS_HOST_BUILD_JS=OFF` skips TypeScript and minifier requirements and
generates an empty runtime header, which is enough for C++-only host tests.
Use `CANVAS_HOST_BUILD_JS=ON` to run the browser runtime tests through CTest.
`CANVAS_HOST_SANITIZE=ON` enables ASan/UBSan for the host tests.

## Build

The C ABI library is enabled by default under the Emscripten toolchain:

```bash
(cd js && npm ci)
emcmake cmake -S . -B build \
  -DCANVAS_HOST_BUILD_C_API=ON \
  -DCANVAS_HOST_BUILD_JS=ON
cmake --build build
```

`npm ci` installs the TypeScript compiler locally; CMake also finds a global
`tsc` in `PATH`. Emscripten builds do not build or run the host tests; use a
separate host configuration for `ctest`.

C and C++ consumers link the exported target:

```cmake
find_package(canvas-host CONFIG REQUIRED)
target_link_libraries(my_kernel PRIVATE canvas_host::canvas_host_c)
```

Because `canvas_host_c` is compiled as C++ and links xeus, a C executable
should use the C++ linker (`LINKER_LANGUAGE CXX`) or be linked from an existing
C++ kernel target.

### Minification

The browser runtime is compiled from TypeScript with `tsc`. The compiled
modules are concatenated into one runtime bundle, then embedded into
`canvas_host_c` at build time. `CANVAS_HOST_MINIFIER` controls minification:

- `CANVAS_HOST_MINIFIER=auto` (default) uses `terser` or `esbuild`
- `CANVAS_HOST_MINIFIER=terser|esbuild` selects one explicitly
- `CANVAS_HOST_MINIFIER=OFF` embeds the unminified compiled runtime

If minification is enabled and no supported tool is installed, CMake fails
with an explicit error. There is no bundled minifier.

`CANVAS_HOST_BUILD_JS=OFF` skips TypeScript/minification and generates an
empty runtime header. It is intended for header installation and C++-only host
tests; `CANVAS_HOST_BUILD_C_API=ON` requires the runtime and therefore requires
`CANVAS_HOST_BUILD_JS=ON`. When the runtime is built, CMake installs both the
unminified `canvas-host.js` and, when enabled, `canvas-host.min.js`.

```bash
# Example: use an installed terser
emcmake cmake -S . -B build \
  -DCANVAS_HOST_BUILD_C_API=ON \
  -DCANVAS_HOST_MINIFIER=terser
```

## C++ quick start

```cpp
#include <canvas_host/canvas_host.hpp>

canvas_host::WorkerConfig config;
config.width = 640;
config.height = 400;
config.status_text = "My canvas";

auto canvas = canvas_host::attach_worker(config);
```

In a later cell, after the canvas has been transferred to the worker:

```cpp
canvas.require_ready();

int context = canvas_host::create_webgl2_context(canvas);
int width = canvas.pixel_width();
int height = canvas.pixel_height();
double dpr = canvas.device_pixel_ratio();

canvas.start_events();

canvas_host::nl::json event;
while (canvas.poll_event(event)) {
    const std::string type = event.value("type", std::string());
    // Update application state from the normalized event.
}
```

Stop the loop and release the canvas:

```cpp
canvas.stop();
```

See `examples/jupyterlite/webgl` for a complete WebGL2 setup with a render loop
and pointer/keyboard handling.

## C17 API

`include/canvas_host/canvas_host.h` is valid C17 and C++. It exposes an opaque
`canvas_host_canvas*` handle:

```c
#include <canvas_host/canvas_host.h>
#include <stdio.h>

void attach_canvas(void)
{
    canvas_host_worker_config config = canvas_host_worker_config_default();
    config.width = 640;
    config.height = 400;

    canvas_host_canvas* canvas = canvas_host_attach_worker(&config);
    if (canvas == NULL) {
        fprintf(stderr, "%s\n", canvas_host_last_error());
        return;
    }
}
```

Drain events with `canvas_host_poll_event()`:

```c
char buffer[4096];
int result;

while ((result = canvas_host_poll_event(canvas, buffer, sizeof(buffer))) == 1) {
    printf("%s\n", buffer);
}

if (result < 0) {
    fprintf(stderr, "%s\n", canvas_host_error(canvas));
}
```

`canvas_host_poll_event()` returns `1` for an event, `0` when the queue is
empty, and `-1` on an invalid buffer or truncation. A truncated event is not
consumed, so it can be read again with a larger buffer.

Call `canvas_host_stop()` before `canvas_host_destroy()`:

```c
canvas_host_stop(canvas);
canvas_host_destroy(canvas);
```

## API summary

### C17

Header: `include/canvas_host/canvas_host.h`
Library: `canvas_host_c` (`canvas_host::canvas_host_c` in CMake)

- Config defaults: `canvas_host_worker_config_default()`,
  `canvas_host_main_config_default()`
- Attach/destroy: `canvas_host_attach_worker()`,
  `canvas_host_attach_main()`, `canvas_host_destroy()`
- Status: `canvas_host_ready()`, `canvas_host_has_error()`,
  `canvas_host_error()`, `canvas_host_last_error()`
- Geometry: `canvas_host_width()`, `canvas_host_height()`,
  `canvas_host_pixel_width()`, `canvas_host_pixel_height()`,
  `canvas_host_device_pixel_ratio()`
- Graphics: `canvas_host_register_canvas()`,
  `canvas_host_create_webgl2_context()`,
  `canvas_host_destroy_webgl2_context()`
- Events: `canvas_host_start_events()`, `canvas_host_poll_event()`
- Control: `canvas_host_send_json()`, `canvas_host_stop()`

### C++

Header: `include/canvas_host/canvas_host.hpp`
Library: `canvas_host_c`

- `attach_worker(config)`, `attach_main(config)`
- `Canvas::ready()`, `require_ready()`, `has_error()`, `error()`
- `Canvas::id()`, `global_name()`, `width()`, `height()`,
  `pixel_width()`, `pixel_height()`, `device_pixel_ratio()`
- `Canvas::register_canvas()`, `create_webgl2_context(canvas)`,
  `destroy_webgl2_context(handle)`
- `Canvas::start_events()`, `poll_event(line)`,
  `poll_event(line, max_size)`, `poll_event(nl::json&)`
- `Canvas::send(nl::json)`, `stop()`

## Behavior and limits

- `attach_worker()` requires positive `width` and `height`; `attach_main()`
  also requires a non-empty `factory` and a JSON-object `options`.
- `start_args` and `start_arg_types` must have the same length when types are
  supplied.
- `require_ready()` throws until the browser has transferred or initialized the
  canvas; it does not block or wait.
- Input events are queued in the Emscripten filesystem. The queue is capped at
  1 MiB; new events are dropped while the queue is full. Poll events regularly
  to avoid losing input.
- `Canvas::send()` and `canvas_host_send_json()` require a JSON object and an
  attached canvas.
- The C and C++ APIs are not thread-safe; call them from the interpreter/worker
  thread that owns the Emscripten runtime.
- Version macros are available in `canvas_host/version.h`.

## Input events

Events are JSON objects:

```json
{
  "target": "canvas-...",
  "type": "pointerdown",
  "eventTime": 1730000000123.4,
  "x": 320,
  "y": 200,
  "deviceX": 640,
  "deviceY": 400,
  "cssWidth": 640,
  "cssHeight": 400,
  "pixelWidth": 640,
  "pixelHeight": 400,
  "dpr": 1,
  "button": 0,
  "buttons": 1,
  "ctrl": false,
  "shift": false,
  "alt": false,
  "meta": false
}
```

Event types include `pointerdown`, `pointermove`, `pointerup`,
`pointercancel`, `wheel`, `keydown`, `keyup`, `keypress`, `focus`, and `blur`.
Keyboard events include `key`, `code`, `keyCode`, and `repeat`. Wheel events
include `dx`, `dy`, and `deltaMode`.

`eventTime` is the DOM capture time in epoch milliseconds
(`performance.timeOrigin + performance.now()`).

## Example

`examples/jupyterlite/webgl` is the reference example. It attaches a worker
canvas, creates a WebGL2 context, polls pointer and keyboard events, and draws a
simple full-screen shader.

Build and serve it from its directory:

```bash
cd examples/jupyterlite/webgl
mamba env create -f environment.yml
mamba activate canvas-host-jupyterlite
./build_kernel.sh
./build.sh
python3 -m http.server 8040 --directory dist
```

Open:

```text
http://127.0.0.1:8040/lab/index.html?path=webgl.ipynb
```

Worker WebGL requires xeus-cpp built with `XEUS_CPP_EMSCRIPTEN_WEBGL=ON`.
`build_kernel.sh` builds the pinned
[compiler-research/xeus-cpp PR 505](https://github.com/compiler-research/xeus-cpp/pull/505)
commit `6b1503e96612b4871be659d3ad631b67a67c3131` into `.deps/wasm-host`, then
builds and installs the `canvas_host_c` library and its headers into the same
prefix. The wasm build environment includes Node.js and esbuild for the
TypeScript runtime.

To reuse an existing xeus-cpp WebGL prefix, install `canvas_host_c` and the
`include/canvas_host/` headers into it first:

```bash
XEUS_CPP_PREFIX=/path/to/xeus-cpp-wasm-host ./build.sh
```

`build.sh` verifies that the prefix contains the kernel, headers, and
`canvas_host_c` before packing the JupyterLite site.

## Tests

On the host, the test targets compile the C++ implementation and C ABI adapter
against fake Emscripten and xeus headers. Both the C API and C++ wrapper tests
use Catch2 3. The suite also installs the package and builds a minimal
`find_package(canvas-host)` consumer, and runs the TypeScript runtime tests when
Node.js is available:

```bash
cmake -S . -B build \
  -DCANVAS_HOST_BUILD_TESTS=ON \
  -DCANVAS_HOST_BUILD_C_API=OFF \
  -DCANVAS_HOST_BUILD_JS=ON \
  -DCANVAS_HOST_MINIFIER=OFF \
  -DCMAKE_PREFIX_PATH="/path/to/catch2;/path/to/nlohmann_json"
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the browser runtime tests directly with:

```bash
(cd js && npm ci && npm test)
```

Tests are host-only; Emscripten builds do not build test executables. Use
`-DCANVAS_HOST_SANITIZE=ON` to enable ASan/UBSan on the host tests.

## Formatting

C/C++ sources are formatted with `clang-format` using the checked-in
`.clang-format` rules. Apply them to `include/`, `src/`, and `tests/` before
committing.

## Repository layout

```text
canvas-host/
├── include/canvas_host/canvas_host.h     # C17 API
├── include/canvas_host/canvas_host.hpp   # C++17 API
├── include/canvas_host/version.h         # Version macros
├── src/canvas_host.cpp                   # C ABI adapter
├── src/canvas_host_detail.cpp            # Escaping, files, HTML, status listener
├── src/canvas_host_canvas.cpp            # Canvas state, events, control, cleanup
├── src/canvas_host_attach.cpp            # attach_worker/attach_main and display
├── src/canvas_host_webgl.cpp             # WebGL2 context creation
├── src/canvas_host_detail.hpp            # Internal implementation declarations
├── js/attach.ts                          # Attach flow and configuration validation
├── js/constants.ts                       # Channel names, globals, and runtime constants
├── js/events.ts                          # DOM input capture and normalization
├── js/main.ts                            # Public globalThis.CanvasHost API
├── js/state.ts                           # Instance registry, status, control, cleanup
├── js/types.ts                           # Runtime interfaces and public contracts
├── js/utils.ts                           # Globals, validation, arguments, script loading
├── js/bundle.mjs                         # Concatenates compiled modules
├── js/tests/                             # Browser runtime tests
├── js/package.json                       # TypeScript dev dependency
├── js/tsconfig.json                      # TypeScript compiler settings
├── examples/jupyterlite/webgl/           # reference example
├── environment-dev.yml                   # conda-forge host dev environment
├── .clang-format                         # C/C++ formatting rules
├── tests/
│   ├── c/                                # Catch2 host tests
│   └── package/                          # Installed-package consumer test
└── CMakeLists.txt
```

## License

BSD-3-Clause.
