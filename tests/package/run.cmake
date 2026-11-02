# Install canvas-host and configure a minimal consumer against the installed
# package. This verifies that the exported INTERFACE target carries the
# nlohmann_json dependency even when the C ABI is not built.

foreach(required
    CANVAS_HOST_BINARY_DIR
    CANVAS_HOST_SOURCE_DIR
    CANVAS_HOST_INSTALL_DIR
    CANVAS_HOST_NLOHMANN_DIR
)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing required variable: ${required}")
    endif()
endforeach()

set(consumer_source "${CANVAS_HOST_SOURCE_DIR}/tests/package/consumer")
set(consumer_build "${CANVAS_HOST_INSTALL_DIR}/consumer-build")

file(REMOVE_RECURSE "${CANVAS_HOST_INSTALL_DIR}" "${consumer_build}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${CANVAS_HOST_BINARY_DIR}"
            --prefix "${CANVAS_HOST_INSTALL_DIR}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "canvas-host install failed:\n${install_output}\n${install_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -S "${consumer_source}"
            -B "${consumer_build}"
            "-Dnlohmann_json_DIR=${CANVAS_HOST_NLOHMANN_DIR}"
            "-DCMAKE_PREFIX_PATH=${CANVAS_HOST_INSTALL_DIR}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "consumer configure failed:\n${configure_output}\n${configure_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_build}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "consumer build failed:\n${build_output}\n${build_error}")
endif()
