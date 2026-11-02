/**
 * Installs the public globalThis.CanvasHost API.
 */

namespace CanvasHostRuntime {
    export function install(): void {
        if (root.CanvasHost) {
            return;
        }
        root.CanvasHost = {
            attach,
            send,
            stop: id => send(id, { type: CONTROL_STOP }),
            get: id => instances.get(id),
            list: () => [...instances.keys()]
        } satisfies CanvasHostApi;
    }
}

CanvasHostRuntime.install();
