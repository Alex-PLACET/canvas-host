/**
 * @file canvas_host.cpp
 * @brief C17 ABI adapter over the C++ canvas-host implementation.
 */

#include <canvas_host/canvas_host.h>

#include <canvas_host/canvas_host.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstring>
#include <exception>
#include <iterator>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>

struct canvas_host_canvas
{
    explicit canvas_host_canvas(canvas_host::Canvas value)
        : canvas(std::move(value))
    {
    }

    canvas_host::Canvas canvas;
    mutable std::string error;
};

namespace
{

std::string g_last_error;

constexpr std::string_view kErrorPrefix = "canvas-host: ";
constexpr std::string_view kUnknownError = "unknown error";
constexpr std::string_view kCanvasNullError = "canvas-host: canvas is NULL";
constexpr std::string_view kWebGL2ContextError = "canvas-host: could not create a WebGL2 context";
constexpr std::string_view kEventBufferNullError = "canvas-host: event buffer is NULL or empty";
constexpr std::string_view kJsonNullError = "canvas-host: JSON message is NULL or empty";
constexpr std::string_view kJsonObjectError = "canvas-host: JSON message must be an object";
constexpr std::string_view kInvalidStartArgsPrefix = "canvas-host: invalid start_args[";
constexpr std::string_view kInvalidOptionsPrefix = "canvas-host: invalid options_json: ";
constexpr std::string_view kStartArgsMismatchError =
    "canvas-host: start_args and start_arg_types must have the same length";
constexpr std::string_view kOptionsObjectError = "canvas-host: options_json must be a JSON object";
constexpr double kDefaultDevicePixelRatio = 1.0;

std::string prefixed_error(std::string_view message)
{
    if (message.starts_with(kErrorPrefix))
    {
        return std::string(message);
    }
    return std::format("{}{}", kErrorPrefix, message);
}

void remember_error(const std::exception& error) { g_last_error = prefixed_error(error.what()); }

void remember_error(std::string_view message)
{
    g_last_error = prefixed_error(message.empty() ? kUnknownError : message);
}

void remember_canvas_error(canvas_host_canvas* canvas, const std::exception& error)
{
    if (canvas)
    {
        canvas->error = prefixed_error(error.what());
    }
    else
    {
        remember_error(error);
    }
}

void remember_canvas_error(canvas_host_canvas* canvas, std::string_view message)
{
    if (canvas)
    {
        canvas->error = prefixed_error(message.empty() ? kUnknownError : message);
    }
    else
    {
        remember_error(message);
    }
}

void clear_canvas_error(canvas_host_canvas* canvas) { canvas->error.clear(); }

template <typename Result, typename Operation>
Result canvas_operation(canvas_host_canvas* canvas, Result failure, Operation&& operation)
{
    if (!canvas)
    {
        remember_error(kCanvasNullError);
        return failure;
    }
    clear_canvas_error(canvas);
    try
    {
        return operation();
    }
    catch (const std::exception& error)
    {
        remember_canvas_error(canvas, error);
        return failure;
    }
}

template <typename Operation>
void canvas_operation(canvas_host_canvas* canvas, Operation&& operation)
{
    if (!canvas)
    {
        remember_error(kCanvasNullError);
        return;
    }
    clear_canvas_error(canvas);
    try
    {
        operation();
    }
    catch (const std::exception& error)
    {
        remember_canvas_error(canvas, error);
    }
}

template <typename Target> void copy_string(Target& target, const char* value)
{
    if (value)
    {
        target = value;
    }
}

template <typename Target, typename Source>
void copy_canvas_config(Target& target, const Source& source)
{
    if (source.width < 0)
    {
        throw std::runtime_error("canvas-host: canvas width must be positive");
    }
    if (source.height < 0)
    {
        throw std::runtime_error("canvas-host: canvas height must be positive");
    }
    if (source.width > 0) target.width = source.width;
    if (source.height > 0) target.height = source.height;
    copy_string(target.canvas_id, source.canvas_id);
    copy_string(target.status_text, source.status_text);
    copy_string(target.js_path, source.js_path);
}

canvas_host::WorkerConfig make_worker_config(const canvas_host_worker_config* config)
{
    canvas_host::WorkerConfig result;
    if (!config)
    {
        return result;
    }

    copy_canvas_config(result, *config);
    result.hidpi = config->hidpi != 0;
    copy_string(result.global_name, config->global_name);
    return result;
}

canvas_host::MainConfig make_main_config(const canvas_host_main_config* config)
{
    canvas_host::MainConfig result;
    if (!config)
    {
        return result;
    }

    copy_canvas_config(result, *config);
    copy_string(result.script, config->script);
    copy_string(result.factory, config->factory);
    copy_string(result.start, config->start);
    copy_string(result.start_return_type, config->start_return_type);
    copy_string(result.control_handler, config->control_handler);
    copy_string(result.on_start, config->on_start);
    copy_string(result.app_global, config->app_global);

    if (config->start_args)
    {
        const std::span<const char* const> values{config->start_args, config->start_args_count};
        for (const std::size_t i : std::views::iota(std::size_t{0}, values.size()))
        {
            const char* value = values[i];
            try
            {
                result.start_args.push_back(nlohmann::json::parse(value ? value : "null"));
            }
            catch (const std::exception& error)
            {
                throw std::runtime_error(
                    std::format("{}{}]: {}", kInvalidStartArgsPrefix, i, error.what()));
            }
        }
    }

    if (config->start_arg_types)
    {
        const std::span<const char* const> types{config->start_arg_types,
                                                 config->start_arg_types_count};
        std::ranges::transform(types,
                               std::back_inserter(result.start_arg_types),
                               [](const char* value) { return std::string(value ? value : ""); });
    }

    if (config->options_json && *config->options_json)
    {
        try
        {
            result.options = nlohmann::json::parse(config->options_json);
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error(std::format("{}{}", kInvalidOptionsPrefix, error.what()));
        }
        if (!result.options.is_object())
        {
            throw std::runtime_error(std::string(kOptionsObjectError));
        }
    }

    if (result.start_arg_types.size() != result.start_args.size())
    {
        throw std::runtime_error(std::string(kStartArgsMismatchError));
    }

    return result;
}

} // namespace

