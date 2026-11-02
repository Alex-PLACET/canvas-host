#ifndef CANVAS_HOST_TEST_EMSCRIPTEN_H
#define CANVAS_HOST_TEST_EMSCRIPTEN_H

// Minimal host-side declarations for the Emscripten APIs used by the
// canvas-host implementation. The definitions live in fakes.cpp.

#ifdef __cplusplus
extern "C"
{
#endif

double emscripten_get_now(void);
void emscripten_run_script(const char* script);
int emscripten_run_script_int(const char* script);
char* emscripten_run_script_string(const char* script);

#ifdef __cplusplus
}
#endif

#endif /* CANVAS_HOST_TEST_EMSCRIPTEN_H */
