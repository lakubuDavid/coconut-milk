/**
@description Save a base64-encoded image to a temp file for OCR
*/
declare function ocr_save_temp(params:{ name: string, data: string },) : Promise<[{ ok: boolean, path?: string, error?: string }]>;
/**
@description Run OCR on an image using Tesseract and return the extracted text
*/
declare function ocr_scan(params:{ image_path: string },) : Promise<[{ text: string, ok: boolean, error?: string }]>;
/**
@description Save OCR text to a file
*/
declare function ocr_save_text(params:{ text: string, filename: string },) : Promise<[{ ok: boolean, path?: string, error?: string }]>;
