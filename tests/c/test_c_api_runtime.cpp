// Tests for the C17 ABI adapter.

#include <canvas_host/canvas_host.h>

#include "test_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstring>
#include <string>

extern "C"
{
const char* test_last_html(void);
const char* test_last_script(void);
void test_reset(void);

extern int test_display_should_fail;
extern int test_script_int_result;
extern const char* test_script_string_result;
extern int test_webgl_destroyed;
}

TEST_CASE("C API manages worker and main-thread canvases", "[canvas_host]")
{
    canvas_host_worker_config worker = canvas_host_worker_config_default();
    canvas_host_main_config app = canvas_host_main_config_default();
    char path[256];
    char event[128];

    worker.width = 800;
    worker.height = 600;
    worker.status_text = "C canvas";
    worker.canvas_id = "c-worker";

    test_script_int_result = 0;
    test_reset();
    canvas_host_canvas* canvas = canvas_host_attach_worker(&worker);
    REQUIRE(canvas != nullptr);

    const char* html = test_last_html();
    REQUIRE(contains(html, "<canvas id=\"c-worker\""));
    REQUIRE(contains(html, "\"mode\":\"worker\""));
    REQUIRE(contains(html, "\"id\":\"c-worker\""));
    REQUIRE(contains(html, "\"globalName\":\"__canvas_host_canvas_c-worker\""));
    REQUIRE(contains(html, "\"width\":800"));
    REQUIRE(contains(html, "\"height\":600"));
    REQUIRE(contains(html, "\"hidpi\":true"));
    REQUIRE(contains(html, "\"statusText\":\"C canvas\""));
    REQUIRE(contains(html, "globalThis.CanvasHost"));
    REQUIRE(contains(html, "role=\"status\""));
    REQUIRE(contains(html, "aria-live=\"polite\""));
    REQUIRE(contains(html, "aria-label=\"Interactive canvas\""));
    REQUIRE_FALSE(contains(html, "outline:none"));
    REQUIRE_FALSE(contains(html, "script.src = '"));
    REQUIRE(contains(test_last_script(), "__canvas_host_status_channel_c-worker"));
    REQUIRE(std::strcmp(canvas_host_id(canvas), "c-worker") == 0);
    REQUIRE(std::strcmp(canvas_host_global_name(canvas), "__canvas_host_canvas_c-worker") == 0);
    REQUIRE(canvas_host_width(canvas) == 800);
    REQUIRE(canvas_host_height(canvas) == 600);

    test_script_int_result = 1600;
    REQUIRE(canvas_host_pixel_width(canvas) == 1600);
    REQUIRE(canvas_host_device_pixel_ratio(canvas) == 2.0);
    test_script_int_result = 1200;
    REQUIRE(canvas_host_pixel_height(canvas) == 1200);
    test_script_int_result = 0;
    REQUIRE(canvas_host_pixel_width(canvas) == 800);

    test_display_should_fail = 1;
    canvas_host_canvas* failed = canvas_host_attach_worker(nullptr);
    REQUIRE(failed == nullptr);
    REQUIRE(contains(canvas_host_last_error(), "deliberate display failure"));
    test_display_should_fail = 0;

    test_script_int_result = 1;
    REQUIRE(canvas_host_ready(canvas));
    test_script_int_result = 0;
    REQUIRE_FALSE(canvas_host_ready(canvas));

    test_script_int_result = 1;
    REQUIRE(canvas_host_register_canvas(canvas));
    REQUIRE(contains(test_last_script(), "Module.GL.offscreenCanvases"));
    REQUIRE(contains(test_last_script(), "c-worker"));
    REQUIRE(contains(test_last_script(), "/tmp/canvas-host-error-c-worker"));

    REQUIRE(canvas_host_start_events(canvas));
    REQUIRE(contains(test_last_script(), "canvas-host-events"));
    REQUIRE(contains(test_last_script(), "/tmp/canvas-host-error-c-worker"));
    REQUIRE(contains(test_last_script(), "flags: 'a'"));
    REQUIRE(contains(test_last_script(), "Module.FS.stat"));
    REQUIRE(contains(test_last_script(), "1048576"));

    std::snprintf(path, sizeof(path), "/tmp/canvas-host-events-%s", canvas_host_id(canvas));
    FILE* file = std::fopen(path, "wb");
    REQUIRE(file != nullptr);
    std::fputs("{\"type\":\"pointerdown\"}\n{\"type\":\"pointerup\"}\n", file);
    std::fclose(file);

    REQUIRE(canvas_host_poll_event(canvas, event, sizeof(event)) == 1);
    REQUIRE(std::strcmp(event, "{\"type\":\"pointerdown\"}") == 0);
    REQUIRE(canvas_host_poll_event(canvas, event, sizeof(event)) == 1);
    REQUIRE(std::strcmp(event, "{\"type\":\"pointerup\"}") == 0);
    REQUIRE(canvas_host_poll_event(canvas, event, sizeof(event)) == 0);

    file = std::fopen(path, "wb");
    REQUIRE(file != nullptr);
    std::fputs("{\"type\":\"pointerdown\"}\n{\"type\":\"pointerup\"}\n", file);
    std::fclose(file);
    REQUIRE(canvas_host_poll_event(canvas, event, 8) == -1);
    REQUIRE(contains(canvas_host_error(canvas), "buffer"));
    REQUIRE(canvas_host_poll_event(canvas, event, sizeof(event)) == 1);
    REQUIRE(std::strcmp(event, "{\"type\":\"pointerdown\"}") == 0);
    REQUIRE(canvas_host_poll_event(canvas, event, sizeof(event)) == 1);
    REQUIRE(std::strcmp(event, "{\"type\":\"pointerup\"}") == 0);
    REQUIRE(canvas_host_poll_event(canvas, event, sizeof(event)) == 0);
    std::remove(path);

    test_script_int_result = 1;
    test_script_string_result = "deliberate JS error";
    REQUIRE(canvas_host_has_error(canvas));
    REQUIRE(std::strcmp(canvas_host_error(canvas), "canvas-host: deliberate JS error") == 0);
    test_script_string_result = "";

    test_reset();
    REQUIRE(canvas_host_send_json(canvas, "{\"type\":\"plot\",\"expr\":\"x\"}"));
    REQUIRE(contains(test_last_script(), "\"target\":\"c-worker\""));
    REQUIRE(contains(test_last_script(), "\"type\":\"plot\""));
    REQUIRE_FALSE(canvas_host_send_json(canvas, "[]"));
    REQUIRE(contains(canvas_host_error(canvas), "object"));

    test_script_int_result = 1;
    const int context = canvas_host_create_webgl2_context(canvas, 0, 1, 1, 0, 1);
    REQUIRE(context == 42);
    canvas_host_destroy_webgl2_context(canvas, context);
    REQUIRE(test_webgl_destroyed);

    test_reset();
    canvas_host_stop(canvas);
    REQUIRE(contains(test_last_script(), "__canvas_host_status_channel_c-worker"));
    REQUIRE(contains(test_last_script(), "__canvas_host_ready_c-worker"));
    REQUIRE(contains(test_last_script(), "__canvas_host_error_c-worker"));
    REQUIRE(contains(test_last_script(), "/tmp/canvas-host-events-c-worker"));
    REQUIRE(contains(test_last_script(), "/tmp/canvas-host-error-c-worker"));

    file = std::fopen(path, "wb");
    REQUIRE(file != nullptr);
    std::fputs("{\"type\":\"stale\"}\n", file);
    std::fclose(file);
    REQUIRE(canvas_host_poll_event(canvas, event, sizeof(event)) == 0);
    std::remove(path);
    canvas_host_destroy(canvas);

    app.canvas_id = "c-app";
    app.script = "/files/app.js";
    app.factory = "createApp";
    app.start = "app_start";
    const char* args[] = {"\"${canvasId}\"", "900"};
    const char* types[] = {"string", "number"};
    app.start_args = args;
    app.start_args_count = 2;
    app.start_arg_types = types;
    app.start_arg_types_count = 2;

    test_reset();
    canvas_host_canvas* main_canvas = canvas_host_attach_main(&app);
    REQUIRE(main_canvas != nullptr);

    html = test_last_html();
    REQUIRE(contains(html, "\"mode\":\"main\""));
    REQUIRE(contains(html, "\"id\":\"c-app\""));
    REQUIRE(contains(html, "\"script\":\"/files/app.js\""));
    REQUIRE(contains(html, "\"startArgs\":[\"${canvasId}\",900]"));
    REQUIRE(contains(html, "\"startArgTypes\":[\"string\",\"number\"]"));
    REQUIRE(contains(html, "\"appGlobal\":\"__canvas_host_app\""));
    REQUIRE(contains(html, "\"hidpi\":true"));
    REQUIRE(std::strcmp(canvas_host_global_name(main_canvas), "") == 0);
    REQUIRE(canvas_host_width(main_canvas) == 900);
    REQUIRE(canvas_host_height(main_canvas) == 600);

    test_script_int_result = 1800;
    REQUIRE(canvas_host_pixel_width(main_canvas) == 1800);
    REQUIRE(canvas_host_device_pixel_ratio(main_canvas) == 2.0);
    test_script_int_result = 0;
    REQUIRE(canvas_host_pixel_width(main_canvas) == 900);

    test_script_int_result = 1;
    std::snprintf(path, sizeof(path), "/tmp/canvas-host-error-%s", canvas_host_id(main_canvas));
    file = std::fopen(path, "wb");
    REQUIRE(file != nullptr);
    std::fputs("deliberate error", file);
    std::fclose(file);
    REQUIRE(canvas_host_has_error(main_canvas));
    REQUIRE(std::strcmp(canvas_host_error(main_canvas), "canvas-host: deliberate error") == 0);
    std::remove(path);

    canvas_host_destroy(main_canvas);
}

