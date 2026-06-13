// Crucial: turning the file into a module so 'declare global' is accepted
export {};

/// <reference path="./generated/commands.d.ts" />

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
  /** Check if a path exists. */
  exists(path: string): Promise<{ ok: boolean; exists?: boolean; error?: string }>;
  /** Write text to a file. */
  writeText(path: string, content: string): Promise<{ ok: boolean; error?: string }>;
  /** Resolve a relative path against a root. */
  resolve(root: string, relpath: string): Promise<{ ok: boolean; data?: string; error?: string }>;
  /** List directory contents. */
  listDir(path: string): Promise<{ ok: boolean; data?: Array<{ name: string; path: string; is_dir: boolean }>; error?: string }>;
}

export interface CoconutDialogAPI {
  /** Show a native message box. */
  message(title?: string, message?: string, kind?: "info" | "warn" | "error" | "question"): Promise<{ confirmed: boolean }>;
  /** Show an open file dialog. */
  open(title?: string, multi?: boolean, chooseDir?: boolean): Promise<{ confirmed: boolean; path: string; paths: string[] }>;
  /** Show a save file dialog. */
  save(title?: string, defaultName?: string): Promise<{ confirmed: boolean; path: string }>;
}

export interface CoconutClipboardAPI {
  /** Read plain text from system clipboard. */
  read(): Promise<string>;
  /** Write plain text to system clipboard. */
  write(text: string): Promise<boolean>;
}

export interface CoconutJsAPI<
  //@ts-ignore
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

  /** Open a URL in the system-default browser. */
  openUrl(url: string): Promise<boolean>;

  /** Show a system notification. */
  notify(title: string, body: string): Promise<boolean>;

  /** Window control helpers. */
  window: CoconutWindowAPI;

  /** Filesystem helpers. */
  fs: CoconutFsAPI;

  /** Native dialog helpers. */
  dialog: CoconutDialogAPI;

  /** Clipboard read/write. */
  clipboard: CoconutClipboardAPI;
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