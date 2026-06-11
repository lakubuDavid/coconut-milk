// Auto-generated command wrappers. Do not edit.
// Uses coconut.call() for Lua command invocation.
// Plain JS with JSDoc — no build step required.
// @ts-check

/**
 * Save a base64-encoded image to a temp file for OCR
 * @param {{name: string, data: string}} params
 * @returns {Promise<{ok: boolean, path?: string, error?: string}>}
 */
export async function ocr_save_temp(params) {
  return coconut.call("ocr_save_temp", {params});
}

/**
 * Run OCR on an image using Tesseract and return the extracted text
 * @param {{image_path: string}} params
 * @returns {Promise<{text: string, ok: boolean, error?: string}>}
 */
export async function ocr_scan(params) {
  return coconut.call("ocr_scan", {params});
}

/**
 * Save OCR text to a file
 * @param {{text: string, filename: string}} params
 * @returns {Promise<{ok: boolean, path?: string, error?: string}>}
 */
export async function ocr_save_text(params) {
  return coconut.call("ocr_save_text", {params});
}

