#pragma once

/**
 * @file canvas_host_detail.hpp
 * @brief Internal declarations and constants shared by the implementation files.
 */

#include <canvas_host/canvas_host.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace canvas_host
{

namespace detail
{

inline constexpr std::string_view kErrorGlobalPrefix = "__canvas_host_error_";
inline constexpr std::string_view kReadyGlobalPrefix = "__canvas_host_ready_";
inline constexpr std::string_view kInfoGlobalPrefix = "__canvas_host_";
inline constexpr std::string_view kErrorPathPrefix = "/tmp/canvas-host-error-";
inline constexpr std::string_view kEventsPathPrefix = "/tmp/canvas-host-events-";
inline constexpr std::string_view kStatusChannelPrefix = "__canvas_host_status_channel_";
inline constexpr std::string_view kEventChannelPrefix = "__canvas_host_event_channel_";
inline constexpr std::string_view kCanvasGlobalPrefix = "__canvas_host_canvas_";
inline constexpr std::string_view kCanvasIdPrefix = "canvas";
inline constexpr std::string_view kCanvasAppIdPrefix = "canvas-app";
inline constexpr std::string_view kErrorPrefix = "canvas-host: ";
inline constexpr std::string_view kUnknownError = "unknown error";
inline constexpr std::string_view kNotAttachedError = "canvas-host: canvas is not attached";
inline constexpr std::string_view kMessageObjectError =
    "canvas-host: control message must be a JSON object";
inline constexpr std::string_view kInvalidWidthError = "canvas-host: canvas width must be positive";
inline constexpr std::string_view kInvalidHeightError =
    "canvas-host: canvas height must be positive";
inline constexpr std::string_view kMissingFactoryError =
    "canvas-host: main-thread mode requires a factory name";
inline constexpr std::string_view kInvalidOptionsError =
    "canvas-host: options must be a JSON object";
inline constexpr std::string_view kStartArgsMismatchError =
    "canvas-host: start_args and start_arg_types must have the same length";
inline constexpr std::string_view kInvalidEventJsonPrefix = "canvas-host: invalid event JSON: ";
inline constexpr std::size_t kMaxEventQueueBytes = 1024 * 1024;
inline constexpr int kStatusChannelCloseDelayMs = 100;
inline constexpr double kDefaultDevicePixelRatio = 1.0;
inline constexpr std::string_view kJsLessThanEscape = "\\u003c";
inline constexpr std::string_view kNotReadyError =
    "canvas-host: the canvas is not ready yet. "
    "Run the attach cell, wait for the output to render, then run "
    "this cell.";
inline constexpr std::string_view kWorkerOnlyError = "canvas-host: register_canvas is worker-only";
inline constexpr std::string_view kEventTooLargeError = "canvas-host: event buffer is too small";
inline constexpr std::string_view kStatusStyle =
    "font-family:sans-serif;font-size:12px;color:#8a8aa0;"
    "margin:4px 0 10px 0;";
inline constexpr std::string_view kErrorColor = "#ff7070";
inline constexpr std::array<std::string_view, 6> kReservedGlobalNames = {"__proto__",
                                                                         "constructor",
                                                                         "prototype",
                                                                         "CanvasHost",
                                                                         "storeAsGlobal",
                                                                         "__canvas_host_control"};
inline constexpr std::string_view kCanvasContainerStyle =
    "max-width:100%;overflow-x:auto;overflow-y:hidden;";
inline constexpr std::string_view kCanvasStyleTail =
    "px;display:block;box-shadow:0 0 0 1px #3a3a52;"
    "border-radius:4px;background:#12121c;"
    "touch-action:none;";

/**
 * @brief Build a unique canvas ID.
 * @param prefix Prefix for the generated ID.
 * @return A unique ID of the form prefix-time-counter.
 */
std::string make_id(std::string_view prefix);
/**
 * @brief Escape text for inclusion in HTML markup.
 * @param text Text to escape.
 * @return HTML-safe text.
 */
std::string html_escape(std::string_view text);
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
std::string json_for_script(const nl::json& value);
/**
 * @brief Read a file completely.
 * @param path File path to read.
 * @return File contents, or an empty string when the file cannot be opened.
 */
std::string read_file(const std::string& path);
/**
 * @brief Replace a file with the given contents.
 * @param path File path to write.
 * @param data Contents to write.
 * @throws std::runtime_error When the file cannot be written completely.
 */
void write_file(const std::string& path, const std::string& data);
/**
 * @brief Build the JavaScript global used to store a canvas error.
 * @param id Canvas ID.
 * @return The error-global name.
 */
std::string error_global(std::string_view id);
/**
 * @brief Build the JavaScript global used to store canvas readiness.
 * @param id Canvas ID.
 * @return The ready-global name.
 */
std::string ready_global(std::string_view id);
/**
 * @brief Build the JavaScript global used to store one piece of canvas geometry.
 * @param id Canvas ID.
 * @param name Geometry name, such as "pixel_width" or "dpr".
 * @return The info-global name.
 */
std::string info_global(std::string_view id, std::string_view name);
/**
 * @brief Reject JavaScript global names that would mutate object prototypes.
 * @param name Global name to validate.
 * @param field Configuration field name used in the error message.
 * @throws std::runtime_error When the name is empty or reserved.
 */
void validate_global_name(std::string_view name, std::string_view field);
/**
 * @brief Validate one positive canvas dimension.
 * @param value Dimension in CSS pixels.
 * @param message Error message to throw when invalid.
 * @throws std::runtime_error When the dimension is not positive.
 */
void validate_dimension(int value, std::string_view message);
/**
 * @brief Validate the dimensions shared by worker and main-thread configs.
 * @param config Canvas configuration.
 * @throws std::runtime_error When either dimension is not positive.
 */
void validate_canvas_dimensions(const CanvasConfig& config);
/**
 * @brief Evaluate whether a JavaScript global is truthy.
 * @param name JavaScript global name.
 * @return true when the global is truthy, false otherwise.
 */
bool js_truthy(std::string_view name);
/**
 * @brief Build the status line shown below the canvas.
 * @param id Canvas ID.
 * @param text Status text.
 * @return HTML for the status line.
 */
std::string status_html(std::string_view id, std::string_view text);
/**
 * @brief Build the canvas element and its horizontal-scroll container.
 * @param id Canvas ID.
 * @param width Canvas width in CSS pixels.
 * @param height Canvas height in CSS pixels.
 * @return HTML for the canvas element.
 */
std::string canvas_html(std::string_view id, int width, int height);
/**
 * @brief Build the inline scripts that install the runtime and attach the canvas.
 * @param config Configuration object serialized into the script.
 * @param id Canvas ID.
 * @param js_path Optional external runtime URL; empty uses the embedded runtime.
 * @return HTML script elements containing the runtime/bootstrap code.
 */
std::string bootstrap_script(const nl::json& config, std::string_view id, std::string_view js_path);
/**
 * @brief Display generated HTML in the notebook through xeus.
 * @param html HTML document to display.
 */
void display_html(const std::string& html);
/**
 * @brief Encode a canvas ID for use as one filesystem path component.
 * @param value Canvas ID.
 * @return A collision-free path component without separators.
 */
std::string path_component(std::string_view value);
/**
 * @brief Return the worker-filesystem path used to cache canvas errors.
 * @param id Canvas ID.
 * @return The error-cache path.
 */
std::string error_path(std::string_view id);
/**
 * @brief Return the worker-filesystem path used to queue input events.
 * @param id Canvas ID.
 * @return The event-queue path.
 */
std::string events_path(std::string_view id);
/**
 * @brief Install the BroadcastChannel listener that records readiness and errors.
 * @param id Canvas ID.
 */
void install_status_listener(std::string_view id);

} // namespace detail

} // namespace canvas_host
