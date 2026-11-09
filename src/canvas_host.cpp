/**
 * @file canvas_host.cpp
 * @brief C17 ABI adapter over the C++ canvas-host implementation.
 */

#include <canvas_host/canvas_host.h>

#include <canvas_host/canvas_host.hpp>

#include <nlohmann/json.hpp>

#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>

/**
 * @brief C ABI handle that owns a C++ Canvas and its cached error string.
 */
struct canvas_host_canvas
{
    /**
     * @brief Construct a C ABI handle from a C++ Canvas.
     * @param value Canvas value to own.
     */
    explicit canvas_host_canvas(canvas_host::Canvas value)
        : canvas(std::move(value))
    {
    }

    canvas_host::Canvas canvas;
    mutable std::string error;
};

namespace
{

/** @brief Last attach-level error. */
std::string g_last_error;

constexpr const char* kErrorPrefix = "canvas-host: ";
constexpr const char* kUnknownError = "unknown error";
constexpr const char* kCanvasNullError = "canvas-host: canvas is NULL";
constexpr const char* kWebGL2ContextError = "canvas-host: could not create a WebGL2 context";
constexpr const char* kEventBufferNullError = "canvas-host: event buffer is NULL or empty";
constexpr const char* kJsonNullError = "canvas-host: JSON message is NULL or empty";
constexpr const char* kJsonObjectError = "canvas-host: JSON message must be an object";
constexpr const char* kInvalidStartArgsPrefix = "canvas-host: invalid start_args[";
constexpr const char* kInvalidOptionsPrefix = "canvas-host: invalid options_json: ";
constexpr const char* kStartArgsMismatchError =
    "canvas-host: start_args and start_arg_types must have the same length";
constexpr const char* kOptionsObjectError = "canvas-host: options_json must be a JSON object";
constexpr double kDefaultDevicePixelRatio = 1.0;

/**
 * @brief Add the canvas-host prefix once.
 * @param message Error message to normalize.
 * @return The prefixed error message.
 */
std::string prefixed_error(std::string_view message)
{
    if (message.starts_with(kErrorPrefix))
    {
        return std::string(message);
    }
    return std::format("{}{}", kErrorPrefix, message);
}

/**
 * @brief Record an attach-level error.
 * @param error Exception to record.
 */
void remember_error(const std::exception& error) { g_last_error = prefixed_error(error.what()); }

/**
 * @brief Record a text attach-level error.
 * @param message Error text to record.
 */
void remember_error(const char* message)
{
    g_last_error = prefixed_error(message ? message : kUnknownError);
}

/**
 * @brief Record an exception on a canvas or globally.
 * @param canvas Canvas handle, or NULL to record globally.
 * @param error Exception to record.
 */
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

/**
 * @brief Record a text error on a canvas or globally.
 * @param canvas Canvas handle, or NULL to record globally.
 * @param message Error text to record.
 */
void remember_canvas_error(canvas_host_canvas* canvas, const char* message)
{
    if (canvas)
    {
        canvas->error = prefixed_error(message ? message : kUnknownError);
    }
    else
    {
        remember_error(message);
    }
}

/**
 * @brief Clear a canvas's cached error before a new operation.
 * @param canvas Canvas handle.
 */
void clear_canvas_error(canvas_host_canvas* canvas) { canvas->error.clear(); }

/**
 * @brief Run a canvas operation with common null and exception handling.
 * @tparam Result Result type.
 * @tparam Operation Callable returning Result.
 * @param canvas Canvas handle, or NULL to record a global error.
 * @param failure Value to return on error.
 * @param operation Operation to run.
 * @return The operation result, or failure.
 */
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

/**
 * @brief Run a void canvas operation with common null and exception handling.
 * @tparam Operation Callable returning void.
 * @param canvas Canvas handle, or NULL to record a global error.
 * @param operation Operation to run.
 */
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

/**
 * @brief Copy an optional C string into a C++ string.
 * @tparam Target Destination string-like type.
 * @param target Destination value.
 * @param value Optional source string.
 */
template <typename Target> void copy_string(Target& target, const char* value)
{
    if (value)
    {
        target = value;
    }
}

/**
 * @brief Copy fields shared by the C worker and main-thread config structs.
 * @tparam Target Destination C++ config type.
 * @tparam Source Source C config type.
 * @param target Destination config.
 * @param source Source config.
 */
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

/**
 * @brief Convert a C worker config into the C++ config type.
 * @param config Optional C worker config.
 * @return The equivalent C++ worker config.
 */
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

/**
 * @brief Convert a C main-thread config into the C++ config type.
 * @param config Optional C main-thread config.
 * @return The equivalent C++ main-thread config.
 * @throws std::runtime_error When start_args or options_json contain invalid JSON.
 */
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
        for (size_t i = 0; i < config->start_args_count; ++i)
        {
            const char* value = config->start_args[i];
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
        for (size_t i = 0; i < config->start_arg_types_count; ++i)
        {
            result.start_arg_types.emplace_back(
                config->start_arg_types[i] ? config->start_arg_types[i] : "");
        }
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
            throw std::runtime_error(kOptionsObjectError);
        }
    }

    if (result.start_arg_types.size() != result.start_args.size())
    {
        throw std::runtime_error(kStartArgsMismatchError);
    }

    return result;
}

} // namespace

