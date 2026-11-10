/**
 * @file canvas_host.hpp
 * @brief C++17 API for canvas-host.
 *
 * The implementation lives in the src/canvas_host_*.cpp files. The C ABI in
 * canvas_host.h is a separate adapter over this same implementation.
 */

#pragma once

#include <canvas_host/version.h>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace canvas_host
{

namespace nl = nlohmann;

/** @brief Default canvas width in CSS pixels. */
inline constexpr int default_canvas_width = 900;

/** @brief Default canvas height in CSS pixels. */
inline constexpr int default_canvas_height = 600;

/** @brief Default status text for worker canvases. */
inline constexpr const char* default_status_text = "Canvas";

/** @brief Default status text for main-thread applications. */
inline constexpr const char* default_application_status_text = "Application";

/** @brief Default Emscripten ccall return type. */
inline constexpr const char* default_start_return_type = "number";

/** @brief Default global that receives main-thread applications. */
inline constexpr const char* default_app_global = "__canvas_host_app";

/**
 * @brief Common configuration shared by worker and main-thread canvases.
 */
struct CanvasConfig
{
    int width = default_canvas_width;              /**< Canvas CSS width in pixels. */
    int height = default_canvas_height;            /**< Canvas CSS height in pixels. */
    std::string canvas_id;                         /**< Optional canvas ID; generated when empty. */
    std::string status_text = default_status_text; /**< Text shown below the canvas. */
    std::string js_path; /**< Optional external runtime URL; empty uses the embedded runtime. */
    std::ostream* log_stream = &std::cerr; /**< Stream for diagnostics; not owned, must outlive
                                               the Canvas. nullptr disables logging. */
};

/**
 * @brief Configuration for a worker-hosted OffscreenCanvas.
 */
struct WorkerConfig : CanvasConfig
{
    bool hidpi = true;       /**< Use devicePixelRatio for the backing store. */
    std::string global_name; /**< Optional worker global that receives the OffscreenCanvas. */
};

/**
 * @brief Configuration for a main-thread browser application.
 */
struct MainConfig : CanvasConfig
{
    /**
     * @brief Construct a main-thread config with the "Application" status text.
     */
    MainConfig() { status_text = default_application_status_text; }

    std::string script;               /**< JavaScript file that defines the application factory. */
    std::string factory;              /**< Global factory function name. */
    std::string start;                /**< Optional start function exported by the application. */
    std::vector<nl::json> start_args; /**< Optional JSON values passed to start. */
    std::vector<std::string> start_arg_types; /**< ccall argument types; required when
                                                   start_args is non-empty. */
    std::string start_return_type = default_start_return_type; /**< ccall return type. */
    std::string control_handler; /**< Optional global control-message handler. */
    std::string on_start;        /**< Optional global callback invoked after start. */
    std::string app_global =
        default_app_global;                /**< Global that receives the application object. */
    nl::json options = nl::json::object(); /**< Options merged into the factory options. */
};

namespace detail
{

/**
 * @brief Quote a string as a JavaScript/JSON string literal.
 *
 * This helper is used when embedding user-provided strings in generated
 * JavaScript.
 *
 * @param value String to quote.
 * @return A JavaScript string literal including the surrounding double quotes.
 */
std::string js_quote(std::string_view value);

} // namespace detail

/**
 * @brief Handle to an attached canvas or main-thread application.
 *
 * Canvas objects are move-only. Attach functions return a fully initialized
 * Canvas, and moving transfers ownership of its event-queue state. Call stop()
 * before destroying a Canvas to release browser listeners and globals. Canvas
 * operations are not thread-safe.
 */
class Canvas
{
public:
    /**
     * @brief Construct an empty, unattached Canvas.
     */
    Canvas() = default;

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;
    Canvas(Canvas&&) = default;
    Canvas& operator=(Canvas&&) = default;

    /**
     * @brief Check whether the canvas is a worker OffscreenCanvas.
     * @return true for worker canvases, false otherwise.
     */
    bool is_worker() const;

    /**
     * @brief Check whether the canvas belongs to a main-thread application.
     * @return true for main-thread canvases, false otherwise.
     */
    bool is_main() const;

    /**
     * @brief Return the canvas ID.
     * @return The canvas ID.
     */
    const std::string& id() const;

    /**
     * @brief Return the worker global that receives the OffscreenCanvas.
     * @return The worker global name, or an empty string for main-thread canvases.
     */
    const std::string& global_name() const;

    /**
     * @brief Return the configured CSS width.
     * @return The CSS width in pixels.
     */
    int width() const;

    /**
     * @brief Return the configured CSS height.
     * @return The CSS height in pixels.
     */
    int height() const;

    /**
     * @brief Check whether the backing store uses devicePixelRatio.
     * @return true when HiDPI is enabled, false otherwise.
     */
    bool hidpi() const;

    /**
     * @brief Return the backing-store width in device pixels.
     * @return The pixel width, or the configured width when unavailable.
     */
    int pixel_width() const;

    /**
     * @brief Return the backing-store height in device pixels.
     * @return The pixel height, or the configured height when unavailable.
     */
    int pixel_height() const;

    /**
     * @brief Return the backing-store device pixel ratio.
     * @return The device pixel ratio, or 1.0 when unavailable.
     */
    double device_pixel_ratio() const;

    /**
     * @brief Check whether the canvas has been transferred or initialized.
     * @return true when the canvas is ready, false otherwise.
     */
    bool ready() const;

    /**
     * @brief Check whether the canvas has reported an error.
     * @return true when an error is available, false otherwise.
     */
    bool has_error() const;

    /**
     * @brief Return the last error associated with the canvas.
     * @return The error text, or "unknown error" when none is cached.
     */
    std::string error() const;

    /**
     * @brief Return the stream used for diagnostic messages.
     * @return The configured stream, or nullptr when logging is disabled. The
     *         stream is not owned and must outlive the Canvas.
     */
    std::ostream* log_stream() const;

    /**
     * @brief Require the canvas to be ready.
     * @throws std::runtime_error When the canvas is not ready or has an error.
     */
    void require_ready() const;

    /**
     * @brief Register the transferred OffscreenCanvas with Emscripten.
     *
     * Worker mode only. Requires Module.GL to be exported by the kernel.
     *
     * @throws std::runtime_error On registration failure or when called for a
     *         main-thread canvas.
     */
    void register_canvas() const;

    /**
     * @brief Destroy a WebGL2 context created by create_webgl2_context().
     * @param handle Context handle, or 0 to do nothing.
     */
    void destroy_webgl2_context(int handle) const;

    /**
     * @brief Start forwarding normalized browser input events.
     * @throws std::runtime_error On setup failure.
     */
    void start_events();

    /**
     * @brief Read one normalized JSON event line.
     * @param line Receives the JSON line.
     * @return true when an event was read, false when the queue is empty.
     * @throws std::runtime_error On an internal queue or buffer error.
     */
    bool poll_event(std::string& line);

    /**
     * @brief Read one normalized event line, limiting its size.
     *
     * The event is not consumed when it does not fit in max_size.
     *
     * @param line Receives the JSON line.
     * @param max_size Maximum line size including the null terminator.
     * @return true when an event was read, false when the queue is empty.
     * @throws std::runtime_error When the line does not fit or the queue
     *         cannot be updated.
     */
    bool poll_event(std::string& line, std::size_t max_size);

    /**
     * @brief Read one normalized event as JSON.
     * @param event Receives the parsed event.
     * @return true when an event was read and parsed, false when the queue is
     *         empty or the line is not valid JSON.
     * @throws std::runtime_error On an internal queue or buffer error.
     */
    bool poll_event(nl::json& event);

    /**
     * @brief Send a control message to the main-thread host.
     *
     * A target ID is added when the message does not already contain one.
     *
     * @param message JSON object to send.
     * @throws std::runtime_error When the message cannot be sent.
     */
    void send(const nl::json& message) const;

    /**
     * @brief Stop the application and close its browser channels.
     *
     * Safe to call on an empty Canvas.
     */
    void stop();

private:
    /** @brief Canvas hosting mode. */
    enum class Mode
    {
        WorkerOffscreen, /**< Canvas is transferred to the kernel worker as an OffscreenCanvas. */
        MainThreadApp    /**< Canvas stays on the main thread for a browser application. */
    };

    /**
     * @brief Construct an attached Canvas with explicit state.
     * @param mode Hosting mode.
     * @param id Canvas ID.
     * @param global_name Worker global name.
     * @param width CSS width.
     * @param height CSS height.
     * @param hidpi Whether the backing store is HiDPI.
     * @param log_stream Stream for diagnostics, or nullptr to disable logging.
     */
    Canvas(Mode mode,
           std::string id,
           std::string global_name,
           int width,
           int height,
           bool hidpi,
           std::ostream* log_stream);

    /**
     * @brief Query one backing-store dimension with a configured fallback.
     * @param member JavaScript member name to query.
     * @param info Info-global suffix to query when the member is unavailable.
     * @param fallback Value to return when both queries fail.
     * @return The queried dimension or fallback.
     */
    int pixel_dimension(std::string_view member, std::string_view info, int fallback) const;

    /**
     * @brief Run a JavaScript snippet and throw when it reports failure.
     * @param script JavaScript source to execute.
     * @throws std::runtime_error When the script returns zero.
     */
    void run_script(const std::string& script) const;

    Mode m_mode = Mode::WorkerOffscreen;     /**< Hosting mode. */
    std::string m_id;                        /**< Canvas ID. */
    std::string m_global_name;               /**< Worker global name. */
    int m_width = default_canvas_width;      /**< CSS width. */
    int m_height = default_canvas_height;    /**< CSS height. */
    bool m_hidpi = true;                     /**< Whether the backing store is HiDPI. */
    std::ostream* m_log_stream = &std::cerr; /**< Stream for diagnostics, or nullptr. */
    std::string m_events_path;               /**< Event queue path, set by start_events(). */
    std::size_t m_events_offset = 0;         /**< Read offset within the event queue. */

    friend Canvas attach_worker(WorkerConfig config);
    friend Canvas attach_main(MainConfig config);
    friend int create_webgl2_context(const Canvas& canvas,
                                     bool alpha,
                                     bool depth,
                                     bool stencil,
                                     bool antialias,
                                     bool preserve_drawing_buffer);
};

/**
 * @brief Attach a worker-hosted OffscreenCanvas.
 *
 * Passing a default WorkerConfig lets the host generate the canvas ID and
 * worker global name.
 *
 * @param config Worker configuration.
 * @return The attached Canvas.
 * @throws std::runtime_error When the canvas cannot be displayed or attached.
 */
Canvas attach_worker(WorkerConfig config = {});

/**
 * @brief Attach a main-thread browser application.
 *
 * @param config Main-thread configuration.
 * @return The attached Canvas.
 * @throws std::runtime_error When the canvas or application cannot be attached.
 */
Canvas attach_main(MainConfig config);

/**
 * @brief Create and make current a WebGL2 context on the canvas.
 *
 * This registers the canvas with Emscripten before creating the context.
 *
 * @param canvas Attached worker canvas.
 * @param alpha Request an alpha channel.
 * @param depth Request a depth buffer.
 * @param stencil Request a stencil buffer.
 * @param antialias Request antialiasing.
 * @param preserve_drawing_buffer Preserve the drawing buffer.
 * @return An Emscripten WebGL context handle, or 0 on context creation failure.
 * @throws std::runtime_error When the canvas is empty or registration fails.
 */
int create_webgl2_context(const Canvas& canvas,
                          bool alpha = false,
                          bool depth = true,
                          bool stencil = true,
                          bool antialias = false,
                          bool preserve_drawing_buffer = true);

} // namespace canvas_host
