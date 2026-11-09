/**
 * Configuration validation, worker/main attachment, and shared attach lifecycle.
 */

namespace CanvasHostRuntime {
    function validateConfig(config: CanvasHostConfig): void {
        const { id } = config;
        if (typeof id !== "string" || id.length === 0) {
            throw new Error("canvas-host: canvas id must not be empty");
        }
        if (config.mode !== MODE_WORKER && config.mode !== MODE_MAIN) {
            throw new Error(`unknown canvas-host mode: ${config.mode}`);
        }
        validateDimension(config.width, "width");
        validateDimension(config.height, "height");
        if (config.options !== undefined && !isRecord(config.options)) {
            throw new Error("canvas-host: options must be an object");
        }
        if (config.mode === MODE_WORKER && !config.globalName) {
            throw new Error("canvas-host: worker mode requires a globalName");
        }
        if (config.globalName) {
            validateGlobalName(config.globalName, "globalName");
        }
        if (config.appGlobal) {
            validateGlobalName(config.appGlobal, "appGlobal");
        }
        if (config.mode === MODE_MAIN && !config.factory) {
            throw new Error("canvas-host: main-thread mode requires a factory name");
        }
        if (config.factory) {
            validateGlobalName(config.factory, "factory");
        }
        if (config.controlHandler) {
            validateGlobalName(config.controlHandler, "controlHandler");
        }
        if (config.onStart) {
            validateGlobalName(config.onStart, "onStart");
        }
        if (config.startArgTypes &&
            config.startArgTypes.length !== (config.startArgs?.length ?? 0)) {
            throw new Error("canvas-host: startArgs and startArgTypes must have the same length");
        }
        if (!config.startArgTypes &&
            (config.startArgs?.length ?? 0) > 0) {
            throw new Error("canvas-host: startArgs require startArgTypes");
        }
    }

    function ensureOpen(instance: CanvasHostInstance): void {
        if (instance.closed) {
            throw new Error("canvas-host: attach cancelled");
        }
    }

    async function attachWorker(
        instance: CanvasHostInstance,
        config: CanvasHostConfig,
        pixelWidth: number,
        pixelHeight: number,
        dpr: number
    ): Promise<CanvasHostHandle> {
        const { id, canvas } = instance;
        ensureOpen(instance);
        const globalName = config.globalName!;
        const storeAsGlobal = root[STORE_AS_GLOBAL] as Root["storeAsGlobal"];
        if (typeof storeAsGlobal !== "function") {
            throw new Error(
                "worker mode requires globalThis.storeAsGlobal; " +
                "use JupyterLite/xeus-lite or switch to main-thread mode"
            );
        }
        if (typeof canvas.transferControlToOffscreen !== "function") {
            throw new Error(
                "worker mode requires HTMLCanvasElement.transferControlToOffscreen"
            );
        }
        const offscreen = canvas.transferControlToOffscreen();
        offscreen.width = pixelWidth;
        offscreen.height = pixelHeight;
        await storeAsGlobal(offscreen, globalName);
        ensureOpen(instance);
        markReady(instance, pixelWidth, pixelHeight, dpr,
                  config.statusText || "Canvas transferred to the kernel worker");
        return { id, mode: MODE_WORKER };
    }

