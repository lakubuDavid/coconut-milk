export type CoconutPayload = Record<string, unknown>;

export interface CoconutError {
  code: string;
  message: string;
  details?: unknown;
}

export interface CoconutViewDescriptor {
  kind: "file" | "html" | "url";
  src?: string;
}

export interface CoconutWindowAPI {
  /** Minimize the window. */
  minimize(): Promise<void>;
  /** Toggle fullscreen mode. */
  toggleFullscreen(): Promise<void>;
  /** Close the window. */
  close(): Promise<void>;
}

export interface CoconutFsAPI {
  /** Read a text file from disk. */
  readText(path: string): Promise<{ ok: boolean; data?: string; error?: string }>;
}

export interface CoconutJsAPI<
  TCommandName extends string = CoconutCommandName,
> {
  /** Wait for the bridge to be ready. */
  ready(): Promise<void>;

  /** Call a Lua command. */
  call<TResponse = unknown, TPayload extends CoconutPayload = CoconutPayload>(
    name: TCommandName,
    payload?: TPayload,
  ): Promise<TResponse>;

  /** Emit an event to Lua. */
  emit<TPayload extends CoconutPayload = CoconutPayload>(
    name: string,
    payload?: TPayload,
  ): Promise<void>;

  /** Listen for events from Lua. */
  on<TPayload extends CoconutPayload = CoconutPayload>(
    name: string,
    fn: (payload: TPayload) => void,
  ): () => void;

  /**
   * Get all registered view names.
   * Use this to build dynamic navigation links.
   * @returns Array of view names (e.g., ["home", "note", "settings"])
   */
  views(): Promise<string[]>;

  /** Ping the Lua bridge for connectivity. */
  ping(): Promise<string>;

  /** Window control helpers. */
  window: CoconutWindowAPI;

  /** Filesystem helpers. */
  fs: CoconutFsAPI;
}

export type CoconutCommandHelper<
  TParams extends CoconutPayload = CoconutPayload,
  TResult = unknown,
> = (payload: TParams) => Promise<TResult>;

declare global {
  interface Window {
    coconut: CoconutJsAPI;
  }
}
