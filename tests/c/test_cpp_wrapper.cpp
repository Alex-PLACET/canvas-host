// Tests for the C++ wrapper API.

#include <canvas_host/canvas_host.hpp>

#include "test_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>

extern "C"
{
const char* test_last_html(void);
const char* test_last_script(void);
void test_reset(void);

extern int test_script_int_result;
extern int test_webgl_destroyed;
extern int test_webgl_create_result;
extern int test_webgl_make_current_result;
}

TEST_CASE("C++ wrapper manages a worker canvas", "[canvas_host]")
{
    test_script_int_result = 0;
    test_reset();

    canvas_host::WorkerConfig worker;
    worker.width = 800;
    worker.height = 600;
    worker.status_text = "C++ canvas";
    worker.canvas_id = "cpp-worker";

    auto canvas = canvas_host::attach_worker(worker);

    REQUIRE(canvas.id() == "cpp-worker");
    REQUIRE(canvas.global_name() == "__canvas_host_canvas_cpp-worker");
    REQUIRE(canvas.width() == 800);
    REQUIRE(canvas.height() == 600);
    REQUIRE(canvas.hidpi());
    REQUIRE(canvas.is_worker());
    REQUIRE_FALSE(canvas.is_main());

    std::string html = test_last_html();
    REQUIRE(contains(html, "\"mode\":\"worker\""));
    REQUIRE(contains(html, "\"id\":\"cpp-worker\""));
    REQUIRE(contains(html, "\"statusText\":\"C++ canvas\""));

    test_script_int_result = 1;
    REQUIRE(canvas.ready());
    REQUIRE_NOTHROW(canvas.require_ready());
    REQUIRE_NOTHROW(canvas.register_canvas());
    REQUIRE(contains(test_last_script(), "Module.GL.offscreenCanvases"));

    const int context = canvas_host::create_webgl2_context(canvas);
    REQUIRE(context == 42);
    canvas.destroy_webgl2_context(context);
    REQUIRE(test_webgl_destroyed);

    REQUIRE_NOTHROW(canvas.start_events());
    const std::string path = "/tmp/canvas-host-events-" + canvas.id();
    FILE* file = fopen(path.c_str(), "wb");
    REQUIRE(file != nullptr);
    fputs("{\"type\":\"pointerdown\"}\n", file);
    fclose(file);

    std::string line;
    REQUIRE_THROWS_AS(canvas.poll_event(line, 8), std::runtime_error);
    REQUIRE(canvas.poll_event(line));
    REQUIRE(line == "{\"type\":\"pointerdown\"}");
    REQUIRE_FALSE(canvas.poll_event(line));

    file = fopen(path.c_str(), "wb");
    REQUIRE(file != nullptr);
    fputs("{\"type\":\"pointerup\"}\n", file);
    fclose(file);

    canvas_host::nl::json event;
    REQUIRE(canvas.poll_event(event));
    REQUIRE(event.value("type", "") == "pointerup");
    REQUIRE_FALSE(canvas.poll_event(event));
    remove(path.c_str());

    test_reset();
    canvas.send(canvas_host::nl::json{{"type", "plot"}, {"expr", "x"}});
    REQUIRE(contains(test_last_script(), "\"target\":\"cpp-worker\""));
    REQUIRE(contains(test_last_script(), "\"type\":\"plot\""));

    test_script_int_result = 0;
    test_reset();

    canvas_host::MainConfig app;
    app.canvas_id = "cpp-app";
    app.script = "/files/app.js";
    app.factory = "createApp";
    app.start = "app_start";
    app.start_args = {"${canvasId}", 900, true};
    app.start_arg_types = {"string", "number", "boolean"};
    app.options = {{"theme", "dark"}};

    auto main_canvas = canvas_host::attach_main(app);
    REQUIRE(main_canvas.is_main());
    REQUIRE(main_canvas.id() == "cpp-app");
    REQUIRE(main_canvas.global_name().empty());
    REQUIRE(main_canvas.width() == 900);
    REQUIRE(main_canvas.height() == 600);

    html = test_last_html();
    REQUIRE(contains(html, "\"mode\":\"main\""));
    REQUIRE(contains(html, "\"startArgs\":[\"${canvasId}\",900,true]"));
    REQUIRE(contains(html, "\"startArgTypes\":[\"string\",\"number\",\"boolean\"]"));
    REQUIRE(contains(html, "\"options\":{\"theme\":\"dark\"}"));

    file = fopen(path.c_str(), "wb");
    REQUIRE(file != nullptr);
    fputs("{\"type\":\"stale\"}\n", file);
    fclose(file);

    canvas.stop();
    REQUIRE_FALSE(canvas.poll_event(line));
    remove(path.c_str());
    main_canvas.stop();

    canvas_host::WorkerConfig special;
    special.canvas_id = "x\"><script>";
    special.status_text = "<b>&</b>";
    auto escaped = canvas_host::attach_worker(special);
    const std::string escaped_html = test_last_html();
    REQUIRE(contains(escaped_html, "<canvas id=\"x&quot;&gt;&lt;script&gt;\""));
    REQUIRE(contains(escaped_html, "&lt;b&gt;&amp;&lt;/b&gt;"));
    REQUIRE(contains(escaped_html, "\\u003cscript>"));
    REQUIRE(contains(escaped_html, "\\u003cb>"));
    escaped.stop();

    test_script_int_result = 1;
    canvas_host::WorkerConfig slash;
    slash.canvas_id = "a/b";
    auto slash_canvas = canvas_host::attach_worker(slash);
    REQUIRE_NOTHROW(slash_canvas.start_events());

    const std::string slash_path = "/tmp/canvas-host-events-a%2Fb";
    file = fopen(slash_path.c_str(), "wb");
    REQUIRE(file != nullptr);
    fputs("{\"type\":\"slash\"}\n", file);
    fclose(file);
    REQUIRE(slash_canvas.poll_event(line));
    REQUIRE(line == "{\"type\":\"slash\"}");
    REQUIRE_FALSE(slash_canvas.poll_event(line));
    remove(slash_path.c_str());
    slash_canvas.stop();

    const std::string quoted = canvas_host::detail::js_quote("a'b<c");
    REQUIRE(quoted == "\"a'b\\u003cc\"");

    const std::string escaped_js = canvas_host::detail::js_quote("a\"b\\c\n<");
    REQUIRE(escaped_js == "\"a\\\"b\\\\c\\n\\u003c\"");
}

