# Host tests for the C ABI, C++ wrapper, package consumer, and browser runtime.

find_package(nlohmann_json CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

add_library(canvas_host_test_impl STATIC
    fakes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/canvas_host.cpp
    ${CANVAS_HOST_IMPL_SOURCES}
)

target_include_directories(canvas_host_test_impl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/fake_emscripten
        ${CMAKE_CURRENT_SOURCE_DIR}/fake_xeus
        ${CANVAS_HOST_GENERATED_DIR}
)
target_link_libraries(canvas_host_test_impl PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(canvas_host_test_impl PUBLIC cxx_std_23)
target_compile_options(canvas_host_test_impl PRIVATE
    -Wall
    -Wextra
    -Werror
    -pedantic-errors
)

set_target_properties(canvas_host_test_impl PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if(CANVAS_HOST_SANITIZE)
    target_compile_options(canvas_host_test_impl PUBLIC
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(canvas_host_test_impl INTERFACE
        -fsanitize=address,undefined
    )
endif()

add_executable(canvas_host_c_runtime_test test_c_api_runtime.cpp)
target_link_libraries(canvas_host_c_runtime_test PRIVATE
    canvas_host_test_impl
    Catch2::Catch2WithMain
)
target_compile_features(canvas_host_c_runtime_test PRIVATE cxx_std_23)
set_target_properties(canvas_host_c_runtime_test PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
target_compile_options(canvas_host_c_runtime_test PRIVATE
    -Wall
    -Wextra
    -Werror
    -pedantic-errors
)
add_test(NAME canvas_host_c_runtime_test COMMAND canvas_host_c_runtime_test)

add_executable(canvas_host_cpp_wrapper_test test_cpp_wrapper.cpp)
target_link_libraries(canvas_host_cpp_wrapper_test PRIVATE
    canvas_host_test_impl
    Catch2::Catch2WithMain
)
target_compile_features(canvas_host_cpp_wrapper_test PRIVATE cxx_std_23)
set_target_properties(canvas_host_cpp_wrapper_test PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
target_compile_options(canvas_host_cpp_wrapper_test PRIVATE
    -Wall
    -Wextra
    -Werror
    -pedantic-errors
)
add_test(NAME canvas_host_cpp_wrapper_test COMMAND canvas_host_cpp_wrapper_test)

add_test(NAME canvas_host_package_consumer_test
    COMMAND ${CMAKE_COMMAND}
        "-DCANVAS_HOST_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DCANVAS_HOST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../.."
        "-DCANVAS_HOST_INSTALL_DIR=${CMAKE_CURRENT_BINARY_DIR}/package-install"
        "-DCANVAS_HOST_NLOHMANN_DIR=${nlohmann_json_DIR}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../package/run.cmake
)

if(CANVAS_HOST_BUILD_JS)
    find_program(CANVAS_HOST_NODE_EXECUTABLE node)
    if(CANVAS_HOST_NODE_EXECUTABLE)
        add_test(NAME canvas_host_js_runtime_test
            COMMAND ${CANVAS_HOST_NODE_EXECUTABLE} --test
                ${CMAKE_CURRENT_SOURCE_DIR}/../../js/tests/canvas-host.test.mjs
        )
        set_tests_properties(canvas_host_js_runtime_test PROPERTIES
            ENVIRONMENT "CANVAS_HOST_JS=${CANVAS_HOST_COMPILED_JS}"
        )
    endif()
endif()
