/**
 * Browser runtime tests using a minimal fake DOM and Node's test runner.
 */

import assert from "node:assert/strict";
import { after, test } from "node:test";
import { pathToFileURL } from "node:url";
import { resolve } from "node:path";

class FakeEventTarget {
    constructor() {
        this.listeners = new Map();
    }

    addEventListener(type, handler, options = {}) {
        const opts = typeof options === "object" && options !== null ? options : {};
        if (opts.signal?.aborted) {
            return;
        }
        const entry = { handler, once: Boolean(opts.once) };
        const list = this.listeners.get(type) ?? [];
        list.push(entry);
        this.listeners.set(type, list);
        if (opts.signal) {
            opts.signal.addEventListener("abort", () => {
                this.removeEventListener(type, handler);
            }, { once: true });
        }
    }

    removeEventListener(type, handler) {
        const list = this.listeners.get(type);
        if (!list) {
            return;
        }
        const index = list.findIndex(entry => entry.handler === handler);
        if (index >= 0) {
            list.splice(index, 1);
        }
        if (list.length === 0) {
            this.listeners.delete(type);
        }
    }

    listenerCount(type) {
        return (this.listeners.get(type) ?? []).length;
    }

    dispatchEvent(event) {
        event.target ??= this;
        const list = [...(this.listeners.get(event.type) ?? [])];
        for (const entry of list) {
            if (entry.once) {
                this.removeEventListener(event.type, entry.handler);
            }
            entry.handler.call(this, event);
        }
        return event;
    }
}

class FakeCanvas extends FakeEventTarget {
    constructor(id, cssWidth = 100, cssHeight = 50) {
        super();
        this.id = id;
        this.tagName = "CANVAS";
        this.style = {};
        this.width = 0;
        this.height = 0;
        this.tabIndex = 0;
        this.cssWidth = cssWidth;
        this.cssHeight = cssHeight;
        this.focused = false;
        this.captured = new Set();
        this.transferred = false;
        this.attributes = new Map();
    }

    focus() {
        this.focused = true;
    }

    getBoundingClientRect() {
        return {
            left: 10,
            top: 5,
            width: this.cssWidth,
            height: this.cssHeight
        };
    }

    setPointerCapture(pointerId) {
        this.captured.add(pointerId);
    }

    releasePointerCapture(pointerId) {
        this.captured.delete(pointerId);
    }

    transferControlToOffscreen() {
        if (this.transferred) {
            throw new Error("canvas has already been transferred");
        }
        this.transferred = true;
        return { width: 0, height: 0 };
    }

    setAttribute(name, value) {
        this.attributes.set(name, value);
    }
}

class FakeElement {
    constructor() {
        this.style = {};
        this.textContent = "";
    }
}

class FakeScript {
    constructor() {
        this.src = "";
        this.onload = null;
        this.onerror = null;
    }
}

const elements = new Map();

function addCanvas(id, width = 100, height = 50) {
    const canvas = new FakeCanvas(id, width, height);
    elements.set(id, canvas);
    elements.set(`${id}-status`, new FakeElement());
    return canvas;
}

function statusText(id) {
    return elements.get(`${id}-status`)?.textContent;
}

globalThis.document = {
    getElementById: id => elements.get(id) ?? null,
    createElement: tag => tag === "script" ? new FakeScript() : new FakeElement(),
    head: {
        appendChild(script) {
            script.onload?.();
        }
    }
};
globalThis.devicePixelRatio = 2;

function pointerEvent(type, overrides = {}) {
    return {
        type,
        clientX: 0,
        clientY: 0,
        button: 0,
        buttons: 0,
        pointerId: 1,
        ctrlKey: false,
        shiftKey: false,
        altKey: false,
        metaKey: false,
        defaultPrevented: false,
        propagationStopped: false,
        preventDefault() {
            this.defaultPrevented = true;
        },
        stopPropagation() {
            this.propagationStopped = true;
        },
        ...overrides
    };
}

function keyboardEvent(type, overrides = {}) {
    return {
        type,
        key: "",
        code: "",
        keyCode: 0,
        charCode: 0,
        repeat: false,
        ctrlKey: false,
        shiftKey: false,
        altKey: false,
        metaKey: false,
        defaultPrevented: false,
        propagationStopped: false,
        preventDefault() {
            this.defaultPrevented = true;
        },
        stopPropagation() {
            this.propagationStopped = true;
        },
        ...overrides
    };
}

const delay = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));

async function withSilencedErrors(callback) {
    const original = console.error;
    console.error = () => {};
    try {
        return await callback();
    } finally {
        console.error = original;
    }
}

async function waitFor(predicate, timeout = 2000) {
    const start = Date.now();
    while (!predicate()) {
        if (Date.now() - start > timeout) {
            throw new Error("timed out waiting for condition");
        }
        await delay(5);
    }
}

const runtimePath = resolve(
    process.env.CANVAS_HOST_JS ??
        new URL("../build/canvas-host.js", import.meta.url).pathname
);
await import(pathToFileURL(runtimePath).href);

const CanvasHost = globalThis.CanvasHost;
assert.ok(CanvasHost, "canvas-host runtime did not install globalThis.CanvasHost");

