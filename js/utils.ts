/**
 * Shared runtime utilities: globals, validation, argument substitution, and script loading.
 */

namespace CanvasHostRuntime {
    export function setGlobal(name: string, value: unknown): void {
        Object.defineProperty(root, name, {
            value,
            configurable: true,
            writable: true,
            enumerable: true
        });
    }

    export function deleteGlobal(name: string): void {
        delete root[name];
    }

    export function validateDimension(value: number, name: string): void {
        if (!Number.isFinite(value) || value <= 0) {
            throw new Error(`canvas-host: ${name} must be a positive number`);
        }
    }

    export function validateGlobalName(name: string, field: string): void {
        if (!name || RESERVED_GLOBALS.has(name)) {
            throw new Error(`canvas-host: invalid ${field}: ${name}`);
        }
    }

    export const isRecord = (value: unknown): value is Record<string, unknown> =>
        typeof value === "object" && value !== null && !Array.isArray(value);

    export const resolveArg = (
        value: unknown,
        vars: { canvasId: string; width: number; height: number; dpr: number }
    ): unknown => {
        if (typeof value === "string") {
            return value.replace(
                /\$\{(canvasId|width|height|dpr)\}/g,
                (_, key) => String(vars[key as keyof typeof vars])
            );
        }
        if (Array.isArray(value)) {
            return value.map(item => resolveArg(item, vars));
        }
        if (isRecord(value)) {
            const result: Record<string, unknown> = {};
            for (const [key, item] of Object.entries(value)) {
                result[key] = resolveArg(item, vars);
            }
            return result;
        }
        return value;
    };

    const scripts = new Map<string, Promise<void>>();

    export function loadScript(src: string): Promise<void> {
        if (!scripts.has(src)) {
            scripts.set(src, new Promise((resolve, reject) => {
                const script = document.createElement("script");
                script.src = src;
                script.onload = () => resolve();
                script.onerror = () => {
                    scripts.delete(src);
                    reject(new Error(`failed to load ${src}`));
                };
                document.head.appendChild(script);
            }));
        }
        return scripts.get(src)!;
    }
}