TEST_CASE("C API validates main-thread configuration", "[canvas_host]")
{
    canvas_host_main_config app = canvas_host_main_config_default();
    app.canvas_id = "invalid-main";

    REQUIRE(canvas_host_attach_main(&app) == nullptr);
    REQUIRE(contains(canvas_host_last_error(), "factory"));

    app.factory = "createApp";
    app.options_json = "[]";
    REQUIRE(canvas_host_attach_main(&app) == nullptr);
    REQUIRE(contains(canvas_host_last_error(), "object"));

    app.options_json = nullptr;
    const char* args[] = {"1"};
    const char* types[] = {"number", "number"};
    app.start_args = args;
    app.start_args_count = 1;
    app.start_arg_types = types;
    app.start_arg_types_count = 2;
    REQUIRE(canvas_host_attach_main(&app) == nullptr);
    REQUIRE(contains(canvas_host_last_error(), "same length"));

    app.start_arg_types = nullptr;
    app.start_arg_types_count = 0;
    REQUIRE(canvas_host_attach_main(&app) == nullptr);
    REQUIRE(contains(canvas_host_last_error(), "same length"));
}

TEST_CASE("C API rejects invalid canvas dimensions", "[canvas_host]")
{
    canvas_host_worker_config worker = canvas_host_worker_config_default();
    worker.width = -1;
    REQUIRE(canvas_host_attach_worker(&worker) == nullptr);
    REQUIRE(contains(canvas_host_last_error(), "width"));

    worker = canvas_host_worker_config_default();
    worker.height = -1;
    REQUIRE(canvas_host_attach_worker(&worker) == nullptr);
    REQUIRE(contains(canvas_host_last_error(), "height"));
}
