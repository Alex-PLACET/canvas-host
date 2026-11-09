/**
 * @file canvas_host_webgl.cpp
 * @brief WebGL2 context creation on an attached canvas.
 */

#include <canvas_host/canvas_host.hpp>

#include <emscripten/html5.h>

namespace canvas_host
{

namespace
{

constexpr int kWebGL2MajorVersion = 2;
constexpr int kWebGL2MinorVersion = 0;

/**
 * @brief Write a diagnostic message to the canvas log stream.
 * @param canvas Canvas whose log stream should receive the message.
 * @param message Message to write.
 */
void log_message(const Canvas& canvas, const char* message)
{
    if (std::ostream* stream = canvas.log_stream())
    {
        try
        {
            *stream << message << std::endl;
        }
        catch (...)
        {
            // Diagnostics must never change the outcome of the WebGL call.
        }
    }
}

} // namespace

/**
 * @brief Create and make current a WebGL2 context on an attached canvas.
 */
int create_webgl2_context(const Canvas& canvas,
                          bool alpha,
                          bool depth,
                          bool stencil,
                          bool antialias,
                          bool preserve_drawing_buffer)
{
    canvas.register_canvas();

    EmscriptenWebGLContextAttributes attributes;
    emscripten_webgl_init_context_attributes(&attributes);
    attributes.alpha = alpha;
    attributes.depth = depth;
    attributes.stencil = stencil;
    attributes.antialias = antialias;
    attributes.premultipliedAlpha = false;
    attributes.preserveDrawingBuffer = preserve_drawing_buffer;
    attributes.majorVersion = kWebGL2MajorVersion;
    attributes.minorVersion = kWebGL2MinorVersion;

    const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context =
        emscripten_webgl_create_context("#canvas", &attributes);
    if (context == 0)
    {
        log_message(canvas,
                    "canvas-host: WebGL2 context creation failed; "
                    "the kernel must export Module.GL and support OffscreenCanvas");
        return 0;
    }

    if (emscripten_webgl_make_context_current(context) != EMSCRIPTEN_RESULT_SUCCESS)
    {
        emscripten_webgl_destroy_context(context);
        log_message(canvas, "canvas-host: could not make the WebGL2 context current");
        return 0;
    }

    return static_cast<int>(context);
}

} // namespace canvas_host
