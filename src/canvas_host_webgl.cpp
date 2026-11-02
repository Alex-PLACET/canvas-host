/**
 * @file canvas_host_webgl.cpp
 * @brief WebGL2 context creation on an attached canvas.
 */

#include <canvas_host/canvas_host.hpp>

#include <emscripten/html5.h>

#include <cstdio>

namespace canvas_host
{

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
    attributes.majorVersion = 2;
    attributes.minorVersion = 0;

    const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context =
        emscripten_webgl_create_context("#canvas", &attributes);
    if (context == 0)
    {
        std::fprintf(stderr,
                     "canvas-host: WebGL2 context creation failed; "
                     "the kernel must export Module.GL and support OffscreenCanvas\n");
        return 0;
    }

    if (emscripten_webgl_make_context_current(context) != EMSCRIPTEN_RESULT_SUCCESS)
    {
        emscripten_webgl_destroy_context(context);
        std::fprintf(stderr, "canvas-host: could not make the WebGL2 context current\n");
        return 0;
    }

    return static_cast<int>(context);
}

} // namespace canvas_host
