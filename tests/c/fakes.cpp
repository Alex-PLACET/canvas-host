// Host-side fakes for Emscripten and xeus. The canvas-host implementation calls
// browser APIs and xeus::display_data even in the host test build.
#include <emscripten.h>
#include <emscripten/html5.h>

#include <xeus/xinterpreter.hpp>

#include <nlohmann/json.hpp>

#include <cstring>
#include <stdexcept>
#include <string>

namespace
{

std::string g_last_script;
std::string g_html;

} // namespace

extern "C"
{

int test_display_should_fail = 0;
int test_script_int_result = 0;
const char* test_script_string_result = "";
int test_webgl_destroyed = 0;
int test_webgl_create_result = 42;
int test_webgl_make_current_result = EMSCRIPTEN_RESULT_SUCCESS;

const char* test_last_html(void) { return g_html.c_str(); }

const char* test_last_script(void) { return g_last_script.c_str(); }

void test_reset(void)
{
    g_last_script.clear();
    test_webgl_destroyed = 0;
}

double emscripten_get_now(void) { return 1234.0; }

void emscripten_run_script(const char* script) { g_last_script = script ? script : ""; }

int emscripten_run_script_int(const char* script)
{
    g_last_script = script ? script : "";
    return test_script_int_result;
}

char* emscripten_run_script_string(const char* script)
{
    (void)script;
    return const_cast<char*>(test_script_string_result);
}

void emscripten_webgl_init_context_attributes(EmscriptenWebGLContextAttributes* attributes)
{
    std::memset(attributes, 0, sizeof(*attributes));
}

EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_create_context(
    const char* target, const EmscriptenWebGLContextAttributes* attributes)
{
    (void)target;
    (void)attributes;
    return test_webgl_create_result;
}

int emscripten_webgl_make_context_current(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context)
{
    (void)context;
    return test_webgl_make_current_result;
}

void emscripten_webgl_destroy_context(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context)
{
    (void)context;
    test_webgl_destroyed = 1;
}

} // extern "C"

namespace xeus
{

void xinterpreter::display_data(nlohmann::json data,
                                nlohmann::json metadata,
                                nlohmann::json transient)
{
    (void)metadata;
    (void)transient;
    if (test_display_should_fail)
    {
        throw std::runtime_error("deliberate display failure");
    }
    g_html = data.value("text/html", std::string());
}

xinterpreter& get_interpreter()
{
    static xinterpreter instance;
    return instance;
}

} // namespace xeus
