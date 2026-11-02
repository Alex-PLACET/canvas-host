#ifndef CANVAS_HOST_TEST_EMSCRIPTEN_HTML5_H
#define CANVAS_HOST_TEST_EMSCRIPTEN_HTML5_H

// Minimal host-side WebGL declarations. The definitions live in fakes.cpp.

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define EMSCRIPTEN_RESULT_SUCCESS 0

typedef int EMSCRIPTEN_WEBGL_CONTEXT_HANDLE;

typedef struct EmscriptenWebGLContextAttributes
{
    bool alpha;
    bool depth;
    bool stencil;
    bool antialias;
    bool premultipliedAlpha;
    bool preserveDrawingBuffer;
    int majorVersion;
    int minorVersion;
} EmscriptenWebGLContextAttributes;

void emscripten_webgl_init_context_attributes(EmscriptenWebGLContextAttributes* attributes);
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_create_context(
    const char* target, const EmscriptenWebGLContextAttributes* attributes);
int emscripten_webgl_make_context_current(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);
void emscripten_webgl_destroy_context(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context);

#ifdef __cplusplus
}
#endif

#endif /* CANVAS_HOST_TEST_EMSCRIPTEN_HTML5_H */
