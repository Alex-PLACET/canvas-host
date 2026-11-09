# canvas-host: build, test, and install the C ABI, C++ headers, and browser runtime.

cmake_minimum_required(VERSION 3.16)
project(canvas-host VERSION 0.2.0 LANGUAGES C CXX)

include(GNUInstallDirs)

if(EMSCRIPTEN)
    set(CANVAS_HOST_BUILD_C_API_DEFAULT ON)
else()
    set(CANVAS_HOST_BUILD_C_API_DEFAULT OFF)
endif()

option(CANVAS_HOST_BUILD_C_API
    "Build the C17 C ABI library"
    ${CANVAS_HOST_BUILD_C_API_DEFAULT}
)
option(CANVAS_HOST_BUILD_JS
    "Build and install the TypeScript browser runtime"
    ${CANVAS_HOST_BUILD_C_API}
)

# Minify the embedded browser runtime: OFF, auto, terser, or esbuild.
set(CANVAS_HOST_MINIFIER "auto" CACHE STRING
    "JavaScript minifier: OFF, auto, terser, or esbuild"
)
set_property(CACHE CANVAS_HOST_MINIFIER PROPERTY STRINGS
    OFF auto terser esbuild
)

set(CANVAS_HOST_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${CANVAS_HOST_GENERATED_DIR}")

set(CANVAS_HOST_IMPL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/canvas_host_detail.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/canvas_host_canvas.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/canvas_host_attach.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/canvas_host_webgl.cpp"
)

if(CANVAS_HOST_BUILD_C_API AND NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "canvas-host: CANVAS_HOST_BUILD_C_API requires the Emscripten toolchain. "
        "Pass -DCANVAS_HOST_BUILD_C_API=OFF to build the headers or host tests."
    )
endif()

if(CANVAS_HOST_BUILD_C_API AND NOT CANVAS_HOST_BUILD_JS)
    message(FATAL_ERROR
        "canvas-host: CANVAS_HOST_BUILD_C_API=ON requires "
        "CANVAS_HOST_BUILD_JS=ON so the browser runtime can be embedded."
    )
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/canvas-host-js.cmake")

if(CANVAS_HOST_BUILD_JS)
    canvas_host_build_runtime(
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CANVAS_HOST_GENERATED_DIR}"
        "${CANVAS_HOST_MINIFIER}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/canvas_host_js.hpp.in"
    )
else()
    # Host tests compile the implementation but do not need the browser runtime.
    file(WRITE "${CANVAS_HOST_GENERATED_DIR}/canvas_host_js.hpp"
        "#pragma once\n\n"
        "// Generated stub for host builds without the browser runtime.\n"
        "inline constexpr const char* canvas_host_js = \"\";\n"
    )
endif()

add_library(canvas_host INTERFACE)
add_library(canvas_host::canvas_host ALIAS canvas_host)

target_compile_features(canvas_host INTERFACE cxx_std_17)
target_include_directories(canvas_host INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(canvas_host INTERFACE nlohmann_json::nlohmann_json)

set(CANVAS_HOST_INSTALL_TARGETS canvas_host)

if(CANVAS_HOST_BUILD_C_API)
    # Emscripten does not report shared-library support by default, but xeus
    # exports its main target as SHARED. Allow CMake to import it.
    if(EMSCRIPTEN)
        set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
    endif()
    find_package(xeus CONFIG REQUIRED)

    # xeus exports --std=c++17 as an interface compile option, which overrides
    # the C++23 standard required by the implementation sources.
    foreach(_canvas_host_xeus_target xeus xeus-static)
        if(TARGET ${_canvas_host_xeus_target})
            get_target_property(
                _canvas_host_xeus_options
                ${_canvas_host_xeus_target}
                INTERFACE_COMPILE_OPTIONS
            )
            if(_canvas_host_xeus_options)
                list(FILTER _canvas_host_xeus_options
                    EXCLUDE REGEX "^-?-std=c\\+\\+[0-9]+$"
                )
                set_property(
                    TARGET ${_canvas_host_xeus_target}
                    PROPERTY INTERFACE_COMPILE_OPTIONS
                    "${_canvas_host_xeus_options}"
                )
            endif()
        endif()
    endforeach()

    # The implementation is C++; the C header is its extern "C" ABI.
    add_library(canvas_host_c STATIC
        src/canvas_host.cpp
        ${CANVAS_HOST_IMPL_SOURCES}
    )
    add_library(canvas_host::canvas_host_c ALIAS canvas_host_c)

    set_target_properties(canvas_host_c PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_features(canvas_host_c PRIVATE cxx_std_23)
    target_include_directories(canvas_host_c PRIVATE
        ${CANVAS_HOST_GENERATED_DIR}
    )
    target_link_libraries(canvas_host_c
        PUBLIC canvas_host
        PRIVATE nlohmann_json::nlohmann_json xeus
    )

    # xeus is built with wasm exceptions. These are link requirements for
    # consumers of the C ABI as well.
    target_link_options(canvas_host_c INTERFACE
        -sSUPPORT_LONGJMP=wasm
        -fwasm-exceptions
    )

    list(APPEND CANVAS_HOST_INSTALL_TARGETS canvas_host_c)
endif()

install(TARGETS ${CANVAS_HOST_INSTALL_TARGETS} EXPORT canvas-host-targets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/canvas-host)

if(CANVAS_HOST_BUILD_JS)
    install(FILES "${CANVAS_HOST_COMPILED_JS}"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/canvas-host
    )
    if(NOT CANVAS_HOST_RUNTIME_JS STREQUAL CANVAS_HOST_COMPILED_JS)
        install(FILES "${CANVAS_HOST_RUNTIME_JS}"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/canvas-host
        )
    endif()
endif()

install(EXPORT canvas-host-targets
    FILE canvas-host-targets.cmake
    NAMESPACE canvas_host::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/canvas-host
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/canvas-host-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/canvas-host-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/canvas-host-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/canvas-host
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/canvas-host-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/canvas-host-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/canvas-host
)

option(CANVAS_HOST_BUILD_TESTS "Build host tests" OFF)
option(CANVAS_HOST_SANITIZE "Enable ASan/UBSan for host tests" OFF)
if(CANVAS_HOST_BUILD_TESTS)
    if(EMSCRIPTEN)
        message(STATUS "canvas-host: host tests are disabled for Emscripten builds")
    else()
        enable_testing()
        add_subdirectory(tests/c)
    endif()
endif()
