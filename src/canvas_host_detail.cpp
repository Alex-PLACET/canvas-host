/**
 * @file canvas_host_detail.cpp
 * @brief Internal helpers for escaping, paths, files, HTML/bootstrap, status, and validation.
 */

#include "canvas_host_detail.hpp"

#include "canvas_host_js.hpp"

#include <emscripten.h>

#include <xeus/xinterpreter.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

namespace canvas_host
{

namespace detail
{

/**
 * @brief Quote a string as a JavaScript/JSON string literal.
 * @param value String to quote.
 * @return A JavaScript string literal including the surrounding double quotes.
 */
std::string js_quote(std::string_view value)
{
    return json_for_script(nl::json(std::string(value)));
}

/**
 * @brief Return the process-wide counter used to make canvas IDs unique.
 * @return A reference to the canvas-ID counter.
 */
int& id_counter()
{
    static int counter = 0;
    return counter;
}

/**
 * @brief Build a unique canvas ID.
 * @param prefix Prefix for the generated ID.
 * @return A unique ID of the form prefix-time-counter.
 */
std::string make_id(std::string_view prefix)
{
    return std::format(
        "{}-{}-{}", prefix, static_cast<long long>(emscripten_get_now()), ++id_counter());
}

/**
 * @brief Escape text for inclusion in HTML markup.
 * @param text Text to escape.
 * @return HTML-safe text.
 */
std::string html_escape(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (const char c : text)
    {
        switch (c)
        {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        case '\'': result += "&#39;"; break;
        default: result.push_back(c); break;
        }
    }
    return result;
}

/**
 * @brief Serialize JSON and make it safe for an HTML script element.
 *
 * Non-ASCII characters are emitted as JavaScript \uXXXX escapes by the
 * serializer, so the result is safe in older JavaScript parsers as well as
 * inside a script element.
 *
 * @param value JSON value to serialize.
 * @return JSON text that cannot terminate the surrounding script element.
 */
std::string json_for_script(const nl::json& value)
{
    const std::string text = value.dump(-1, ' ', true);
    std::string result;
    result.reserve(text.size());
    for (const char c : text)
    {
        if (c == '<')
        {
            result += kJsLessThanEscape;
        }
        else
        {
            result.push_back(c);
        }
    }
    return result;
}

/**
 * @brief Read a file completely.
 * @param path File path to read.
 * @return File contents, or an empty string when the file cannot be opened.
 */
std::string read_file(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

/**
 * @brief Replace a file with the given contents.
 * @param path File path to write.
 * @param data Contents to write.
 * @throws std::runtime_error When the file cannot be written completely.
 */
void write_file(const std::string& path, const std::string& data)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error(
            std::format("{}could not write event queue: {}", kErrorPrefix, path));
    }
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!output)
    {
        throw std::runtime_error(
            std::format("{}could not write event queue: {}", kErrorPrefix, path));
    }
}

/**
 * @brief Build the JavaScript global used to store a canvas error.
 * @param id Canvas ID.
 * @return The error-global name.
 */
std::string error_global(std::string_view id)
{
    return std::format("{}{}", kErrorGlobalPrefix, id);
}

/**
 * @brief Build the JavaScript global used to store canvas readiness.
 * @param id Canvas ID.
 * @return The ready-global name.
 */
std::string ready_global(std::string_view id)
{
    return std::format("{}{}", kReadyGlobalPrefix, id);
}

/**
 * @brief Build the JavaScript global used to store one piece of canvas geometry.
 * @param id Canvas ID.
 * @param name Geometry name, such as "pixel_width" or "dpr".
 * @return The info-global name.
 */
std::string info_global(std::string_view id, std::string_view name)
{
    return std::format("{}{}_{}", kInfoGlobalPrefix, name, id);
}

/**
 * @brief Reject JavaScript global names that would mutate object prototypes.
 * @param name Global name to validate.
 * @param field Configuration field name used in the error message.
 * @throws std::runtime_error When the name is empty or reserved.
 */
