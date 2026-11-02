# Simple WebGL2 example

This is the reference example for `canvas-host`. It uses only the core C++ API
and raw WebGL2:

1. attach a worker `OffscreenCanvas`
2. create a WebGL2 context
3. start the input event queue
4. poll and log pointer and keyboard events in a render loop
5. stop the loop, destroy the context, and close the canvas

The fragment shader draws a glow under the pointer. Clicking the canvas or
holding a key changes its color.

The context cell prints the type of each polled event with `std::cout`:

```text
pointerdown
pointerup
keydown
keyup
```

## Kernel

The worker canvas needs xeus-cpp built with `XEUS_CPP_EMSCRIPTEN_WEBGL=ON`.
This example builds the pinned
[compiler-research/xeus-cpp PR 505](https://github.com/compiler-research/xeus-cpp/pull/505)
commit `6b1503e96612b4871be659d3ad631b67a67c3131`.

The kernel build uses two environments because the tools and the wasm
packages target different platforms:

- `environment-wasm-build.yml`: host tools (`cmake`, Emscripten 4.0.9, Node.js,
  esbuild)
- `environment-wasm-host.yml`: wasm dependencies of xeus-cpp

The JupyterLite tooling only needs `environment.yml`, which installs
`jupyterlite-xeus`, `jupyterlite-core`, and `jupyter_server`.

`build_kernel.sh` also builds canvas-host from `recipe/` as a local wasm
conda package, publishes it into `.deps/channel`, and installs it into the
kernel prefix. The package contains the `canvas_host` headers plus both the
static and shared libraries, so JupyterLite includes them in the packed kernel
environment instead of dropping files installed only through CMake.

The in-browser JIT loads each cell as a side module with `RTLD_NOW`, so
`build_kernel.sh` links `libcanvas_host_c.a` into the `xcpp` main module and
exports its symbols. This lets notebook cells resolve
`canvas_host::attach_worker` and the rest of the C++ API when the incremental
module is loaded. The packaged `libcanvas_host_c.so` remains available for
consumers that load canvas-host as a JupyterLite side module.

## Build and serve

Create the JupyterLite environment:

```bash
mamba env create -f environment.yml
mamba activate canvas-host-jupyterlite
```

Build the xeus-cpp WebGL kernel and then the JupyterLite site:

```bash
./build_kernel.sh
./build.sh
python3 -m http.server 8040 --directory dist
```

`build_kernel.sh` creates local prefixes under `.deps/`; subsequent runs reuse
them. `build.sh` verifies the prepared prefix before packing it as the
JupyterLite kernel.

To reuse an existing xeus-cpp WebGL prefix:

```bash
XEUS_CPP_PREFIX=/path/to/xeus-cpp-wasm-host ./build.sh
```

Open:

```text
http://127.0.0.1:8040/lab/index.html?path=webgl.ipynb
```

Run the cells in order. The attach cell must finish rendering before the
context cell runs, because the browser transfers the canvas to the worker
asynchronously.

## Deployment

The dedicated `Pages` workflow in `.github/workflows/pages.yml` builds the
kernel and JupyterLite site and deploys `dist/` to GitHub Pages on pushes to
`master` or on manual dispatch.

Enable GitHub Pages with **Source: GitHub Actions** in the repository settings.
The deployed notebook is available at:

```text
https://<user>.github.io/<repository>/lab/index.html?path=webgl.ipynb
```

The workflow caches the wasm environments, the pinned xeus-cpp source, and the
local package cache, and adds `.nojekyll` before uploading the Pages artifact.