extern "C"
{

/**
 * @brief Return the default C worker configuration.
 */
canvas_host_worker_config canvas_host_worker_config_default(void)
{
    canvas_host_worker_config config;
    std::memset(&config, 0, sizeof(config));
    config.width = CANVAS_HOST_DEFAULT_WIDTH;
    config.height = CANVAS_HOST_DEFAULT_HEIGHT;
    config.hidpi = 1;
    return config;
}

/**
 * @brief Return the default C main-thread configuration.
 */
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

/**
 * @brief Attach a worker canvas through the C ABI.
 */
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

/**
 * @brief Attach a main-thread canvas through the C ABI.
 */
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

/**
 * @brief Release a C ABI canvas handle.
 */
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

/**
 * @brief Return the last attach-level error.
 */
const char* canvas_host_last_error(void) { return g_last_error.c_str(); }

/**
 * @brief Return the canvas ID.
 */
const char* canvas_host_id(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.id().c_str() : "";
}

/**
 * @brief Return the worker global name.
 */
const char* canvas_host_global_name(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.global_name().c_str() : "";
}

/**
 * @brief Return the configured CSS width.
 */
int canvas_host_width(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.width() : 0;
}

/**
 * @brief Return the configured CSS height.
 */
int canvas_host_height(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.height() : 0;
}

/**
 * @brief Return the backing-store width.
 */
int canvas_host_pixel_width(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.pixel_width() : 0;
}

/**
 * @brief Return the backing-store height.
 */
int canvas_host_pixel_height(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.pixel_height() : 0;
}

/**
 * @brief Return the backing-store device pixel ratio.
 */
double canvas_host_device_pixel_ratio(const canvas_host_canvas* canvas)
{
    return canvas ? canvas->canvas.device_pixel_ratio() : kDefaultDevicePixelRatio;
}

/**
 * @brief Return whether the canvas is ready.
 */
int canvas_host_ready(const canvas_host_canvas* canvas)
{
    return canvas && canvas->canvas.ready() ? 1 : 0;
}

/**
 * @brief Return whether the canvas has reported an error.
 */
int canvas_host_has_error(const canvas_host_canvas* canvas)
{
    return canvas && (!canvas->error.empty() || canvas->canvas.has_error()) ? 1 : 0;
}

/**
 * @brief Return the last error for a canvas or the last attach error for NULL.
 */
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

/**
 * @brief Register a worker canvas with Emscripten.
 */
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

/**
 * @brief Create a WebGL2 context through the C ABI.
 */
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

/**
 * @brief Destroy a WebGL2 context through the C ABI.
 */
void canvas_host_destroy_webgl2_context(canvas_host_canvas* canvas, int handle)
{
    canvas_operation(canvas, [&] { canvas->canvas.destroy_webgl2_context(handle); });
}

/**
 * @brief Start forwarding normalized input events through the C ABI.
 */
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

/**
 * @brief Copy one event line through the C ABI.
 */
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

/**
 * @brief Send a JSON control message through the C ABI.
 */
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

/**
 * @brief Stop the application through the C ABI.
 */
void canvas_host_stop(canvas_host_canvas* canvas)
{
    canvas_operation(canvas, [&] { canvas->canvas.stop(); });
}

} // extern "C"
