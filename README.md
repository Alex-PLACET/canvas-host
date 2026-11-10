# canvas-host

A backend-agnostic canvas host for **xeus-cpp / JupyterLite**.

`canvas-host` owns the browser-side canvas lifecycle and exposes a small C17 and C++17 API to
notebook cells. It does not render anything itself: applications receive a canvas, a WebGL2 context,
and normalized input events, then drive their own renderer.

**Live demo:** [Open the WebGL notebook on GitHub Pages](https://alex-placet.github.io/canvas-host/lab/index.html?path=webgl.ipynb)

## What it handles

- **Canvas lifecycle.** The library emits the `<canvas>` into notebook output, generates unique IDs,
  keeps CSS and backing-store sizes in sync, and releases browser channels and globals on stop.
- **Two hosting modes.** A worker canvas is transferred to the kernel as an `OffscreenCanvas`; a
  main-thread canvas stays in the page while a JavaScript application owns it. The kernel controls
  both through the same C++ API.
- **Graphics and input.** Worker canvases can be registered with Emscripten and turned into WebGL2
  contexts. Pointer, wheel, keyboard, focus, and blur events are normalized into JSON and queued for
  the kernel.
- **Status reporting.** Readiness, geometry, and errors are exposed to the kernel without blocking the
  interpreter thread.

## Requirements

- **Emscripten, xeus, and nlohmann_json** to build the C ABI library (`canvas_host_c`).
- **A C++23 compiler** for the implementation sources. The public headers remain C++17-compatible.
- **xeus-cpp with OffscreenCanvas/WebGL support** (`XEUS_CPP_EMSCRIPTEN_WEBGL`) for worker-side
  OpenGL.
- **Node.js 20+ and TypeScript 7.0.2** to build the browser runtime. `npm ci` installs the pinned
  compiler locally.
- **Catch2 3** for the host test suite.
- **JupyterLite and jupyterlite-xeus** to run the example notebook. The example section below points
  to its additional tooling.

Emscripten 4.0.9 and 6.0.8 are covered by CI.

## Build

### Host development

The host configuration builds the C++ implementation and tests without Emscripten or the C ABI:

```bash
mamba env create -f environment-dev.yml
mamba activate canvas-host-dev
npm --prefix js ci

cmake -S . -B build-dev -G Ninja \
  -DCANVAS_HOST_BUILD_TESTS=ON \
  -DCANVAS_HOST_BUILD_JS=ON \
  -DCANVAS_HOST_MINIFIER=OFF
cmake --build build-dev
ctest --test-dir build-dev --output-on-failure
```

`CANVAS_HOST_BUILD_JS=ON` adds the TypeScript runtime and its browser tests. Without it, CMake
generates an empty runtime header, which is enough for C++-only host work but skips the JavaScript
runtime tests.

### Emscripten build

Under Emscripten, the C ABI and browser runtime are enabled by default:

```bash
npm --prefix js ci
emcmake cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

CMake uses the local TypeScript compiler from `js/node_modules/.bin` when available and falls back
to `tsc` in `PATH`. If no supported minifier is installed, pass `-DCANVAS_HOST_MINIFIER=OFF`.

### Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `CANVAS_HOST_BUILD_C_API` | ON with Emscripten, OFF otherwise | Builds `canvas_host_c`; requires the JavaScript runtime. |
| `CANVAS_HOST_BUILD_JS` | Follows `CANVAS_HOST_BUILD_C_API` | Compiles and embeds the TypeScript runtime. |
| `CANVAS_HOST_MINIFIER` | `auto` | `auto` selects terser/esbuild; `OFF` embeds the unminified runtime. |
| `CANVAS_HOST_BUILD_TESTS` | OFF | Builds the Catch2 host tests. Emscripten builds ignore this option. |
| `CANVAS_HOST_SANITIZE` | OFF | Enables ASan/UBSan for the host tests. |

### Installed package

C and C++ consumers link the exported target:

```cmake
find_package(canvas-host CONFIG REQUIRED)
target_link_libraries(my_kernel PRIVATE canvas_host::canvas_host_c)
```

`canvas_host_c` is compiled as C++ and links xeus. A C executable should therefore use the C++
linker (`LINKER_LANGUAGE CXX`) or link through an existing C++ kernel target.

### Versioning

The top-level `VERSION` file is the only version source. CMake uses it for the project version and
generates `canvas_host/version.h` from `cmake/version.h.in` in the build directory. The generated
header is installed with the public headers; edit `VERSION` and rebuild rather than editing a
version header.

## Using the C++ API

The public header is `include/canvas_host/canvas_host.hpp`; its comments document every field and
method. The typical worker flow has two steps because the browser transfers the canvas
asynchronously.

Attach the canvas from one cell:

```cpp
#include <canvas_host/canvas_host.hpp>

canvas_host::WorkerConfig config;
config.width = 640;
config.height = 400;
config.status_text = "My canvas";

auto canvas = canvas_host::attach_worker(config);
```

Once the output has rendered, use the canvas from a later cell:

```cpp
canvas.require_ready();

int context = canvas_host::create_webgl2_context(canvas);
int width = canvas.pixel_width();
int height = canvas.pixel_height();

canvas.start_events();

canvas_host::nl::json event;
while (canvas.poll_event(event)) {
    const std::string type = event.value("type", std::string());
    // Update application state from the normalized event.
}

canvas.stop();
```

`require_ready()` does not wait. It throws until the transfer or initialization has completed, which
makes it safe to rerun the cell.

For a main-thread application, use `canvas_host::MainConfig` instead. The factory is loaded from
`script`, the canvas and geometry information are passed to it, and `start` can invoke an exported
application function:

```cpp
canvas_host::MainConfig app;
app.script = "/files/app.js";
app.factory = "createApp";
app.start = "app_start";
app.start_args = {"${canvasId}", 900};
app.start_arg_types = {"string", "number"};

auto canvas = canvas_host::attach_main(app);
```

`options` are merged into the factory arguments, `control_handler` receives messages sent with
`Canvas::send()`, and `on_start` runs after the start function. See the header for the remaining
fields.

### Logging

Diagnostics go to `config.log_stream`, which defaults to `std::cerr`:

```cpp
#include <sstream>

std::ostringstream log;
config.log_stream = &log;

auto canvas = canvas_host::attach_worker(config);
// log.str() now contains canvas-host diagnostics.
```

Set `config.log_stream = nullptr` to disable logging. The stream is not owned and must outlive the
`Canvas`.

## C17 API

`include/canvas_host/canvas_host.h` is valid C17 and C++. It exposes an opaque handle and is
implemented as an `extern "C"` adapter over the same C++ code. A minimal attach and event loop looks
like this:

```c
#include <canvas_host/canvas_host.h>
#include <stdio.h>

canvas_host_worker_config config = canvas_host_worker_config_default();
config.width = 640;
config.height = 400;

canvas_host_canvas* canvas = canvas_host_attach_worker(&config);
if (canvas == NULL) {
    fprintf(stderr, "%s\n", canvas_host_last_error());
    return;
}

char buffer[4096];
int result;
while ((result = canvas_host_poll_event(canvas, buffer, sizeof(buffer))) == 1) {
    printf("%s\n", buffer);
}
if (result < 0) {
    fprintf(stderr, "%s\n", canvas_host_error(canvas));
}

canvas_host_destroy(canvas);
```

`canvas_host_poll_event()` returns `1` for an event, `0` when the queue is empty, and `-1` for an
invalid buffer, a truncation, or an internal error. A truncated event is not consumed, so it can be
retried with a larger buffer.

`canvas_host_last_error()` reports attach failures; `canvas_host_error(handle)` reports errors for a
specific canvas. Geometry, WebGL, control-message, stop, and readiness functions mirror the C++ API.
Their contracts are documented in the header.

## Input events

Events are JSON objects. A pointer event looks like this:

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

All events carry `target`, `type`, and `eventTime`; the target identifies the canvas when several
are active. Pointer and wheel events include CSS and device coordinates plus the canvas geometry.
Keyboard events add `key`, `code`, `keyCode`, and `repeat`; wheel events add `dx`, `dy`, and
`deltaMode`; keypress events also include `charCode`. Supported types are `pointerdown`,
`pointermove`, `pointerup`, `pointercancel`, `wheel`, `keydown`, `keyup`, `keypress`, `focus`, and
`blur`.

`eventTime` is the DOM capture time in epoch milliseconds
(`performance.timeOrigin + performance.now()`).

## Behavior and limits

- `attach_worker()` and `attach_main()` require positive canvas dimensions. `attach_main()` also
  requires a factory and a JSON-object `options` value.
- `start_args` and `start_arg_types` must have the same length. Strings in `start_args` can
  interpolate `${canvasId}`, `${width}`, `${height}`, and `${dpr}`.
- Input events are queued in the Emscripten filesystem. The queue is capped at 1 MiB; new events are
  dropped while it is full, so poll regularly.
- `Canvas::send()` and `canvas_host_send_json()` require a JSON object and an attached canvas.
- The C and C++ APIs are not thread-safe. Call them from the interpreter/worker thread that owns the
  Emscripten runtime.

## Development checks

The host configuration above registers the C API test, the C++ wrapper test, and an
installed-package consumer test with CTest. The browser runtime tests are included when the runtime
is part of the build.

Run the JavaScript tests directly with:

```bash
npm --prefix js test
```

Enable ASan/UBSan by adding `-DCANVAS_HOST_SANITIZE=ON` to the host configuration. C/C++ sources are
formatted with the checked-in `.clang-format`; apply `clang-format` to `include/`, `src/`, and
`tests/` before committing.

CI runs the host tests, formatting and TypeScript type checks, and Emscripten builds for 4.0.9 and
6.0.8. The separate Pages workflow builds and deploys the WebGL example.

## JupyterLite example

[`examples/jupyterlite/webgl`](examples/jupyterlite/webgl) is the reference example. It builds the
pinned xeus-cpp WebGL kernel, packages canvas-host as a local wasm dependency, and produces a
JupyterLite site with a render loop, pointer input, and keyboard input. Its README covers the
required environments, build commands, and local serving instructions.

## Project layout

- `VERSION` — single library version source.
- `include/canvas_host/` — public C17 and C++17 headers.
- `src/` — implementation and the C ABI adapter.
- `js/` — TypeScript browser runtime and its Node tests.
- `cmake/` — runtime build helpers, package configuration, and the generated version-header template.
- `tests/` — Catch2 host tests and the installed-package consumer check.
- `examples/jupyterlite/webgl/` — reference notebook and its build tooling.

## License

BSD-3-Clause.