void validate_global_name(std::string_view name, std::string_view field)
{
    if (name.empty() || name == "__proto__" || name == "constructor" || name == "prototype" ||
        name == "CanvasHost" || name == "storeAsGlobal" || name == "__canvas_host_control")
    {
        throw std::runtime_error(std::format("canvas-host: invalid {}: {}", field, name));
    }
}

/**
 * @brief Validate one positive canvas dimension.
 * @param value Dimension in CSS pixels.
 * @param message Error message to throw when invalid.
 * @throws std::runtime_error When the dimension is not positive.
 */
void validate_dimension(int value, const char* message)
{
    if (value <= 0)
    {
        throw std::runtime_error(message);
    }
}

/**
 * @brief Validate the dimensions shared by worker and main-thread configs.
 * @param config Canvas configuration.
 * @throws std::runtime_error When either dimension is not positive.
 */
void validate_canvas_dimensions(const CanvasConfig& config)
{
    validate_dimension(config.width, kInvalidWidthError);
    validate_dimension(config.height, kInvalidHeightError);
}

/**
 * @brief Evaluate whether a JavaScript global is truthy.
 * @param name JavaScript global name.
 * @return true when the global is truthy, false otherwise.
 */
bool js_truthy(std::string_view name)
{
    return emscripten_run_script_int(std::format("!!globalThis[{}]", js_quote(name)).c_str()) != 0;
}

/**
 * @brief Build the status line shown below the canvas.
 * @param id Canvas ID.
 * @param text Status text.
 * @return HTML for the status line.
 */
std::string status_html(std::string_view id, std::string_view text)
{
    const std::string safe_id = html_escape(id);
    const std::string safe_text = html_escape(text);
    return std::format(
        "<div id=\"{}-status\" role=\"status\" aria-live=\"polite\" style=\"{}\">{}</div>\n",
        safe_id,
        kStatusStyle,
        safe_text);
}

/**
 * @brief Build the canvas element and its horizontal-scroll container.
 * @param id Canvas ID.
 * @param width Canvas width in CSS pixels.
 * @param height Canvas height in CSS pixels.
 * @return HTML for the canvas element.
 */
std::string canvas_html(std::string_view id, int width, int height)
{
    // No CSS border: SDL's Emscripten HiDPI path measures the canvas CSS box,
    // and a border can trigger a resize loop that collapses the canvas on
    // Retina displays. box-shadow does not affect layout.
    const std::string safe_id = html_escape(id);
    return std::format("<div style=\"{}\">\n"
                       "<canvas id=\"{}\" width=\"{}\" height=\"{}\" tabindex=\"0\" "
                       "aria-label=\"Interactive canvas\" "
                       "style=\"width:{}px;height:{}px{}\">"
                       "Your browser does not support the HTML canvas element."
                       "</canvas>\n</div>\n",
                       kCanvasContainerStyle,
                       safe_id,
                       width,
                       height,
                       width,
                       height,
                       kCanvasStyleTail);
}

/**
 * @brief Build the inline scripts that install the runtime and attach the canvas.
 * @param config Configuration object serialized into the script.
 * @param id Canvas ID.
 * @param js_path Optional external runtime URL; empty uses the embedded runtime.
 * @return HTML script elements containing the runtime/bootstrap code.
 */
