# Simple WebGL2 example

This is the reference example for `canvas-host`. It uses only the core C++ API
and raw WebGL2:

1. attach a worker `OffscreenCanvas`
2. create a WebGL2 context
3. start the input event queue
4. poll pointer and keyboard events in a render loop
5. stop the loop, destroy the context, and close the canvas

The fragment shader draws a glow under the pointer. Clicking the canvas or
holding a key changes its color.

## Kernel

The worker canvas needs xeus-cpp built with `XEUS_CPP_EMSCRIPTEN_WEBGL=ON`.
This example builds the pinned
[compiler-research/xeus-cpp PR 505](https://github.com/compiler-research/xeus-cpp/pull/505)
commit `5c7c189c27834199199f42d10c441192f787bf5f`.

The kernel build uses two environments because the tools and the wasm
packages target different platforms:

- `environment-wasm-build.yml`: host tools (`cmake`, Emscripten 4.0.9, Node.js,
  esbuild)
- `environment-wasm-host.yml`: wasm dependencies of xeus-cpp

The JupyterLite tooling only needs `environment.yml`, which installs
`jupyterlite-xeus`; that pulls `jupyterlite-core` and `empack`.

`build_kernel.sh` also builds and installs the `<canvas_host/...>` headers and
`canvas_host_c` into the same wasm prefix, so notebook cells can include the
C++ API and link the library. `build.sh` verifies those files before packing
the site.

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

`build_kernel.sh` creates local prefixes under `.deps/`, clones and builds
xeus-cpp at the pinned PR commit, installs it into `.deps/wasm-host`, and then
builds and installs canvas-host into the same prefix. `build.sh` verifies the
prefix and packs it as the JupyterLite kernel.

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
