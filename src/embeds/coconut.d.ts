type CoconutPayload = Record<string, unknown>;
export type CoconutError = {
    code: string;
    message: string;
    details?: unknown;
};
type CoconutEventCallback<TPayload extends CoconutPayload = CoconutPayload> = (payload: TPayload) => void;
type Unsubscribe = () => void;
/**
 * Coconut frontend bridge API.
 *
 * Communication:
 * - payloads cross the bridge as JSON strings
 * - events are delivered via injected dispatcher callbacks
 */
export declare const coconut: {
    ready: () => Promise<void>;
    on: (event: string, callbackFn: CoconutEventCallback) => Unsubscribe;
    emit: (event: string, params: CoconutPayload) => Promise<void>;
    call: <TResponse = unknown>(name: string, params: CoconutPayload) => Promise<TResponse>;
};
export {};
