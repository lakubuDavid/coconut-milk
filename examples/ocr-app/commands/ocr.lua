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

  -- Decode base64
  local decoded = data
  local ext = name:match("%.([^.]+)$") or "png"
  local path = os.tmpname() .. "." .. ext

  -- Write as binary (base64 is already decoded by the bridge if needed)
  -- For now, we write the raw base64 and tesseract may handle it if
  -- the image is valid. Actually, we need to decode base64.
  local decoded_bin = os.tmpname()
  os.execute("echo " .. data .. " | base64 -d > " .. decoded_bin)
  
  local handle = io.open(decoded_bin, "rb")
  if not handle then
    return { ok = false, error = "failed to decode image" }
  end
  local bin = handle:read("*a")
  handle:close()

  local out = io.open(path, "wb")
  if not out then
    return { ok = false, error = "failed to write temp file" }
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
  local cmd = string.format(
    'tesseract "%s" "%s" 2>&1',
    path:gsub('"', '\\"'),
    tmp:gsub("%.txt$", "")
  )

  local handle = io.popen(cmd)
  local stderr = handle:read("*a") or ""
  local ok = handle:close()

  -- Read the output file
  local out_path = tmp:gsub("%.txt$", "") .. ".txt"
  local text = ""
  if fs.exists(out_path) then
    text = fs.readText(out_path) or ""
    fs.remove(out_path)
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
