#ifndef CANVAS_HOST_CANVAS_HOST_H
#define CANVAS_HOST_CANVAS_HOST_H

#include <canvas_host/version.h>

/**
 * @file canvas_host.h
 * @brief C17 ABI for canvas-host.
 *
 * This header is valid C17 and C++. The implementation is compiled as C++ in
 * the src/canvas_host_*.cpp files; src/canvas_host.cpp is the extern "C" adapter.
 *
 * C programs link against the canvas_host_c library. C++ programs may use the
 * C++ API in canvas_host.hpp, which requires the same library.
 *
 * The C ABI is not thread-safe. Call it from the interpreter/worker thread
 * that owns the Emscripten runtime.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Opaque handle for a canvas host instance.
 *
 * Create a handle with canvas_host_attach_worker() or
 * canvas_host_attach_main(), and release it with canvas_host_destroy().
 */
typedef struct canvas_host_canvas canvas_host_canvas;

/**
 * @brief Default canvas dimensions in CSS pixels.
 */
enum
{
    CANVAS_HOST_DEFAULT_WIDTH = 900, /**< Default canvas width. */
    CANVAS_HOST_DEFAULT_HEIGHT = 600 /**< Default canvas height. */
};

/**
 * @brief Configuration for a worker-hosted OffscreenCanvas.
 *
 * Start from canvas_host_worker_config_default() and then override the
 * fields you need.
 */
typedef struct canvas_host_worker_config
{
    int width;               /**< Canvas CSS width in pixels. */
    int height;              /**< Canvas CSS height in pixels. */
    int hidpi;               /**< Non-zero to use devicePixelRatio for the backing store. */
    const char* canvas_id;   /**< Optional canvas ID; generated when NULL or empty. */
    const char* global_name; /**< Optional worker global that receives the OffscreenCanvas. */
    const char* status_text; /**< Optional text shown below the canvas. */
    const char* js_path;     /**< Optional external runtime URL; NULL uses the embedded runtime. */
} canvas_host_worker_config;

/**
 * @brief Configuration for a main-thread browser application.
 *
 * Start from canvas_host_main_config_default() and then override the fields
 * you need.
 */
typedef struct canvas_host_main_config
{
    int width;                     /**< Canvas CSS width in pixels. */
    int height;                    /**< Canvas CSS height in pixels. */
    const char* canvas_id;         /**< Optional canvas ID; generated when NULL or empty. */
    const char* script;            /**< JavaScript file that defines the application factory. */
    const char* factory;           /**< Global factory function name. */
    const char* start;             /**< Optional start function exported by the application. */
    const char* const* start_args; /**< Optional array of JSON values passed to start. */
    size_t start_args_count;       /**< Number of entries in start_args. */
    const char* const* start_arg_types; /**< Optional array of Emscripten ccall argument types. */
    size_t start_arg_types_count;       /**< Number of entries in start_arg_types. */
    const char* start_return_type;      /**< Optional ccall return type; defaults to "number". */
    const char* control_handler;        /**< Optional global control-message handler. */
    const char* on_start;               /**< Optional global callback invoked after start. */
    const char* app_global;   /**< Optional global that receives the application object. */
    const char* options_json; /**< Optional JSON object merged into the factory options. */
    const char* status_text;  /**< Optional text shown below the canvas. */
    const char* js_path;      /**< Optional external runtime URL; NULL uses the embedded runtime. */
} canvas_host_main_config;

/**
 * @brief Fill a worker configuration with defaults.
 *
 * The returned configuration uses a 900x600 CSS canvas with HiDPI enabled.
 *
 * @return A default worker configuration.
 */
canvas_host_worker_config canvas_host_worker_config_default(void);

/**
 * @brief Fill a main-thread configuration with defaults.
 *
 * The returned configuration uses a 900x600 CSS canvas, "Application" as the
 * status text, and "__canvas_host_app" as the application global.
 *
 * @return A default main-thread configuration.
 */
canvas_host_main_config canvas_host_main_config_default(void);

/**
 * @brief Attach a worker-hosted OffscreenCanvas.
 *
 * Passing NULL uses the default configuration. On success, the canvas is
 * displayed in the notebook output and transferred to the kernel worker.
 *
 * @param config Optional worker configuration.
 * @return A canvas handle, or NULL on failure.
 */
canvas_host_canvas* canvas_host_attach_worker(const canvas_host_worker_config* config);

/**
 * @brief Attach a main-thread browser application.
 *
 * Passing NULL uses the default configuration. The default has no factory,
 * so a factory must be provided for the attach to succeed. On success, the
 * canvas is displayed in the notebook output and the application factory is
 * loaded.
 *
 * @param config Optional main-thread configuration.
 * @return A canvas handle, or NULL on failure.
 */
canvas_host_canvas* canvas_host_attach_main(const canvas_host_main_config* config);

/**
 * @brief Destroy a canvas handle.
 *
 * Call canvas_host_stop() first to stop the application and close its
 * browser channels.
 *
 * @param canvas Canvas handle to destroy. May be NULL.
 */
void canvas_host_destroy(canvas_host_canvas* canvas);

/**
 * @brief Return the last attach or invalid-handle error.
 *
 * @return An empty string when no error has been recorded. The returned string
 *         remains valid until the next error is recorded.
 */
const char* canvas_host_last_error(void);

/**
 * @brief Return the canvas ID.
 *
 * @param canvas Canvas handle.
 * @return The canvas ID, or an empty string when canvas is NULL. The string is
 *         owned by the handle and remains valid until it is destroyed.
 */
