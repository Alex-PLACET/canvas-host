#!/bin/bash
# Build the pinned xeus-cpp WebGL kernel and the canvas-host wasm package.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${HERE}"

DEPS="${HERE}/.deps"
SRC="${XEUS_CPP_SRC:-${DEPS}/xeus-cpp}"
BUILD="${DEPS}/xeus-cpp-build"
BUILD_ENV="${XEUS_CPP_BUILD_PREFIX:-${DEPS}/wasm-build}"
HOST_ENV="${XEUS_CPP_HOST_PREFIX:-${DEPS}/wasm-host}"
CANVAS_HOST_CHANNEL="${DEPS}/channel"
CANVAS_HOST_RECIPE="${HERE}/recipe/recipe.yaml"
export RATTLER_CACHE_DIR="${RATTLER_CACHE_DIR:-${DEPS}/rattler-cache}"

MAMBA="${MAMBA:-micromamba}"
REPO="${XEUS_CPP_REPO:-https://github.com/compiler-research/xeus-cpp.git}"
PR="${XEUS_CPP_PR:-505}"
# Head commit of compiler-research/xeus-cpp PR 505.
REV="${XEUS_CPP_REV:-6b1503e96612b4871be659d3ad631b67a67c3131}"

NPROC="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

if ! command -v "${MAMBA}" >/dev/null 2>&1; then
    echo "error: ${MAMBA} was not found; install micromamba or set MAMBA" >&2
    exit 1
fi

if [[ ! -d "${BUILD_ENV}" ]]; then
    "${MAMBA}" create -y -p "${BUILD_ENV}" \
        -f "${HERE}/environment-wasm-build.yml"
fi

if [[ ! -x "${BUILD_ENV}/bin/rattler-build" ]]; then
    "${MAMBA}" install -y -p "${BUILD_ENV}" \
        -c conda-forge rattler-build
fi

# Build canvas-host as a local wasm package so empack includes it in the
# JupyterLite kernel environment.
rm -rf "${CANVAS_HOST_CHANNEL}"
"${BUILD_ENV}/bin/rattler-build" build \
    --recipe "${CANVAS_HOST_RECIPE}" \
    --target-platform emscripten-wasm32 \
    --output-dir "${CANVAS_HOST_CHANNEL}" \
    -c https://prefix.dev/emscripten-forge-4x \
    -c https://prefix.dev/conda-forge

if [[ ! -d "${HOST_ENV}" ]]; then
    "${MAMBA}" create -y -p "${HOST_ENV}" \
        -f "${HERE}/environment-wasm-host.yml" \
        --platform=emscripten-wasm32 \
        -c "${CANVAS_HOST_CHANNEL}" \
        -c https://prefix.dev/emscripten-forge-4x \
        -c https://prefix.dev/conda-forge
fi

# Ensure an existing cached prefix receives the current local package. Remove
# first: micromamba's solver does not reinstall a local package with the same
# name/version/build from a file channel.
"${MAMBA}" remove -y -p "${HOST_ENV}" canvas-host >/dev/null 2>&1 || true
"${MAMBA}" install -y --platform=emscripten-wasm32 -p "${HOST_ENV}" \
    -c "${CANVAS_HOST_CHANNEL}" \
    -c https://prefix.dev/emscripten-forge-4x \
    -c https://prefix.dev/conda-forge \
    canvas-host

# The JIT loads each cell as a side module with RTLD_NOW. Link canvas_host into
# the main kernel and export its symbols so incremental modules can resolve
# canvas_host::attach_worker and the rest of the C++ API.
CANVAS_HOST_LINK_FLAGS="-Wl,--whole-archive ${HOST_ENV}/lib/libcanvas_host_c.a -Wl,--no-whole-archive -sEXPORT_ALL=1"

if [[ ! -d "${SRC}/.git" ]]; then
    mkdir -p "${SRC}"
    git init -q "${SRC}"
    git -C "${SRC}" remote add origin "${REPO}"
else
    git -C "${SRC}" remote set-url origin "${REPO}"
fi

if [[ -n "${REV}" ]]; then
    # Fetch the pinned commit directly so the build stays reproducible even if
    # the pull request head moves.
    if ! git -C "${SRC}" fetch --depth 1 origin "${REV}"; then
        git -C "${SRC}" fetch origin "refs/pull/${PR}/head"
    fi
    git -C "${SRC}" checkout -q "${REV}"
else
    git -C "${SRC}" fetch --depth 1 origin "refs/pull/${PR}/head"
    git -C "${SRC}" checkout -q FETCH_HEAD
fi

ACTUAL_REV="$(git -C "${SRC}" rev-parse HEAD)"
if [[ -n "${REV}" && "${ACTUAL_REV}" != "${REV}" ]]; then
    echo "error: checked out ${ACTUAL_REV}, expected ${REV}" >&2
    exit 1
fi

rm -rf "${BUILD}"
PATH="${BUILD_ENV}/bin:${PATH}" emcmake cmake \
    -S "${SRC}" \
    -B "${BUILD}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${HOST_ENV}" \
    -DCMAKE_FIND_ROOT_PATH="${HOST_ENV}" \
    -DSYSROOT_PATH="${BUILD_ENV}/opt/emsdk/upstream/emscripten/cache/sysroot" \
    -DXEUS_CPP_EMSCRIPTEN_WEBGL=ON \
    -DXEUS_CPP_BUILD_TESTS=OFF \
    -DCMAKE_EXE_LINKER_FLAGS="${CANVAS_HOST_LINK_FLAGS} ${CMAKE_EXE_LINKER_FLAGS:-}"

PATH="${BUILD_ENV}/bin:${PATH}" cmake --build "${BUILD}" \
    --target install \
    -j "${NPROC}"

if ! grep -q 'offscreenCanvases' "${HOST_ENV}/bin/xcpp.js"; then
    echo "error: the built xcpp.js does not export OffscreenCanvas support" >&2
    exit 1
fi

if [[ ! -f "${HOST_ENV}/include/canvas_host/canvas_host.hpp" ]]; then
    echo "error: canvas-host headers were not installed into ${HOST_ENV}" >&2
    exit 1
fi
if ! find "${HOST_ENV}/lib" -maxdepth 1 -name 'libcanvas_host_c*' -print -quit | grep -q .; then
    echo "error: canvas_host_c was not installed into ${HOST_ENV}/lib" >&2
    exit 1
fi

echo "Built xeus-cpp ${ACTUAL_REV} with WebGL support in ${HOST_ENV}"
echo "Installed canvas-host headers and canvas_host_c in ${HOST_ENV}"
