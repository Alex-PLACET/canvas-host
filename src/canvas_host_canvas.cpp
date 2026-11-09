/**
 * @file canvas_host_canvas.cpp
 * @brief Canvas state, readiness, event queue, control messages, and cleanup.
 */

#include <canvas_host/canvas_host.hpp>

#include "canvas_host_detail.hpp"

#include <emscripten.h>
#include <emscripten/html5.h>

#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace canvas_host
{

/**
 * @brief Construct an attached Canvas with explicit state.
 */
Canvas::Canvas(Mode mode,
               std::string id,
               std::string global_name,
               int width,
               int height,
               bool hidpi,
               std::ostream* log_stream)
    : m_mode(mode)
    , m_id(std::move(id))
    , m_global_name(std::move(global_name))
    , m_width(width)
    , m_height(height)
    , m_hidpi(hidpi)
    , m_log_stream(log_stream)
{
}

/**
 * @brief Return the hosting mode.
 */
Mode Canvas::mode() const { return m_mode; }

/**
 * @brief Check whether the canvas is a worker OffscreenCanvas.
 */
bool Canvas::is_worker() const { return m_mode == Mode::WorkerOffscreen; }

/**
 * @brief Check whether the canvas belongs to a main-thread application.
 */
bool Canvas::is_main() const { return m_mode == Mode::MainThreadApp; }

/**
 * @brief Return the canvas ID.
 */
const std::string& Canvas::id() const { return m_id; }

/**
 * @brief Return the worker global name.
 */
const std::string& Canvas::global_name() const { return m_global_name; }

/**
 * @brief Return the configured CSS width.
 */
int Canvas::width() const { return m_width; }

/**
 * @brief Return the configured CSS height.
 */
int Canvas::height() const { return m_height; }

/**
 * @brief Check whether the backing store is HiDPI.
 */
bool Canvas::hidpi() const { return m_hidpi; }

/**
 * @brief Return the backing-store width in device pixels.
 */
int Canvas::pixel_width() const { return pixel_dimension("width", "pixel_width", m_width); }

/**
 * @brief Return the backing-store height in device pixels.
 */
int Canvas::pixel_height() const { return pixel_dimension("height", "pixel_height", m_height); }

/**
 * @brief Return the backing-store device pixel ratio.
 */
double Canvas::device_pixel_ratio() const
{
    const int width = pixel_width();
    return m_width > 0 && width > 0 ? static_cast<double>(width) / static_cast<double>(m_width)
                                    : 1.0;
}

/**
 * @brief Check whether the canvas has been transferred or initialized.
 */
bool Canvas::ready() const
{
    if (m_id.empty())
    {
        return false;
    }
    return detail::js_truthy(m_mode == Mode::WorkerOffscreen ? m_global_name
                                                             : detail::ready_global(m_id));
}

/**
 * @brief Check whether the canvas has reported an error.
 */
bool Canvas::has_error() const
{
    return !m_id.empty() && detail::js_truthy(detail::error_global(m_id));
}

/**
 * @brief Return the last error text.
 */
std::string Canvas::error() const
{
    if (m_id.empty())
    {
        return detail::kUnknownError;
    }
    std::string message = detail::read_file(detail::error_path(m_id));
    if (!message.empty())
    {
        return message;
    }
    const std::string script =
        std::format("String(globalThis[{}] || '')", detail::js_quote(detail::error_global(m_id)));
    const char* value = emscripten_run_script_string(script.c_str());
    return value && *value ? std::string(value) : std::string(detail::kUnknownError);
}

/**
 * @brief Return the stream used for diagnostic messages.
 */
std::ostream* Canvas::log_stream() const { return m_log_stream; }

/**
 * @brief Require the canvas to be ready.
 */
void Canvas::require_ready() const
{
    if (ready())
    {
        return;
    }
    if (has_error())
    {
        throw std::runtime_error(std::format("{}{}", detail::kErrorPrefix, error()));
    }
    throw std::runtime_error(detail::kNotReadyError);
}

/**
 * @brief Register the transferred OffscreenCanvas as Emscripten's "#canvas".
 */
void Canvas::register_canvas() const
{
    if (m_id.empty())
    {
        throw std::runtime_error(detail::kNotAttachedError);
    }
    if (m_mode != Mode::WorkerOffscreen)
    {
        throw std::runtime_error(detail::kWorkerOnlyError);
    }
    require_ready();

    const std::string error_name = detail::error_global(m_id);
    const std::string error_file = detail::error_path(m_id);
    const std::string script = std::format(
        R"JS((function() {{
    try {{
        delete globalThis[{0}];
        try {{ Module.FS.unlink({1}); }} catch (e) {{}}
        var canvas = globalThis[{2}];
        if (!canvas) throw new Error('canvas has not been transferred yet');
        if (!Module.GL || !Module.GL.offscreenCanvases) {{
            throw new Error(
                'Module.GL is not available; rebuild xeus-cpp with '
                + '-sEXPORTED_RUNTIME_METHODS=GL'
            );
        }}
        Module.canvas = canvas;
        Module.GL.offscreenCanvases['canvas'] = canvas;
        Module.GL.offscreenCanvases[{3}] = canvas;
        return 1;
    }} catch (e) {{
        var message = String(e);
        globalThis[{0}] = message;
        try {{ Module.FS.writeFile({1}, message); }} catch (ignored) {{}}
        return 0;
    }}
}})()
)JS",
        detail::js_quote(error_name),
        detail::js_quote(error_file),
        detail::js_quote(m_global_name),
        detail::js_quote(m_id));

    run_script(script);
}

