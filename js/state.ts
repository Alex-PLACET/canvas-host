/**
 * Instance registry, status/error reporting, control messages, readiness, and cleanup.
 */

namespace CanvasHostRuntime {
    export const instances = new Map<string, CanvasHostInstance>();

    const statusElement = (id: string): HTMLElement | null =>
        document.getElementById(`${id}-status`);

    export function setStatus(id: string, text: string, isError = false): void {
        const element = statusElement(id);
        if (!element) {
            return;
        }
        element.textContent = text;
        element.style.color = isError ? "#ff7070" : "#8a8aa0";
    }

    export function setError(id: string, error: unknown): void {
        const message = String(error);
        setGlobal(`${ERROR_GLOBAL}${id}`, message);
        const instance = instances.get(id);
        if (instance && !instance.closed) {
            instance.status.postMessage({ target: id, type: "error", message });
        }
        setStatus(id, `Canvas failed: ${message}`, true);
        console.error("[canvas-host]", message);
    }

    const postStatus = (
        instance: CanvasHostInstance,
        type: string,
        data: Record<string, unknown> = {}
    ): void => {
        if (!instance.closed) {
            instance.status.postMessage({ target: instance.id, type, ...data });
        }
    };

    export function publishInfo(
        id: string,
        width: number,
        height: number,
        dpr: number
    ): void {
        setGlobal(`${PIXEL_WIDTH_GLOBAL}${id}`, width);
        setGlobal(`${PIXEL_HEIGHT_GLOBAL}${id}`, height);
        setGlobal(`${DPR_GLOBAL}${id}`, dpr);
    }

    export function closeInstance(instance: CanvasHostInstance): void {
        if (instance.closed) {
            return;
        }
        instance.closed = true;
        instance.abort.abort();
        if (instance.control) {
            instance.control.onmessage = null;
        }
        for (const channel of [instance.events, instance.status, instance.control]) {
            channel?.close();
        }
        if (instance.globalName) {
            deleteGlobal(instance.globalName);
        }
        if (instance.appGlobal) {
            const shared = [...instances.values()].some(other =>
                other !== instance && other.appGlobal === instance.appGlobal
            );
            if (!shared) {
                deleteGlobal(instance.appGlobal);
            }
        }
        for (const name of [
            `${READY_GLOBAL}${instance.id}`,
            `${ERROR_GLOBAL}${instance.id}`,
            `${PIXEL_WIDTH_GLOBAL}${instance.id}`,
            `${PIXEL_HEIGHT_GLOBAL}${instance.id}`,
            `${DPR_GLOBAL}${instance.id}`
        ]) {
            deleteGlobal(name);
        }
        instance.pending.length = 0;
        instances.delete(instance.id);
    }

    export async function handleControlMessage(
        instance: CanvasHostInstance,
        message: ControlMessage
    ): Promise<void> {
        if (instance.closed) {
            return;
        }
        if (message.type === CONTROL_STOP) {
            let failed = false;
            try {
                await instance.app?.stop?.();
            } catch (error) {
                failed = true;
                setError(instance.id, error);
            }
            closeInstance(instance);
            if (!failed) {
                setStatus(instance.id, "Stopped");
            }
            return;
        }
        if (typeof instance.controlHandler === "function") {
            await instance.controlHandler(instance.app, message);
        } else {
            await instance.app?.handleCanvasMessage?.(message);
        }
    }

    function dispatchControl(
        instance: CanvasHostInstance,
        message: ControlMessage
    ): void {
        if (instance.closed) {
            return;
        }
        if (message.target && message.target !== instance.id) {
            return;
        }
        if (instance.mode === MODE_MAIN && !instance.app && message.type !== CONTROL_STOP) {
            instance.pending.push(message);
            return;
        }
        void handleControlMessage(instance, message).catch(error => {
            setError(instance.id, error);
        });
    }

    export function setupControl(instance: CanvasHostInstance): void {
        if (instance.closed) {
            return;
        }
        const control = new BroadcastChannel(CONTROL_CHANNEL);
        instance.control = control;
        control.onmessage = event => {
            dispatchControl(instance, (event.data || {}) as ControlMessage);
        };
    }

    export function markReady(
        instance: CanvasHostInstance,
        width: number,
        height: number,
        dpr: number,
        statusText: string
    ): void {
        if (instance.closed) {
            return;
        }
        setGlobal(`${READY_GLOBAL}${instance.id}`, true);
        postStatus(instance, "ready", { width, height, dpr });
        setStatus(instance.id, statusText);
    }

    export function send(id: string, message: ControlMessage): void {
        const instance = instances.get(id);
        if (!instance) {
            throw new Error(`unknown canvas: ${id}`);
        }
        if (instance.closed) {
            throw new Error(`canvas is closed: ${id}`);
        }
        if (!isRecord(message)) {
            throw new Error("canvas-host: control message must be an object");
        }
        dispatchControl(instance, { target: id, ...message });
    }
}