const char* canvas_host_id(const canvas_host_canvas* canvas);

/**
 * @brief Return the worker global that receives the OffscreenCanvas.
 *
 * @param canvas Canvas handle.
 * @return The global name, or an empty string when canvas is NULL. The string
 *         is owned by the handle and remains valid until it is destroyed.
 */
const char* canvas_host_global_name(const canvas_host_canvas* canvas);

/**
 * @brief Return the configured CSS width.
 *
 * @param canvas Canvas handle.
 * @return The CSS width in pixels, or 0 when canvas is NULL.
 */
int canvas_host_width(const canvas_host_canvas* canvas);

/**
 * @brief Return the configured CSS height.
 *
 * @param canvas Canvas handle.
 * @return The CSS height in pixels, or 0 when canvas is NULL.
 */
int canvas_host_height(const canvas_host_canvas* canvas);

/**
 * @brief Return the backing-store width in device pixels.
 *
 * @param canvas Canvas handle.
 * @return The pixel width, or 0 when canvas is NULL or unavailable.
 */
int canvas_host_pixel_width(const canvas_host_canvas* canvas);

/**
 * @brief Return the backing-store height in device pixels.
 *
 * @param canvas Canvas handle.
 * @return The pixel height, or 0 when canvas is NULL or unavailable.
 */
int canvas_host_pixel_height(const canvas_host_canvas* canvas);

/**
 * @brief Return the backing-store device pixel ratio.
 *
 * @param canvas Canvas handle.
 * @return The device pixel ratio, or 1.0 when canvas is NULL or unavailable.
 */
double canvas_host_device_pixel_ratio(const canvas_host_canvas* canvas);

/**
 * @brief Check whether the canvas has been transferred or initialized.
 *
 * @param canvas Canvas handle.
 * @return 1 when ready, 0 otherwise.
 */
int canvas_host_ready(const canvas_host_canvas* canvas);

/**
 * @brief Check whether the canvas has reported an error.
 *
 * @param canvas Canvas handle.
 * @return 1 when an error is available, 0 otherwise.
 */
int canvas_host_has_error(const canvas_host_canvas* canvas);

/**
 * @brief Return the last error associated with a canvas.
 *
 * When canvas is NULL, this returns the last attach error.
 *
 * @param canvas Canvas handle, or NULL.
 * @return The error string. It is owned by the handle and remains valid until
 *         the handle is destroyed or another C API operation is called.
 */
const char* canvas_host_error(const canvas_host_canvas* canvas);

/**
 * @brief Register a transferred OffscreenCanvas with Emscripten.
 *
 * Worker mode only. Requires Module.GL to be exported by the kernel.
 *
 * @param canvas Canvas handle.
 * @return 1 on success, 0 on failure.
 */
int canvas_host_register_canvas(canvas_host_canvas* canvas);

/**
 * @brief Create and make current a WebGL2 context on the canvas.
 *
 * This also registers the canvas with Emscripten.
 *
 * @param canvas Canvas handle.
 * @param alpha Non-zero to request an alpha channel.
 * @param depth Non-zero to request a depth buffer.
 * @param stencil Non-zero to request a stencil buffer.
 * @param antialias Non-zero to request antialiasing.
 * @param preserve_drawing_buffer Non-zero to preserve the drawing buffer.
 * @return An Emscripten WebGL context handle, or 0 on failure.
 */
int canvas_host_create_webgl2_context(canvas_host_canvas* canvas,
                                      int alpha,
                                      int depth,
                                      int stencil,
                                      int antialias,
                                      int preserve_drawing_buffer);

/**
 * @brief Destroy a WebGL2 context created by canvas_host_create_webgl2_context().
 *
 * @param canvas Canvas handle.
 * @param handle Context handle returned by canvas_host_create_webgl2_context().
 */
void canvas_host_destroy_webgl2_context(canvas_host_canvas* canvas, int handle);

/**
 * @brief Start forwarding normalized browser input events.
 *
 * Events are written to an internal queue and read with
 * canvas_host_poll_event().
 *
 * @param canvas Canvas handle.
 * @return 1 on success, 0 on failure.
 */
int canvas_host_start_events(canvas_host_canvas* canvas);

/**
 * @brief Read one normalized JSON event line.
 *
 * @param canvas Canvas handle.
 * @param buffer Destination buffer.
 * @param size Size of buffer in bytes.
 * @return 1 when an event was copied, 0 when the queue is empty, and -1 on
 *         invalid arguments, truncation, or an internal error. The event is
 *         not consumed when the buffer is too small.
 */
int canvas_host_poll_event(canvas_host_canvas* canvas, char* buffer, size_t size);

/**
 * @brief Send a JSON control message to the main-thread host.
 *
 * The JSON object is copied and a target ID is added when absent. The caller
 * keeps ownership of the input string.
 *
 * @param canvas Canvas handle.
 * @param json JSON object text.
 * @return 1 on success, 0 on failure.
 */
int canvas_host_send_json(canvas_host_canvas* canvas, const char* json);

/**
 * @brief Stop the application and close its browser channels.
 *
 * The handle remains valid for accessors after this call, but the canvas no
 * longer receives control messages.
 *
 * @param canvas Canvas handle. May be NULL.
 */
void canvas_host_stop(canvas_host_canvas* canvas);

#ifdef __cplusplus
}
#endif

#endif /* CANVAS_HOST_CANVAS_HOST_H */