std::string bootstrap_script(const nl::json& config, std::string_view id, std::string_view js_path)
{
    const std::string error_name = error_global(id);
    const std::string config_text = json_for_script(config);
    const std::string status_id = std::string(id) + "-status";

    std::string runtime;
    if (js_path.empty())
    {
        runtime = std::format("<script>\n{}\n</script>\n", canvas_host_js);
    }

    std::string loader;
    if (!js_path.empty())
    {
        loader = std::format(
            R"JS(        if (!globalThis.CanvasHost) {{
            await new Promise((resolve, reject) => {{
                const script = document.createElement('script');
                script.src = {0};
                script.onload = resolve;
                script.onerror = () => reject(new Error('failed to load canvas-host.js'));
                document.head.appendChild(script);
            }});
        }}
)JS",
            js_quote(js_path));
    }

    const std::string attach_script = std::format(
        R"JS(<script>
(async () => {{
    const status = document.getElementById({0});
    try {{
{1}        await globalThis.CanvasHost.attach({2});
    }} catch (error) {{
        globalThis[{3}] = String(error);
        try {{
            var statusChannel = new BroadcastChannel('canvas-host-status');
            statusChannel.postMessage({{target: {4}, type: 'error', message: String(error)}});
            setTimeout(function() {{ statusChannel.close(); }}, {5});
        }} catch (ignored) {{}}
        if (status) {{
            status.textContent = 'Canvas failed: ' + error;
            status.style.color = {6};
        }}
        console.error(error);
    }}
}})();
</script>
)JS",
        js_quote(status_id),
        loader,
        config_text,
        js_quote(error_name),
        js_quote(id),
        kStatusChannelCloseDelayMs,
        js_quote(kErrorColor));

    return runtime + attach_script;
}

/**
 * @brief Display generated HTML in the notebook through xeus.
 * @param html HTML document to display.
 */
void display_html(const std::string& html)
{
    nl::json data;
    data["text/html"] = html;
    xeus::get_interpreter().display_data(data, nl::json::object(), nl::json::object());
}

/**
 * @brief Encode a canvas ID for use as one filesystem path component.
 * @param value Canvas ID.
 * @return A collision-free path component without separators.
 */
std::string path_component(std::string_view value)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size());
    for (const unsigned char c : value)
    {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        if (safe)
        {
            result.push_back(static_cast<char>(c));
        }
        else
        {
            result.push_back('%');
            result.push_back(kHex[c / 16]);
            result.push_back(kHex[c % 16]);
        }
    }
    return result;
}

/**
 * @brief Return the worker-filesystem path used to cache canvas errors.
 * @param id Canvas ID.
 * @return The error-cache path.
 */
std::string error_path(std::string_view id)
{
    return std::format("{}{}", kErrorPathPrefix, path_component(id));
}

/**
 * @brief Return the worker-filesystem path used to queue input events.
 * @param id Canvas ID.
 * @return The event-queue path.
 */
std::string events_path(std::string_view id)
{
    return std::format("{}{}", kEventsPathPrefix, path_component(id));
}

/**
 * @brief Install the BroadcastChannel listener that records readiness and errors.
 * @param id Canvas ID.
 */
void install_status_listener(std::string_view id)
{
    const std::string ready_name = ready_global(id);
    const std::string error_name = error_global(id);
    std::string channel_name(kStatusChannelPrefix);
    channel_name += id;
    const std::string error_file = error_path(id);

    const std::string script = std::format(
        R"JS((function() {{
    if (globalThis[{0}]) {{
        globalThis[{0}].close();
    }}
    delete globalThis[{1}];
    delete globalThis[{2}];
    delete globalThis[{3}];
    delete globalThis[{4}];
    delete globalThis[{5}];
    try {{ Module.FS.unlink({6}); }} catch (e) {{}}
    var channel = new BroadcastChannel('canvas-host-status');
    channel.onmessage = function(event) {{
        var data = event.data || {{}};
        if (data.target && data.target !== {7}) return;
        if (data.type === 'ready') {{
            globalThis[{1}] = true;
            if (data.width) globalThis[{3}] = data.width;
            if (data.height) globalThis[{4}] = data.height;
            if (data.dpr) globalThis[{5}] = data.dpr;
        }} else if (data.type === 'error') {{
            var message = String(data.message || 'canvas-host error');
            globalThis[{2}] = message;
            try {{ Module.FS.writeFile({6}, message); }} catch (e) {{}}
        }}
    }};
    globalThis[{0}] = channel;
}})()
)JS",
        js_quote(channel_name),
        js_quote(ready_name),
        js_quote(error_name),
        js_quote(info_global(id, "pixel_width")),
        js_quote(info_global(id, "pixel_height")),
        js_quote(info_global(id, "dpr")),
        js_quote(error_file),
        js_quote(id));

    emscripten_run_script(script.c_str());
}

} // namespace detail

} // namespace canvas_host