after(async () => {
    for (const id of CanvasHost.list()) {
        try {
            CanvasHost.stop(id);
        } catch (error) {
            // The instance may already be closing.
        }
    }
    await delay(25);
});

test("worker attach stores the canvas and normalizes pointer events", async () => {
    const id = "worker-events";
    const canvas = addCanvas(id);
    let stored;

    globalThis.storeAsGlobal = async (value, name) => {
        stored = { value, name };
        globalThis[name] = value;
    };

    const events = new BroadcastChannel("canvas-host-events");
    const eventPromise = new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(new Error("event was not posted")), 2000);
        events.onmessage = event => {
            if (event.data?.target === id) {
                clearTimeout(timer);
                resolve(event.data);
            }
        };
    });

    await CanvasHost.attach({
        mode: "worker",
        id,
        globalName: "__test_worker_canvas",
        width: 100,
        height: 50,
        hidpi: true,
        statusText: "Worker ready"
    });

    assert.equal(stored.name, "__test_worker_canvas");
    assert.equal(stored.value.width, 200);
    assert.equal(stored.value.height, 100);
    assert.equal(globalThis.__test_worker_canvas, stored.value);
    assert.equal(globalThis[`__canvas_host_pixel_width_${id}`], 200);
    assert.equal(globalThis[`__canvas_host_pixel_height_${id}`], 100);
    assert.equal(statusText(id), "Worker ready");

    canvas.dispatchEvent(pointerEvent("pointerdown", {
        clientX: 30,
        clientY: 20,
        button: 0,
        buttons: 1,
        pointerId: 7
    }));

    const data = await eventPromise;
    assert.equal(data.type, "pointerdown");
    assert.equal(data.x, 20);
    assert.equal(data.y, 15);
    assert.equal(data.deviceX, 40);
    assert.equal(data.deviceY, 30);
    assert.equal(data.cssWidth, 100);
    assert.equal(data.cssHeight, 50);
    assert.equal(data.pixelWidth, 200);
    assert.equal(data.pixelHeight, 100);
    assert.equal(data.dpr, 2);
    assert.equal(data.pointerId, 7);

    canvas.dispatchEvent(pointerEvent("pointercancel", { pointerId: 7 }));
    assert.equal(canvas.captured.size, 0);

    events.close();
    CanvasHost.stop(id);
    await waitFor(() => CanvasHost.get(id) === undefined);
});

test("main attach resolves nested start args and stops the app", async () => {
    const id = "main-args";
    addCanvas(id);

    let started;
    let stopped = false;
    globalThis.__test_factory = options => ({
        options,
        start(...args) {
            started = args;
        },
        stop() {
            stopped = true;
        },
        handleCanvasMessage() {}
    });

    await CanvasHost.attach({
        mode: "main",
        id,
        width: 320,
        height: 200,
        hidpi: false,
        factory: "__test_factory",
        start: "start",
        startArgs: ["${canvasId}", { size: "${width}x${height}" }],
        startArgTypes: ["string", "object"],
        appGlobal: "__test_app",
        options: { theme: "dark", width: 999 },
        statusText: "App ready"
    });

    assert.deepEqual(started, [id, { size: "320x200" }]);
    assert.equal(globalThis.__test_app, CanvasHost.get(id).app);
    assert.equal(CanvasHost.get(id).app.options.width, 320);
    assert.equal(CanvasHost.get(id).app.options.theme, "dark");
    assert.equal(statusText(id), "App ready");

    CanvasHost.stop(id);
    await waitFor(() => stopped && CanvasHost.get(id) === undefined);
    assert.equal(globalThis.__test_app, undefined);
});

test("reattaching stops the previous app and removes old listeners", async () => {
    const id = "main-reattach";
    const canvas = addCanvas(id);
    let stops = 0;

    globalThis.__test_factory = () => ({
        stop() {
            stops += 1;
        }
    });

    await CanvasHost.attach({
        mode: "main",
        id,
        width: 100,
        height: 50,
        factory: "__test_factory",
        appGlobal: "__test_reattach_a"
    });
    await CanvasHost.attach({
        mode: "main",
        id,
        width: 100,
        height: 50,
        factory: "__test_factory",
        appGlobal: "__test_reattach_b"
    });

    assert.equal(stops, 1);
    assert.equal(globalThis.__test_reattach_a, undefined);
    assert.doesNotThrow(() => canvas.dispatchEvent(pointerEvent("pointermove")));

    CanvasHost.stop(id);
    await waitFor(() => stops === 2 && CanvasHost.get(id) === undefined);
});

test("stopping during attach cancels the pending app", async () => {
    await withSilencedErrors(async () => {
        const id = "main-cancel";
        addCanvas(id);
        let resolveFactory;
        let stopped = false;

        globalThis.__test_factory = () => new Promise(resolve => {
            resolveFactory = resolve;
        });

        const attaching = CanvasHost.attach({
            mode: "main",
            id,
            width: 100,
            height: 50,
            factory: "__test_factory",
            appGlobal: "__test_cancel"
        });

        await waitFor(() => typeof resolveFactory === "function");
        CanvasHost.stop(id);
        await waitFor(() => CanvasHost.get(id) === undefined);

        resolveFactory({
            stop() {
                stopped = true;
            }
        });

        await assert.rejects(attaching, /cancelled/);
        assert.equal(stopped, true);
    });
});