extern "C"
{

canvas_host_worker_config canvas_host_worker_config_default(void)
{
    canvas_host_worker_config config;
    std::memset(&config, 0, sizeof(config));
    config.width = CANVAS_HOST_DEFAULT_WIDTH;
    config.height = CANVAS_HOST_DEFAULT_HEIGHT;
    config.hidpi = 1;
    return config;
}

canvas_host_main_config canvas_host_main_config_default(void)
{
    canvas_host_main_config config;
    std::memset(&config, 0, sizeof(config));
    config.width = CANVAS_HOST_DEFAULT_WIDTH;
    config.height = CANVAS_HOST_DEFAULT_HEIGHT;
    config.start_return_type = canvas_host::default_start_return_type;
    config.app_global = canvas_host::default_app_global;
    config.status_text = canvas_host::default_application_status_text;
    return config;
}

canvas_host_canvas* canvas_host_attach_worker(const canvas_host_worker_config* config)
{
    try
    {
        return new canvas_host_canvas(canvas_host::attach_worker(make_worker_config(config)));
    }
    catch (const std::exception& error)
    {
        remember_error(error);
        return nullptr;
    }
}

canvas_host_canvas* canvas_host_attach_main(const canvas_host_main_config* config)
{
    try
    {
        return new canvas_host_canvas(canvas_host::attach_main(make_main_config(config)));
    }
    catch (const std::exception& error)
    {
        remember_error(error);
        return nullptr;
    }
}

void canvas_host_destroy(canvas_host_canvas* canvas)
{
    if (!canvas)
    {
        return;
    }
    try
    {
        canvas->canvas.stop();
    }
    catch (...)
    {
        // Destruction must not propagate cleanup failures.
    }
    delete canvas;
}

const char* canvas_host_last_error(void) { return g_last_error.c_str(); }

const char* canvas_host_id(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.id().c_str() : "";
}

const char* canvas_host_global_name(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.global_name().c_str() : "";
}

int canvas_host_width(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.width() : 0;
}

int canvas_host_height(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.height() : 0;
}

int canvas_host_pixel_width(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.pixel_width() : 0;
}

int canvas_host_pixel_height(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.pixel_height() : 0;
}

double canvas_host_device_pixel_ratio(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.device_pixel_ratio() : kDefaultDevicePixelRatio;
}

int canvas_host_ready(const canvas_host_canvas* canvas)
{
    return canvas && canvas->canvas.ready() ? 1 : 0;
}

int canvas_host_has_error(const canvas_host_canvas* canvas)
{
    return canvas && (!canvas->error.empty() || canvas->canvas.has_error()) ? 1 : 0;
}

const char* canvas_host_error(const canvas_host_canvas* canvas)
{
    if (!canvas)
    {
        return g_last_error.c_str();
    }
    if (canvas->error.empty() && canvas->canvas.has_error())
    {
        canvas->error = prefixed_error(canvas->canvas.error());
    }
    return canvas->error.c_str();
}

int canvas_host_register_canvas(canvas_host_canvas* canvas)
{
    return canvas_operation<int>(canvas,
                                 0,
                                 [&]
                                 {
                                     canvas->canvas.register_canvas();
                                     return 1;
                                 });
}

int canvas_host_create_webgl2_context(canvas_host_canvas* canvas,
                                      int alpha,
                                      int depth,
                                      int stencil,
                                      int antialias,
                                      int preserve_drawing_buffer)
{
    return canvas_operation<int>(canvas,
                                 0,
                                 [&]
                                 {
                                     const int handle = canvas_host::create_webgl2_context(
                                         canvas->canvas,
                                         alpha != 0,
                                         depth != 0,
                                         stencil != 0,
                                         antialias != 0,
                                         preserve_drawing_buffer != 0);
                                     if (handle == 0)
                                     {
                                         remember_canvas_error(canvas, kWebGL2ContextError);
                                     }
                                     return handle;
                                 });
}

void canvas_host_destroy_webgl2_context(canvas_host_canvas* canvas, int handle)
{
    canvas_operation(canvas, [&] { canvas->canvas.destroy_webgl2_context(handle); });
}

int canvas_host_start_events(canvas_host_canvas* canvas)
{
    return canvas_operation<int>(canvas,
                                 0,
                                 [&]
                                 {
                                     canvas->canvas.start_events();
                                     return 1;
                                 });
}

int canvas_host_poll_event(canvas_host_canvas* canvas, char* buffer, size_t size)
{
    if (!canvas)
    {
        remember_error(kCanvasNullError);
        return -1;
    }
    clear_canvas_error(canvas);
    if (!buffer || size == 0)
    {
        remember_canvas_error(canvas, kEventBufferNullError);
        return -1;
    }

    std::string line;
    try
    {
        if (!canvas->canvas.poll_event(line, size))
        {
            return 0;
        }
    }
    catch (const std::exception& error)
    {
        remember_canvas_error(canvas, error);
        return -1;
    }
    std::memcpy(buffer, line.c_str(), line.size() + 1);
    return 1;
}

int canvas_host_send_json(canvas_host_canvas* canvas, const char* json)
{
    if (!canvas)
    {
        remember_error(kCanvasNullError);
        return 0;
    }
    clear_canvas_error(canvas);
    if (!json || !*json)
    {
        remember_canvas_error(canvas, kJsonNullError);
        return 0;
    }
    nlohmann::json message;
    try
    {
        message = nlohmann::json::parse(json);
    }
    catch (const std::exception& error)
    {
        remember_canvas_error(canvas, error);
        return 0;
    }
    if (!message.is_object())
    {
        remember_canvas_error(canvas, kJsonObjectError);
        return 0;
    }
    return canvas_operation<int>(canvas,
                                 0,
                                 [&]
                                 {
                                     canvas->canvas.send(message);
                                     return 1;
                                 });
}

void canvas_host_stop(canvas_host_canvas* canvas)
{
    canvas_operation(canvas, [&] { canvas->canvas.stop(); });
}

} // extern "C"
