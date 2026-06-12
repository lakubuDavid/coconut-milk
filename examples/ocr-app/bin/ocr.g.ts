// Auto-generated command wrappers. Do not edit.
// Uses __coconut_call for Lua command invocation.

/**
 * Save a base64-encoded image to a temp file for OCR
 * @param params { name: string, data: string }
 * @returns { ok: boolean, path?: string, error?: string }
 */
export async function ocr_save_temp(params: { name: string, data: string }): Promise<{ ok: boolean, path?: string, error?: string }> {
  return __coconut_call("ocr_save_temp", {params});
}

/**
 * Run OCR on an image using Tesseract and return the extracted text
 * @param params { image_path: string }
 * @returns { text: string, ok: boolean, error?: string }
 */
export async function ocr_scan(params: { image_path: string }): Promise<{ text: string, ok: boolean, error?: string }> {
  return __coconut_call("ocr_scan", {params});
}

/**
 * Save OCR text to a file
 * @param params { text: string, filename: string }
 * @returns { ok: boolean, path?: string, error?: string }
 */
export async function ocr_save_text(params: { text: string, filename: string }): Promise<{ ok: boolean, path?: string, error?: string }> {
  return __coconut_call("ocr_save_text", {params});
}