test("worker canvases reject duplicate global names", async () => {
    const first = "duplicate-global-a";
    const second = "duplicate-global-b";
    addCanvas(first);
    addCanvas(second);
    globalThis.storeAsGlobal = async (value, name) => {
        globalThis[name] = value;
    };

    await CanvasHost.attach({
        mode: "worker",
        id: first,
        globalName: "__duplicate_global",
        width: 10,
        height: 10
    });

    await assert.rejects(
        CanvasHost.attach({
            mode: "worker",
            id: second,
            globalName: "__duplicate_global",
            width: 10,
            height: 10
        }),
        /already in use/
    );

    CanvasHost.stop(first);
    await waitFor(() => CanvasHost.get(first) === undefined);
});

test("main attach rejects a factory that returns no app", async () => {
    await withSilencedErrors(async () => {
        const id = "main-null-app";
        addCanvas(id);
        globalThis.__test_factory = () => null;

        await assert.rejects(
            CanvasHost.attach({
                mode: "main",
                id,
                width: 10,
                height: 10,
                factory: "__test_factory"
            }),
            /must return an object/
        );
        assert.equal(CanvasHost.get(id), undefined);
    });
});

test("attach validates configuration and reports failures", async () => {
    await withSilencedErrors(async () => {
    await assert.rejects(
        CanvasHost.attach({ mode: "worker", id: "", width: 1, height: 1 }),
        /canvas id/
    );
    await assert.rejects(
        CanvasHost.attach({ mode: "worker", id: "bad-width", width: 0, height: 1 }),
        /width/
    );
    await assert.rejects(
        CanvasHost.attach({ mode: "worker", id: "missing-canvas", width: 1, height: 1, globalName: "g" }),
        /not found/
    );
    await assert.rejects(
        CanvasHost.attach({ mode: "main", id: "missing-factory", width: 1, height: 1 }),
        /factory/
    );
    await assert.rejects(
        CanvasHost.attach({
            mode: "main",
            id: "reserved-global",
            width: 1,
            height: 1,
            factory: "__test_factory",
            appGlobal: "__proto__"
        }),
        /appGlobal/
    );
    await assert.rejects(
        CanvasHost.attach({
            mode: "main",
            id: "missing-types",
            width: 1,
            height: 1,
            factory: "__test_factory",
            start: "start",
            startArgs: ["x"]
        }),
        /startArgTypes/
    );
    await assert.rejects(
        CanvasHost.attach({
            mode: "worker",
            id: "reserved-worker-global",
            width: 1,
            height: 1,
            globalName: "CanvasHost"
        }),
        /globalName/
    );

    const id = "no-store";
    addCanvas(id);
    const saved = globalThis.storeAsGlobal;
    delete globalThis.storeAsGlobal;
    await assert.rejects(
        CanvasHost.attach({ mode: "worker", id, width: 1, height: 1, globalName: "__no_store" }),
        /storeAsGlobal/
    );
    globalThis.storeAsGlobal = saved;
    assert.equal(CanvasHost.get(id), undefined);
    assert.match(statusText(id), /Canvas failed/);
    });
});

test("control messages are delivered directly and queued before start", async () => {
    const id = "control";
    addCanvas(id);
    let resolveFactory;
    const messages = [];

    globalThis.__test_factory = () => new Promise(resolve => {
        resolveFactory = () => resolve({
            handleCanvasMessage(message) {
                messages.push(message);
            }
        });
    });

    const attaching = CanvasHost.attach({
        mode: "main",
        id,
        width: 100,
        height: 50,
        factory: "__test_factory"
    });

    await waitFor(() => typeof resolveFactory === "function");
    CanvasHost.send(id, { type: "early" });
    resolveFactory();
    await attaching;

    CanvasHost.send(id, { type: "late" });
    await waitFor(() => messages.length === 2);
    assert.equal(messages[0].type, "early");
    assert.equal(messages[1].type, "late");

    CanvasHost.stop(id);
    await waitFor(() => CanvasHost.get(id) === undefined);
});

test("keyboard handling leaves Tab available for focus navigation", async () => {
    const id = "keyboard";
    const canvas = addCanvas(id);
    globalThis.__test_factory = () => ({});

    await CanvasHost.attach({
        mode: "main",
        id,
        width: 100,
        height: 50,
        factory: "__test_factory"
    });

    const tab = keyboardEvent("keydown", { key: "Tab", code: "Tab" });
    canvas.dispatchEvent(tab);
    assert.equal(tab.defaultPrevented, false);
    assert.equal(tab.propagationStopped, true);

    const arrow = keyboardEvent("keydown", { key: "ArrowLeft", code: "ArrowLeft" });
    canvas.dispatchEvent(arrow);
    assert.equal(arrow.defaultPrevented, true);

    CanvasHost.stop(id);
    await waitFor(() => CanvasHost.get(id) === undefined);
});
