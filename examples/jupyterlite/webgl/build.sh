#!/bin/bash
# Pack the prepared wasm prefix into a JupyterLite site and verify required files.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${HERE}"

PREFIX="${XEUS_CPP_PREFIX:-${HERE}/.deps/wasm-host}"

if [[ ! -d "${PREFIX}/share/jupyter/kernels" ]]; then
    echo "error: xeus-cpp WebGL kernel not found in ${PREFIX}" >&2
    echo "       Run ./build_kernel.sh first or set XEUS_CPP_PREFIX." >&2
    exit 1
fi

if [[ ! -f "${PREFIX}/include/canvas_host/canvas_host.hpp" ]]; then
    echo "error: canvas-host headers not found in ${PREFIX}/include" >&2
    echo "       Run ./build_kernel.sh or install canvas-host into the prefix." >&2
    exit 1
fi

if ! find "${PREFIX}/lib" -maxdepth 1 -name 'libcanvas_host_c*' -print -quit | grep -q .; then
    echo "error: canvas_host_c not found in ${PREFIX}/lib" >&2
    echo "       Run ./build_kernel.sh or install canvas-host into the prefix." >&2
    exit 1
fi

rm -rf "${HERE}/dist"
jupyter lite build \
    --XeusAddon.prefix="${PREFIX}" \
    --output-dir dist

if [[ ! -f "${HERE}/dist/xeus/kernels.json" ]]; then
    echo "error: xeus-cpp kernel is missing from dist/xeus/." >&2
    echo "       Install jupyterlite-xeus in the build environment and rerun." >&2
    exit 1
fi

if [[ ! -f "${HERE}/dist/files/webgl.ipynb" ]]; then
    echo "error: notebook is missing from dist/files/." >&2
    exit 1
fi

echo "Built ${HERE}/dist"
