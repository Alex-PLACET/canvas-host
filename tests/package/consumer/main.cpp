// Consumer smoke test: verifies the installed header and nlohmann dependency.

#include <canvas_host/canvas_host.hpp>
#include <canvas_host/version.h>

int main()
{
    canvas_host::nl::json value = {{"ok", true}};
    return value.value("ok", false) && CANVAS_HOST_VERSION_MAJOR == 0 ? 0 : 1;
}
