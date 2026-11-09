/**
 * @file canvas_host_detail.cpp
 * @brief Internal helpers for escaping, paths, files, HTML/bootstrap, status, and validation.
 */

#include "canvas_host_detail.hpp"

#include "canvas_host_js.hpp"

#include <emscripten.h>

#include <xeus/xinterpreter.hpp>

#include <algorithm>
#include <fstream>
#include <ranges>
#include <stdexcept>
#include <string>

namespace canvas_host
{

namespace detail
{

std::string js_quote(std::string_view value)
{
    return json_for_script(nl::json(std::string(value)));
}

int& id_counter()
{
    static int counter = 0;
    return counter;
}

std::string make_id(std::string_view prefix)
{
    return std::format(
        "{}-{}-{}", prefix, static_cast<long long>(emscripten_get_now()), ++id_counter());
}

std::string html_escape(std::string_view text)
{
    if (std::ranges::find_first_of(text, std::string_view{"&<>\"'"}) == text.end())
    {
        return std::string(text);
    }

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

std::string json_for_script(const nl::json& value)
{
    const std::string text = value.dump(-1, ' ', true);
    if (std::ranges::find(text, '<') == text.end())
    {
        return text;
    }

    std::string result;
    result.reserve(text.size() + kJsLessThanEscape.size());
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

std::string read_file(const std::string& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
    {
        return {};
    }
    const std::streamoff end = input.tellg();
    if (end <= 0)
    {
        return {};
    }

    std::string result;
    result.resize_and_overwrite(static_cast<std::size_t>(end),
                                [&](char* data, std::size_t size)
                                {
                                    input.seekg(0, std::ios::beg);
                                    input.read(data, static_cast<std::streamsize>(size));
                                    return static_cast<std::size_t>(input.gcount());
                                });
    return result;
}

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

std::string error_global(std::string_view id)
{
    return std::format("{}{}", kErrorGlobalPrefix, id);
}

std::string ready_global(std::string_view id)
{
    return std::format("{}{}", kReadyGlobalPrefix, id);
}

std::string info_global(std::string_view id, std::string_view name)
{
    return std::format("{}{}_{}", kInfoGlobalPrefix, name, id);
}

void validate_global_name(std::string_view name, std::string_view field)
{
    if (name.empty() || std::ranges::contains(kReservedGlobalNames, name))
    {
        throw std::runtime_error(std::format("canvas-host: invalid {}: {}", field, name));
    }
}

void validate_dimension(int value, std::string_view message)
{
    if (value <= 0)
    {
        throw std::runtime_error(std::string(message));
    }
}

void validate_canvas_dimensions(const CanvasConfig& config)
{
    validate_dimension(config.width, kInvalidWidthError);
    validate_dimension(config.height, kInvalidHeightError);
}

bool js_truthy(std::string_view name)
{
    return emscripten_run_script_int(std::format("!!globalThis[{}]", js_quote(name)).c_str()) != 0;
}

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

void display_html(const std::string& html)
{
    nl::json data;
    data["text/html"] = html;
    xeus::get_interpreter().display_data(data, nl::json::object(), nl::json::object());
}

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

std::string error_path(std::string_view id)
{
    return std::format("{}{}", kErrorPathPrefix, path_component(id));
}

std::string events_path(std::string_view id)
{
    return std::format("{}{}", kEventsPathPrefix, path_component(id));
}

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