/**
 * @brief Run a JavaScript snippet and throw when it reports failure.
 */
void Canvas::run_script(const std::string& script) const
{
    if (emscripten_run_script_int(script.c_str()) == 0)
    {
        throw std::runtime_error(std::format("{}{}", detail::kErrorPrefix, error()));
    }
}

/**
 * @brief Destroy an Emscripten WebGL context.
 */
void Canvas::destroy_webgl2_context(int handle) const
{
    if (handle != 0)
    {
        emscripten_webgl_destroy_context(static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(handle));
    }
}

/**
 * @brief Start forwarding normalized browser input events.
 */
void Canvas::start_events()
{
    if (m_id.empty())
    {
        throw std::runtime_error(detail::kNotAttachedError);
    }

    const std::string error_name = detail::error_global(m_id);
    const std::string error_file = detail::error_path(m_id);
    const std::string channel_name = std::string(detail::kEventChannelPrefix) + m_id;
    const std::string events_file = detail::events_path(m_id);

    const std::string script = std::format(
        R"JS((function() {{
    try {{
        delete globalThis[{0}];
        try {{ Module.FS.unlink({1}); }} catch (e) {{}}
        if (globalThis[{2}]) {{
            globalThis[{2}].close();
        }}
        try {{ Module.FS.unlink({3}); }} catch (e) {{}}
        Module.FS.writeFile({3}, '');
        var channel = new BroadcastChannel('canvas-host-events');
        channel.onmessage = function(event) {{
            var data = event.data || {{}};
            if (data.target && data.target !== {4}) return;
            var line = JSON.stringify(data) + '\n';
            var size = 0;
            try {{ size = Module.FS.stat({3}).size; }} catch (e) {{}}
            if (size + line.length >= {5}) return;
            try {{
                Module.FS.writeFile(
                    {3},
                    line,
                    {{ flags: 'a' }}
                );
            }} catch (e) {{}}
        }};
        globalThis[{2}] = channel;
        return 1;
    }} catch (e) {{
        var message = String(e);
        globalThis[{0}] = message;
        try {{ Module.FS.writeFile({1}, message); }} catch (ignored) {{}}
        return 0;
    }}
}})()
)JS",
        detail::js_quote(error_name),
        detail::js_quote(error_file),
        detail::js_quote(channel_name),
        detail::js_quote(events_file),
        detail::js_quote(m_id),
        detail::kMaxEventQueueBytes);

    m_events_offset = 0;
    try
    {
        run_script(script);
    }
    catch (...)
    {
        m_events_path.clear();
        m_events_offset = 0;
        throw;
    }
    m_events_path = events_file;
}

/**
 * @brief Read one JSON event line from the worker event queue.
 */
bool Canvas::poll_event(std::string& line) { return poll_event(line, std::string::npos); }

/**
 * @brief Read one JSON event line with a size limit.
 */
