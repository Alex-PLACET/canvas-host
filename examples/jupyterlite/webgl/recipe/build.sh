#!/bin/bash
# Build the canvas-host wasm package and its JupyterLite side module.
set -euxo pipefail

export SYSROOT_PATH="${EMSCRIPTEN_FORGE_EMSDK_DIR}/upstream/emscripten/cache/sysroot"
export CXXFLAGS="${CXXFLAGS:-} -fPIC"

# TypeScript 7 is installed through the local npm lockfile.
(cd js && npm ci)

emcmake cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DCMAKE_FIND_ROOT_PATH="${PREFIX}" \
    -DCMAKE_PREFIX_PATH="${PREFIX}" \
    -DSYSROOT_PATH="${SYSROOT_PATH}" \
    -DCANVAS_HOST_BUILD_C_API=ON \
    -DCANVAS_HOST_BUILD_JS=ON \
    -DCANVAS_HOST_MINIFIER=OFF \
    -DCANVAS_HOST_BUILD_TESTS=OFF

emmake make -C build -j"${CPU_COUNT}" install

# Build the side module shipped for consumers that load canvas-host dynamically.
emcc "${PREFIX}/lib/libcanvas_host_c.a" \
    ${EM_FORGE_SIDE_MODULE_LDFLAGS:--sSIDE_MODULE=1 -sERROR_ON_UNDEFINED_SYMBOLS=0} \
    -fwasm-exceptions \
    -sSUPPORT_LONGJMP=wasm \
    -o "${PREFIX}/lib/libcanvas_host_c.so"
