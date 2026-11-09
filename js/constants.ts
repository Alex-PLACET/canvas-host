/**
 * Runtime constants, channel names, and global key prefixes.
 */

namespace CanvasHostRuntime {
    export const root = globalThis as Root;

    export const CONTROL_CHANNEL = "canvas-host-control";
    export const EVENT_CHANNEL = "canvas-host-events";
    export const STATUS_CHANNEL = "canvas-host-status";

    export const CONTROL_STOP = "stop";
    export const MODE_MAIN = "main";
    export const MODE_WORKER = "worker";
    export const STORE_AS_GLOBAL = "storeAsGlobal";

    export const DEFAULT_APP_GLOBAL = "__canvas_host_app";
    export const DEFAULT_DEVICE_PIXEL_RATIO = 1;
    export const MIN_CSS_DIMENSION = 1;
    export const CANVAS_TAB_INDEX = 0;
    export const STATUS_COLOR = "#8a8aa0";
    export const ERROR_COLOR = "#ff7070";

    export const ERROR_GLOBAL = "__canvas_host_error_";
    export const READY_GLOBAL = "__canvas_host_ready_";
    export const PIXEL_WIDTH_GLOBAL = "__canvas_host_pixel_width_";
    export const PIXEL_HEIGHT_GLOBAL = "__canvas_host_pixel_height_";
    export const DPR_GLOBAL = "__canvas_host_dpr_";

    export const EVENT_POINTER_DOWN = "pointerdown";
    export const EVENT_POINTER_MOVE = "pointermove";
    export const EVENT_POINTER_UP = "pointerup";
    export const EVENT_POINTER_CANCEL = "pointercancel";
    export const EVENT_WHEEL = "wheel";
    export const EVENT_KEY_DOWN = "keydown";
    export const EVENT_KEY_UP = "keyup";
    export const EVENT_KEY_PRESS = "keypress";
    export const EVENT_FOCUS = "focus";
    export const EVENT_BLUR = "blur";

    export const PREVENT_DEFAULT_KEYS: ReadonlySet<string> = new Set([
        "ArrowLeft",
        "ArrowRight",
        "ArrowUp",
        "ArrowDown",
        " "
    ]);

    export const RESERVED_GLOBALS: ReadonlySet<string> = new Set([
        "__proto__",
        "constructor",
        "prototype",
        "CanvasHost",
        "storeAsGlobal",
        "__canvas_host_control"
    ]);
}