bool Canvas::poll_event(std::string& line, std::size_t max_size)
{
    if (m_events_path.empty())
    {
        return false;
    }

    std::ifstream input(m_events_path, std::ios::binary);
    if (!input)
    {
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff end = input.tellg();
    if (end < 0)
    {
        return false;
    }
    if (static_cast<std::size_t>(end) < m_events_offset)
    {
        m_events_offset = 0;
    }
    if (m_events_offset >= static_cast<std::size_t>(end))
    {
        if (end > 0)
        {
            detail::write_file(m_events_path, {});
        }
        m_events_offset = 0;
        return false;
    }

    input.seekg(static_cast<std::streamoff>(m_events_offset), std::ios::beg);
    std::string candidate;
    std::getline(input, candidate);
    if (input.eof())
    {
        // No trailing newline yet: leave the partial line for the next poll.
        return false;
    }
    if (max_size != std::string::npos && candidate.size() + 1 > max_size)
    {
        throw std::runtime_error(detail::kEventTooLargeError);
    }

    line = std::move(candidate);
    m_events_offset += line.size() + 1;

    if (m_events_offset >= static_cast<std::size_t>(end))
    {
        detail::write_file(m_events_path, {});
        m_events_offset = 0;
    }
    return true;
}

/**
 * @brief Read one event line and parse it as JSON.
 */
bool Canvas::poll_event(nl::json& event)
{
    std::string line;
    if (!poll_event(line))
    {
        return false;
    }
    try
    {
        event = nl::json::parse(line);
        return true;
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error(
            std::format("{}{}", detail::kInvalidEventJsonPrefix, error.what()));
    }
}

/**
 * @brief Send a JSON control message to the main-thread host.
 */
void Canvas::send(const nl::json& message) const
{
    if (m_id.empty())
    {
        throw std::runtime_error(detail::kNotAttachedError);
    }
    if (!message.is_object())
    {
        throw std::runtime_error(detail::kMessageObjectError);
    }
    nl::json copy = message;
    if (!copy.contains("target"))
    {
        copy["target"] = m_id;
    }
    const std::string error_name = detail::error_global(m_id);
    const std::string error_file = detail::error_path(m_id);
    const std::string script = std::format(
        R"JS((function() {{
    try {{
        if (!globalThis.__canvas_host_control) {{
            globalThis.__canvas_host_control = new BroadcastChannel('canvas-host-control');
        }}
        globalThis.__canvas_host_control.postMessage({0});
        return 1;
    }} catch (e) {{
        var message = String(e);
        globalThis[{1}] = message;
        try {{ Module.FS.writeFile({2}, message); }} catch (ignored) {{}}
        return 0;
    }}
}})()
)JS",
        detail::json_for_script(copy),
        detail::js_quote(error_name),
        detail::js_quote(error_file));
    run_script(script);
}

/**
 * @brief Stop the application and close canvas-host browser channels.
 */
void Canvas::stop()
{
    if (m_id.empty())
    {
        return;
    }

    try
    {
        send(nl::json{{"type", "stop"}});
    }
    catch (const std::exception& error)
    {
        if (std::ostream* stream = m_log_stream)
        {
            try
            {
                *stream << error.what() << std::endl;
            }
            catch (...)
            {
                // Cleanup must not fail because the diagnostic stream failed.
            }
        }
    }

    const std::string channel_name = std::string(detail::kEventChannelPrefix) + m_id;
    const std::string status_name = std::string(detail::kStatusChannelPrefix) + m_id;
    const std::string error_name = detail::error_global(m_id);
    const std::string ready_name = detail::ready_global(m_id);
    const std::string events_file =
        m_events_path.empty() ? detail::events_path(m_id) : m_events_path;
    const std::string error_file = detail::error_path(m_id);

    const std::string script = std::format(
        R"JS((function() {{
    if (globalThis[{0}]) {{
        globalThis[{0}].close();
        delete globalThis[{0}];
    }}
    if (globalThis[{1}]) {{
        globalThis[{1}].close();
        delete globalThis[{1}];
    }}
    if (globalThis.__canvas_host_control) {{
        globalThis.__canvas_host_control.close();
        delete globalThis.__canvas_host_control;
    }}
    delete globalThis[{2}];
    delete globalThis[{3}];
    delete globalThis[{4}];
    delete globalThis[{5}];
    delete globalThis[{6}];
    try {{ Module.FS.unlink({7}); }} catch (e) {{}}
    try {{ Module.FS.unlink({8}); }} catch (e) {{}}
}})()
)JS",
        detail::js_quote(channel_name),
        detail::js_quote(status_name),
        detail::js_quote(ready_name),
        detail::js_quote(error_name),
        detail::js_quote(detail::info_global(m_id, "pixel_width")),
        detail::js_quote(detail::info_global(m_id, "pixel_height")),
        detail::js_quote(detail::info_global(m_id, "dpr")),
        detail::js_quote(events_file),
        detail::js_quote(error_file));

    emscripten_run_script(script.c_str());
    m_events_path.clear();
    m_events_offset = 0;
}

/**
 * @brief Query a backing-store dimension with a configured fallback.
 */
int Canvas::pixel_dimension(std::string_view member, std::string_view info, int fallback) const
{
    if (m_mode == Mode::WorkerOffscreen)
    {
        const std::string expression = std::format(
            "(globalThis[{}] || {{}}).{} || 0", detail::js_quote(m_global_name), member);
        const int value = emscripten_run_script_int(expression.c_str());
        if (value > 0)
        {
            return value;
        }
    }
    const int value = emscripten_run_script_int(
        std::format("globalThis[{}] || 0", detail::js_quote(detail::info_global(m_id, info)))
            .c_str());
    return value > 0 ? value : fallback;
}

} // namespace canvas_host
