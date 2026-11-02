# Build the TypeScript browser runtime and embed it in a generated C++ header.
#
# Called from the top-level CMakeLists when CANVAS_HOST_BUILD_JS is enabled.
# Sets CANVAS_HOST_COMPILED_JS and CANVAS_HOST_RUNTIME_JS in the caller scope.

function(canvas_host_build_runtime source_dir generated_dir minifier template_file)
    find_program(CANVAS_HOST_TSC_EXECUTABLE
        NAMES tsc
        HINTS "${source_dir}/js/node_modules/.bin"
    )
    if(NOT CANVAS_HOST_TSC_EXECUTABLE)
        message(FATAL_ERROR
            "canvas-host: the TypeScript compiler (tsc) is required. "
            "Run `npm ci` in js/ or add tsc to PATH."
        )
    endif()
    find_program(CANVAS_HOST_NODE_EXECUTABLE NAMES node)
    if(NOT CANVAS_HOST_NODE_EXECUTABLE)
        message(FATAL_ERROR
            "canvas-host: Node.js is required to bundle the TypeScript runtime."
        )
    endif()

    set(ts_output_dir "${generated_dir}/ts")
    file(MAKE_DIRECTORY "${ts_output_dir}")
    set(compiled_js "${generated_dir}/canvas-host.js")
    execute_process(
        COMMAND "${CANVAS_HOST_TSC_EXECUTABLE}"
                -p "${source_dir}/js/tsconfig.json"
                --outDir "${ts_output_dir}"
        RESULT_VARIABLE tsc_result
        ERROR_VARIABLE tsc_error
    )
    if(NOT tsc_result EQUAL 0)
        message(FATAL_ERROR
            "canvas-host: TypeScript compilation failed: ${tsc_error}"
        )
    endif()
    if(NOT EXISTS "${ts_output_dir}/main.js")
        message(FATAL_ERROR
            "canvas-host: tsc did not create ${ts_output_dir}/main.js"
        )
    endif()

    execute_process(
        COMMAND "${CANVAS_HOST_NODE_EXECUTABLE}"
                "${source_dir}/js/bundle.mjs"
                "${ts_output_dir}"
                "${compiled_js}"
        RESULT_VARIABLE bundle_result
        ERROR_VARIABLE bundle_error
    )
    if(NOT bundle_result EQUAL 0)
        message(FATAL_ERROR
            "canvas-host: failed to bundle the TypeScript runtime: ${bundle_error}"
        )
    endif()
    if(NOT EXISTS "${compiled_js}")
        message(FATAL_ERROR
            "canvas-host: bundling did not create ${compiled_js}"
        )
    endif()

    set(runtime_js "${compiled_js}")
    if(NOT minifier STREQUAL "OFF")
        set(minifier_names terser esbuild)
        set(minifier_kind "")
        if(minifier STREQUAL "auto")
            foreach(name IN LISTS minifier_names)
                unset(minifier_executable CACHE)
                find_program(minifier_executable NAMES ${name})
                if(minifier_executable)
                    set(minifier_kind "${name}")
                    break()
                endif()
            endforeach()
            if(NOT minifier_kind)
                message(FATAL_ERROR
                    "canvas-host: CANVAS_HOST_MINIFIER=auto requires terser or "
                    "esbuild. Install one, select it explicitly, or pass "
                    "-DCANVAS_HOST_MINIFIER=OFF to embed the unminified runtime."
                )
            endif()
        elseif(minifier IN_LIST minifier_names)
            unset(minifier_executable CACHE)
            find_program(minifier_executable NAMES ${minifier})
            if(NOT minifier_executable)
                message(FATAL_ERROR
                    "canvas-host: ${minifier} was not found"
                )
            endif()
            set(minifier_kind "${minifier}")
        else()
            message(FATAL_ERROR
                "canvas-host: unknown CANVAS_HOST_MINIFIER value: ${minifier}"
            )
        endif()

        set(runtime_js "${generated_dir}/canvas-host.min.js")
        message(STATUS
            "canvas-host: minifying the compiled runtime with ${minifier_kind}"
        )
        if(minifier_kind STREQUAL "esbuild")
            set(minifier_args --minify "--outfile=${runtime_js}")
        else()
            set(minifier_args -c -m -o "${runtime_js}")
        endif()
        execute_process(
            COMMAND "${minifier_executable}" "${compiled_js}" ${minifier_args}
            RESULT_VARIABLE minify_result
            ERROR_VARIABLE minify_error
        )
        if(NOT minify_result EQUAL 0)
            message(FATAL_ERROR
                "canvas-host: failed to minify the compiled runtime with "
                "${minifier_kind}: ${minify_error}"
            )
        endif()
        if(NOT EXISTS "${runtime_js}")
            message(FATAL_ERROR
                "canvas-host: minifier did not create ${runtime_js}"
            )
        endif()
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${source_dir}/js/types.ts"
        "${source_dir}/js/constants.ts"
        "${source_dir}/js/utils.ts"
        "${source_dir}/js/events.ts"
        "${source_dir}/js/state.ts"
        "${source_dir}/js/attach.ts"
        "${source_dir}/js/main.ts"
        "${source_dir}/js/bundle.mjs"
        "${source_dir}/js/tsconfig.json"
        "${source_dir}/js/package.json"
        "${source_dir}/js/package-lock.json"
    )

    file(READ "${runtime_js}" canvas_host_js)
    configure_file(
        "${template_file}"
        "${generated_dir}/canvas_host_js.hpp"
        @ONLY
    )

    set(CANVAS_HOST_COMPILED_JS "${compiled_js}" PARENT_SCOPE)
    set(CANVAS_HOST_RUNTIME_JS "${runtime_js}" PARENT_SCOPE)
endfunction()
