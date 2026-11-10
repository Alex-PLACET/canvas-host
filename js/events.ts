/**
 * DOM input capture and normalized event posting for one canvas instance.
 */

namespace CanvasHostRuntime {
    export function attachInput(instance: CanvasHostInstance): void {
        const { id, canvas, events, abort } = instance;
        const signal = abort.signal;

        const post = (
            type: string,
            data: Record<string, unknown> = {}
        ): void => events.postMessage({
            target: id,
            type,
            eventTime: performance.timeOrigin + performance.now(),
            ...data
        });

        const position = (
            event: MouseEvent | PointerEvent | WheelEvent
        ): Record<string, number> => {
            const rect = canvas.getBoundingClientRect();
            const cssWidth = Math.max(MIN_CSS_DIMENSION, rect.width);
            const cssHeight = Math.max(MIN_CSS_DIMENSION, rect.height);
            const x = event.clientX - rect.left;
            const y = event.clientY - rect.top;
            return {
                x,
                y,
                deviceX: x * (canvas.width / cssWidth),
                deviceY: y * (canvas.height / cssHeight),
                cssWidth,
                cssHeight,
                pixelWidth: canvas.width,
                pixelHeight: canvas.height,
                dpr: canvas.width / cssWidth
            };
        };

        const modifiers = (
            event: KeyboardEvent | MouseEvent | PointerEvent | WheelEvent
        ): Record<string, boolean> => ({
            ctrl: event.ctrlKey,
            shift: event.shiftKey,
            alt: event.altKey,
            meta: event.metaKey
        });

        const pointerData = (event: PointerEvent): Record<string, unknown> => ({
            ...position(event),
            button: event.button,
            buttons: event.buttons,
            pointerId: event.pointerId,
            ...modifiers(event)
        });

        const keyData = (event: KeyboardEvent): Record<string, unknown> => ({
            key: event.key,
            code: event.code,
            keyCode: event.keyCode,
            repeat: event.repeat,
            ...modifiers(event)
        });

        const pointerEvents = [
            EVENT_POINTER_DOWN,
            EVENT_POINTER_MOVE,
            EVENT_POINTER_UP,
            EVENT_POINTER_CANCEL
        ] as const satisfies readonly string[];
        for (const type of pointerEvents) {
            canvas.addEventListener(type, event => {
                const pointer = event as PointerEvent;
                if (type === EVENT_POINTER_DOWN) {
                    canvas.focus();
                    try {
                        canvas.setPointerCapture(pointer.pointerId);
                    } catch (ignored) {}
                } else if (type === EVENT_POINTER_UP || type === EVENT_POINTER_CANCEL) {
                    try {
                        canvas.releasePointerCapture(pointer.pointerId);
                    } catch (ignored) {}
                }
                post(type, pointerData(pointer));
                if (type === EVENT_POINTER_DOWN || type === EVENT_POINTER_UP) {
                    pointer.preventDefault();
                }
            }, { signal });
        }

        canvas.addEventListener(EVENT_WHEEL, event => {
            post(EVENT_WHEEL, {
                ...position(event),
                dx: event.deltaX,
                dy: event.deltaY,
                deltaMode: event.deltaMode,
                ...modifiers(event)
            });
            event.preventDefault();
        }, { passive: false, signal });

        canvas.addEventListener(EVENT_KEY_DOWN, event => {
            post(EVENT_KEY_DOWN, keyData(event));
            if (PREVENT_DEFAULT_KEYS.has(event.key)) {
                event.preventDefault();
            }
            event.stopPropagation();
        }, { signal });

        const textEvents = [EVENT_KEY_UP, EVENT_KEY_PRESS] as const satisfies readonly string[];
        for (const type of textEvents) {
            canvas.addEventListener(type, event => {
                const keyboard = event as KeyboardEvent;
                post(type, type === EVENT_KEY_PRESS
                    ? { ...keyData(keyboard), charCode: keyboard.charCode }
                    : keyData(keyboard));
                event.stopPropagation();
            }, { signal });
        }

        canvas.addEventListener(EVENT_FOCUS, () => post(EVENT_FOCUS), { signal });
        canvas.addEventListener(EVENT_BLUR, () => post(EVENT_BLUR), { signal });
        canvas.addEventListener("contextmenu", event => event.preventDefault(), { signal });
    }
}
