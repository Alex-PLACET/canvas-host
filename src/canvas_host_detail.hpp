#pragma once

/**
 * @file canvas_host_detail.hpp
 * @brief Internal declarations and constants shared by the implementation files.
 */

#include <canvas_host/canvas_host.hpp>

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
inline constexpr const char* kCanvasIdPrefix = "canvas";
inline constexpr const char* kCanvasAppIdPrefix = "canvas-app";
inline constexpr const char* kErrorPrefix = "canvas-host: ";
inline constexpr const char* kUnknownError = "unknown error";
inline constexpr const char* kNotAttachedError = "canvas-host: canvas is not attached";
inline constexpr const char* kMessageObjectError =
    "canvas-host: control message must be a JSON object";
inline constexpr const char* kInvalidWidthError = "canvas-host: canvas width must be positive";
inline constexpr const char* kInvalidHeightError = "canvas-host: canvas height must be positive";
inline constexpr const char* kMissingFactoryError =
    "canvas-host: main-thread mode requires a factory name";
inline constexpr const char* kInvalidOptionsError = "canvas-host: options must be a JSON object";
inline constexpr const char* kStartArgsMismatchError =
    "canvas-host: start_args and start_arg_types must have the same length";
inline constexpr const char* kInvalidEventJsonPrefix = "canvas-host: invalid event JSON: ";
inline constexpr std::size_t kMaxEventQueueBytes = 1024 * 1024;
inline constexpr int kStatusChannelCloseDelayMs = 100;
inline constexpr double kDefaultDevicePixelRatio = 1.0;
inline constexpr std::string_view kJsLessThanEscape = "\\u003c";
inline constexpr const char* kNotReadyError =
    "canvas-host: the canvas is not ready yet. "
    "Run the attach cell, wait for the output to render, then run "
    "this cell.";
inline constexpr const char* kWorkerOnlyError = "canvas-host: register_canvas is worker-only";
inline constexpr const char* kEventTooLargeError = "canvas-host: event buffer is too small";
inline constexpr const char* kStatusStyle = "font-family:sans-serif;font-size:12px;color:#8a8aa0;"
                                            "margin:4px 0 10px 0;";
inline constexpr const char* kErrorColor = "#ff7070";
inline constexpr const char* kCanvasContainerStyle =
    "max-width:100%;overflow-x:auto;overflow-y:hidden;";
inline constexpr const char* kCanvasStyleTail = "px;display:block;box-shadow:0 0 0 1px #3a3a52;"
                                                "border-radius:4px;background:#12121c;"
                                                "touch-action:none;";

std::string make_id(std::string_view prefix);
std::string html_escape(std::string_view text);
std::string json_for_script(const nl::json& value);
std::string read_file(const std::string& path);
void write_file(const std::string& path, const std::string& data);
std::string error_global(std::string_view id);
std::string ready_global(std::string_view id);
std::string info_global(std::string_view id, std::string_view name);
void validate_global_name(std::string_view name, std::string_view field);
void validate_dimension(int value, const char* message);
void validate_canvas_dimensions(const CanvasConfig& config);
bool js_truthy(std::string_view name);
std::string status_html(std::string_view id, std::string_view text);
std::string canvas_html(std::string_view id, int width, int height);
std::string bootstrap_script(const nl::json& config, std::string_view id, std::string_view js_path);
void display_html(const std::string& html);
std::string path_component(std::string_view value);
std::string error_path(std::string_view id);
std::string events_path(std::string_view id);
void install_status_listener(std::string_view id);

} // namespace detail

} // namespace canvas_host
