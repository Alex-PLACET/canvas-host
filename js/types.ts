/**
 * Shared types and public contracts for the canvas-host browser runtime.
 */

namespace CanvasHostRuntime {
    export type Mode = "worker" | "main";

    export interface CanvasHostConfig {
        /** Hosting mode. */
        readonly mode: Mode;
        /** Canvas element ID. */
        readonly id: string;
        /** Worker global that receives the OffscreenCanvas. */
        readonly globalName?: string;
        /** Canvas width in CSS pixels. */
        readonly width: number;
        /** Canvas height in CSS pixels. */
        readonly height: number;
        /** Use devicePixelRatio for the backing store. */
        readonly hidpi?: boolean;
        /** Initial status text. */
        readonly statusText?: string;
        /** JavaScript file that defines the application factory. */
        readonly script?: string;
        /** Global application factory name. */
        readonly factory?: string;
        /** Optional start function exported by the application. */
        readonly start?: string;
        /** Values passed to the start function. */
        readonly startArgs?: readonly unknown[];
        /** Emscripten ccall argument types; required when startArgs is non-empty. */
        readonly startArgTypes?: readonly string[];
        /** Emscripten ccall return type. */
        readonly startReturnType?: string;
        /** Global control-message handler name. */
        readonly controlHandler?: string;
        /** Global callback invoked after start. */
        readonly onStart?: string;
        /** Global that receives the application object. */
        readonly appGlobal?: string;
        /** Extra options passed to the application factory. */
        readonly options?: Readonly<Record<string, unknown>>;
    }

    export interface CanvasHostInstance {
        /** Canvas element ID. */
        readonly id: string;
        /** Canvas element. */
        readonly canvas: HTMLCanvasElement;
        /** Worker global name for worker mode. */
        readonly globalName?: string;
        /** Hosting mode. */
        readonly mode: Mode;
        /** Main-thread application object. */
        app: MainApp | null;
        /** Global that receives the application object. */
        appGlobal: string | null;
        /** Control-message handler. */
        controlHandler: ControlHandler | null;
        /** Input event channel. */
        readonly events: BroadcastChannel;
        /** Status channel. */
        readonly status: BroadcastChannel;
        /** Control channel. */
        control: BroadcastChannel | null;
        /** Control messages queued before start. */
        readonly pending: ControlMessage[];
        /** Removes all DOM listeners when the instance is closed. */
        readonly abort: AbortController;
        /** True once the instance has been stopped or replaced. */
        closed: boolean;
    }

    export interface CanvasHostHandle {
        /** Canvas element ID. */
        readonly id: string;
        /** Hosting mode. */
        readonly mode: Mode;
    }

    export interface CanvasHostInputEvent {
        /** Canvas ID. */
        readonly target: string;
        /** Event type. */
        readonly type: string;
        /** Epoch milliseconds when the DOM event was captured. */
        readonly eventTime: number;
        /** Pointer X in CSS pixels. */
        readonly x: number;
        /** Pointer Y in CSS pixels. */
        readonly y: number;
        /** Pointer X in device pixels. */
        readonly deviceX: number;
        /** Pointer Y in device pixels. */
        readonly deviceY: number;
        /** Canvas CSS width. */
        readonly cssWidth: number;
        /** Canvas CSS height. */
        readonly cssHeight: number;
        /** Canvas backing-store width. */
        readonly pixelWidth: number;
        /** Canvas backing-store height. */
        readonly pixelHeight: number;
        /** Device pixel ratio. */
        readonly dpr: number;
    }

    export interface ControlMessage {
        readonly type?: string;
        readonly target?: string;
        readonly [key: string]: unknown;
    }

    export interface MainApp {
        readonly stop?: () => void | Promise<void>;
        readonly handleCanvasMessage?: (message: ControlMessage) => void | Promise<void>;
        readonly ccall?: (
            name: string,
            returnType: string,
            argTypes: readonly string[],
            args: readonly unknown[]
        ) => unknown;
        readonly [key: string]: unknown;
    }

    export type ControlHandler = (
        app: MainApp | null,
        message: ControlMessage
    ) => void | Promise<void>;

    export type StartCallback = (
        app: MainApp | null,
        info: {
            canvas: HTMLCanvasElement;
            canvasId: string;
            width: number;
            height: number;
            dpr: number;
        }
    ) => void | Promise<void>;

    export type AppFactory = (
        options: Record<string, unknown>
    ) => MainApp | Promise<MainApp>;

    export interface CanvasHostApi {
        attach(config: CanvasHostConfig): Promise<CanvasHostHandle>;
        send(id: string, message: ControlMessage): void;
        stop(id: string): void;
        get(id: string): CanvasHostInstance | undefined;
        list(): string[];
    }

    export type Root = typeof globalThis & {
        CanvasHost?: CanvasHostApi;
        storeAsGlobal?: (
            value: OffscreenCanvas,
            name: string
        ) => void | Promise<void>;
        devicePixelRatio?: number;
        [key: string]: unknown;
    };
}