TEST_CASE("C++ wrapper validates configuration and empty canvases", "[canvas_host]")
{
    test_reset();

    canvas_host::Canvas empty;
    REQUIRE_FALSE(empty.ready());
    REQUIRE_FALSE(empty.has_error());
    REQUIRE(empty.error() == "unknown error");
    REQUIRE_THROWS_AS(empty.send(canvas_host::nl::json::object()), std::runtime_error);
    REQUIRE_THROWS_AS(empty.start_events(), std::runtime_error);
    REQUIRE_THROWS_AS(empty.register_canvas(), std::runtime_error);

    canvas_host::WorkerConfig worker;
    worker.width = 0;
    REQUIRE_THROWS_AS(canvas_host::attach_worker(worker), std::runtime_error);

    worker.width = 640;
    worker.height = -1;
    REQUIRE_THROWS_AS(canvas_host::attach_worker(worker), std::runtime_error);

    worker.height = 480;
    worker.global_name = "__proto__";
    REQUIRE_THROWS_AS(canvas_host::attach_worker(worker), std::runtime_error);

    canvas_host::MainConfig app;
    REQUIRE_THROWS_AS(canvas_host::attach_main(app), std::runtime_error);

    app.factory = "createApp";
    app.options = canvas_host::nl::json::array();
    REQUIRE_THROWS_AS(canvas_host::attach_main(app), std::runtime_error);

    app.options = canvas_host::nl::json::object();
    app.start_args = {1};
    app.start_arg_types = {"number", "number"};
    REQUIRE_THROWS_AS(canvas_host::attach_main(app), std::runtime_error);

    app.start_arg_types = {};
    app.app_global = "__proto__";
    REQUIRE_THROWS_AS(canvas_host::attach_main(app), std::runtime_error);
}

TEST_CASE("C++ wrapper writes WebGL diagnostics to the configured stream", "[canvas_host]")
{
    test_webgl_create_result = 42;
    test_webgl_make_current_result = 0;
    test_reset();

    canvas_host::WorkerConfig worker;
    worker.canvas_id = "log-stream";
    std::ostringstream log;
    worker.log_stream = &log;

    auto canvas = canvas_host::attach_worker(worker);

    test_script_int_result = 1;
    test_webgl_create_result = 0;
    REQUIRE(canvas_host::create_webgl2_context(canvas) == 0);
    REQUIRE(contains(log.str(), "WebGL2 context creation failed"));
    REQUIRE(contains(log.str(), "Module.GL"));

    log.str("");
    log.clear();
    test_webgl_create_result = 42;
    test_webgl_make_current_result = 1;
    REQUIRE(canvas_host::create_webgl2_context(canvas) == 0);
    REQUIRE(test_webgl_destroyed);
    REQUIRE(contains(log.str(), "could not make the WebGL2 context current"));

    test_script_int_result = 0;
    test_webgl_create_result = 42;
    test_webgl_make_current_result = 0;
}

TEST_CASE("C++ wrapper rejects malformed event JSON", "[canvas_host]")
{
    test_script_int_result = 1;
    test_reset();

    canvas_host::WorkerConfig worker;
    worker.canvas_id = "bad-json";
    auto canvas = canvas_host::attach_worker(worker);
    REQUIRE_NOTHROW(canvas.start_events());

    const std::string path = "/tmp/canvas-host-events-bad-json";
    FILE* file = fopen(path.c_str(), "wb");
    REQUIRE(file != nullptr);
    fputs("not json\n", file);
    fclose(file);

    canvas_host::nl::json event;
    REQUIRE_THROWS_AS(canvas.poll_event(event), std::runtime_error);
    remove(path.c_str());
    canvas.stop();
}