    async function attachMain(
        instance: CanvasHostInstance,
        config: CanvasHostConfig,
        pixelWidth: number,
        pixelHeight: number,
        dpr: number
    ): Promise<CanvasHostHandle> {
        const { id, canvas } = instance;
        const { width, height } = config;
        const factoryName = config.factory!;
        if (config.script && typeof root[factoryName] === "undefined") {
            await loadScript(config.script);
            ensureOpen(instance);
        }
        const factory = root[factoryName];
        if (typeof factory !== "function") {
            throw new Error(`application factory not found: ${factoryName}`);
        }

        const info = { canvas, canvasId: id, width, height, dpr };
        const app = await (factory as AppFactory)({
            ...(config.options ?? {}),
            ...info
        });
        if (app === null || (typeof app !== "object" && typeof app !== "function")) {
            throw new Error("application factory must return an object");
        }
        instance.app = app;
        ensureOpen(instance);
        instance.appGlobal = config.appGlobal || "__canvas_host_app";
        setGlobal(instance.appGlobal, instance.app);

        if (config.controlHandler) {
            instance.controlHandler =
                root[config.controlHandler] as ControlHandler;
        }

        if (config.start) {
            const args = (config.startArgs ?? []).map(value =>
                resolveArg(value, { canvasId: id, width, height, dpr })
            );
            if (typeof instance.app?.ccall === "function") {
                instance.app.ccall(
                    config.start,
                    config.startReturnType || "number",
                    config.startArgTypes ?? [],
                    args
                );
            } else {
                const start = instance.app?.[config.start];
                if (typeof start === "function") {
                    await (start as (...values: unknown[]) => unknown)(...args);
                } else {
                    throw new Error(`start function not found: ${config.start}`);
                }
            }
            ensureOpen(instance);
        }

        if (config.onStart) {
            const onStart = root[config.onStart] as StartCallback | undefined;
            await onStart?.(instance.app, info);
            ensureOpen(instance);
        }

        while (instance.pending.length) {
            ensureOpen(instance);
            await handleControlMessage(instance, instance.pending.shift()!);
        }

        ensureOpen(instance);
        markReady(instance, pixelWidth, pixelHeight, dpr,
                  config.statusText || "Application running");
        return { id, mode: MODE_MAIN };
    }

    export async function attach(config: CanvasHostConfig): Promise<CanvasHostHandle> {
        validateConfig(config);
        const { id } = config;
        const element = document.getElementById(id);
        if (!element || element.tagName?.toUpperCase() !== "CANVAS") {
            throw new Error(`canvas element not found: ${id}`);
        }
        const canvas = element as HTMLCanvasElement;

        const dpr = config.hidpi === false ? 1 : (root.devicePixelRatio || 1);
        const { width, height } = config;
        const pixelWidth = Math.round(width * dpr);
        const pixelHeight = Math.round(height * dpr);

        Object.assign(canvas.style, {
            width: `${width}px`,
            height: `${height}px`,
            maxWidth: "none"
        });
        canvas.width = pixelWidth;
        canvas.height = pixelHeight;
        canvas.tabIndex = 0;

        const previous = instances.get(id);
        if (previous) {
            try {
                await previous.app?.stop?.();
            } catch (error) {
                setError(previous.id, error);
            }
            closeInstance(previous);
        }

        if (config.mode === MODE_WORKER && config.globalName) {
            const conflict = [...instances.values()].find(
                other => other.globalName === config.globalName
            );
            if (conflict) {
                throw new Error(
                    `canvas-host: globalName already in use: ${config.globalName}`
                );
            }
        }

        const instance: CanvasHostInstance = {
            id,
            canvas,
            globalName: config.globalName,
            mode: config.mode,
            app: null,
            appGlobal: null,
            controlHandler: null,
            events: new BroadcastChannel(EVENT_CHANNEL),
            status: new BroadcastChannel(STATUS_CHANNEL),
            control: null,
            pending: [],
            abort: new AbortController(),
            closed: false
        };
        instances.set(id, instance);

        publishInfo(id, pixelWidth, pixelHeight, dpr);
        attachInput(instance);
        setupControl(instance);
        setStatus(id, config.statusText || "Canvas ready");

        try {
            if (config.mode === MODE_WORKER) {
                return await attachWorker(instance, config, pixelWidth, pixelHeight, dpr);
            }
            return await attachMain(instance, config, pixelWidth, pixelHeight, dpr);
        } catch (error) {
            try {
                await instance.app?.stop?.();
            } catch (ignored) {}
            setError(instance.id, error);
            closeInstance(instance);
            throw error;
        }
    }
}
