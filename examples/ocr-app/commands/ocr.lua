local json = coconut.json
local fs = coconut.fs

---@command ocr_save_temp
---@description Save a base64-encoded image to a temp file for OCR
---@param params { name: string, data: string }
---@return { ok: boolean, path?: string, error?: string }
local function ocr_save_temp(params, ctx)
  local name = params.name or "image.png"
  local data = params.data or ""
  if data == "" then
    return { ok = false, error = "no image data" }
  end

  local ext = name:match("%.([^.]+)$") or "png"
  local path = os.tmpname() .. "." .. ext

  -- Write base64 data to a temp file, then decode with base64 -d.
  local b64_path = os.tmpname()
  local b64_out = io.open(b64_path, "w")
  if not b64_out then
    return { ok = false, error = "failed to write base64 temp file" }
  end
  b64_out:write(data)
  b64_out:close()

  local decoded_bin = os.tmpname()
  local cmd = string.format("base64 -d < '%s' > '%s'", b64_path, decoded_bin)
  local ok = os.execute(cmd)
  os.remove(b64_path)

  if not ok then
    return { ok = false, error = "base64 decode failed" }
  end

  local handle = io.open(decoded_bin, "rb")
  if not handle then
    return { ok = false, error = "failed to open decoded image" }
  end
  local bin = handle:read("*a")
  handle:close()

  if #bin == 0 then
    os.remove(decoded_bin)
    return { ok = false, error = "decoded image is empty" }
  end

  local out = io.open(path, "wb")
  if not out then
    os.remove(decoded_bin)
    return { ok = false, error = "failed to write image file" }
  end
  out:write(bin)
  out:close()

  os.remove(decoded_bin)

  return { ok = true, path = path }
end

---@command ocr_scan
---@description Run OCR on an image using Tesseract and return the extracted text
---@param params { image_path: string }
---@return { text: string, ok: boolean, error?: string }
local function ocr_scan(params, ctx)
  local path = params.image_path
  if not path or not fs.exists(path) then
    return { ok = false, error = "image not found: " .. tostring(path) }
  end

  -- Create a temporary output path
  local tmp = os.tmpname() .. ".txt"
  -- Use stdin to feed the image to tesseract.  This avoids file-handle
  -- inheritance issues that can occur when forking from a multi-threaded
  -- process (the webview runtime) on macOS.
  local cmd = string.format(
    'tesseract - "%s" < "%s" 2>&1',
    tmp:gsub("%.txt$", ""),
    path:gsub('"', '\\"')
  )

  local handle = io.popen(cmd)
  local stderr = handle:read("*a") or ""
  local ok = handle:close()

  -- Read the output file
  local out_path = tmp:gsub("%.txt$", "") .. ".txt"
  local text = ""
  if fs.exists(out_path) then
    text = fs.readText(out_path) or ""
    os.remove(out_path)
  end

  if text == "" then
    return { ok = false, error = "OCR failed: " .. stderr, text = "" }
  end

  return { ok = true, text = text }
end

---@command ocr_save_text
---@description Save OCR text to a file
---@param params { text: string, filename: string }
---@return { ok: boolean, path?: string, error?: string }
local function ocr_save_text(params, ctx)
  local filename = params.filename or "ocr-output.txt"
  local text = params.text or ""
  local path = filename

  local ok = fs.writeText(path, text)
  if ok then
    return { ok = true, path = path }
  else
    return { ok = false, error = "failed to write file" }
  end
end

return {
  ocr_save_temp = ocr_save_temp,
  ocr_scan = ocr_scan,
  ocr_save_text = ocr_save_text,
}
