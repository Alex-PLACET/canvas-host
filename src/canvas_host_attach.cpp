/**
 * @file canvas_host_attach.cpp
 * @brief attach_worker(), attach_main(), and notebook display setup.
 */

#include <canvas_host/canvas_host.hpp>

#include "canvas_host_detail.hpp"

#include <stdexcept>
#include <string>

namespace canvas_host
{

namespace
{

void display_canvas(const CanvasConfig& config, const nl::json& data)
{
    // Install the status listener before emitting HTML so the browser cannot
    // post ready/error messages before the worker is listening.
    detail::install_status_listener(config.canvas_id);
    detail::display_html(detail::canvas_html(config.canvas_id, config.width, config.height) +
                         detail::status_html(config.canvas_id, config.status_text) +
                         detail::bootstrap_script(data, config.canvas_id, config.js_path));
}

} // namespace

Canvas attach_worker(WorkerConfig config)
{
    detail::validate_canvas_dimensions(config);

    if (config.canvas_id.empty())
    {
        config.canvas_id = detail::make_id(detail::kCanvasIdPrefix);
    }
    if (config.global_name.empty())
    {
        config.global_name = std::string(detail::kCanvasGlobalPrefix) + config.canvas_id;
    }
    detail::validate_global_name(config.global_name, "global_name");

    nl::json data;
    data["mode"] = "worker";
    data["id"] = config.canvas_id;
    data["globalName"] = config.global_name;
    data["width"] = config.width;
    data["height"] = config.height;
    data["hidpi"] = config.hidpi;
    data["statusText"] = config.status_text;

    display_canvas(config, data);
    return Canvas(Canvas::Mode::WorkerOffscreen,
                  config.canvas_id,
                  config.global_name,
                  config.width,
                  config.height,
                  config.hidpi,
                  config.log_stream);
}

Canvas attach_main(MainConfig config)
{
    detail::validate_canvas_dimensions(config);
    if (config.factory.empty())
    {
        throw std::runtime_error(std::string(detail::kMissingFactoryError));
    }
    if (!config.options.is_object())
    {
        throw std::runtime_error(std::string(detail::kInvalidOptionsError));
    }
    if (config.start_args.size() != config.start_arg_types.size())
    {
        throw std::runtime_error(std::string(detail::kStartArgsMismatchError));
    }
    if (config.app_global.empty())
    {
        config.app_global = default_app_global;
    }
    detail::validate_global_name(config.app_global, "app_global");
    if (!config.control_handler.empty())
    {
        detail::validate_global_name(config.control_handler, "control_handler");
    }
    if (!config.on_start.empty())
    {
        detail::validate_global_name(config.on_start, "on_start");
    }

    if (config.canvas_id.empty())
    {
        config.canvas_id = detail::make_id(detail::kCanvasAppIdPrefix);
    }

    nl::json data;
    data["mode"] = "main";
    data["id"] = config.canvas_id;
    data["width"] = config.width;
    data["height"] = config.height;
    data["script"] = config.script;
    data["factory"] = config.factory;
    data["start"] = config.start;
    data["startArgs"] = config.start_args;
    data["startArgTypes"] = config.start_arg_types;
    data["startReturnType"] = config.start_return_type;
    data["controlHandler"] = config.control_handler;
    data["onStart"] = config.on_start;
    data["appGlobal"] = config.app_global;
    data["options"] = config.options;
    data["statusText"] = config.status_text;
    data["hidpi"] = true;

    display_canvas(config, data);
    return Canvas(Canvas::Mode::MainThreadApp,
                  config.canvas_id,
                  {},
                  config.width,
                  config.height,
                  true,
                  config.log_stream);
}

} // namespace canvas_host
