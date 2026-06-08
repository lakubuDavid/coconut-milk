export type CoconutPayload = Record<string, unknown>

export interface CoconutError {
  code: string
  message: string
  details?: unknown
}

export type CoconutWireCall = {
  type: "call"
  id: string
  name: string
  payload: CoconutPayload
}

export type CoconutWireReturn<T = unknown> = {
  type: "return"
  id: string
  payload: T
}

export type CoconutWireError = {
  type: "error"
  id: string
  error: CoconutError
}

export type CoconutWireEvent = {
  type: "event"
  name: string
  payload: CoconutPayload
}

export type CoconutWireReady = {
  type: "ready"
}

export type CoconutWireMessage<T = unknown> =
  | CoconutWireCall
  | CoconutWireReturn<T>
  | CoconutWireError
  | CoconutWireEvent
  | CoconutWireReady

export type CoconutCommandName = string

export interface CoconutJsAPI<TCommandName extends string = CoconutCommandName> {
  ready(): Promise<void>

  call<TResponse = unknown, TPayload extends CoconutPayload = CoconutPayload>(
    name: TCommandName,
    payload: TPayload,
  ): Promise<TResponse>

  emit<TPayload extends CoconutPayload = CoconutPayload>(
    name: string,
    payload: TPayload,
  ): Promise<void>

  on<TPayload extends CoconutPayload = CoconutPayload>(
    name: string,
    fn: (payload: TPayload) => void,
  ): () => void
}

export type CoconutCommandHelper<TParams extends CoconutPayload = CoconutPayload, TResult = unknown> =
  (payload: TParams) => Promise<TResult>

  declare global{
    interface Window {
      coconut:CoconutJsAPI
    }
  }
