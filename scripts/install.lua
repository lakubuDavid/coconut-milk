#!/usr/bin/env luajit
--- Coconut Milk Installer (Lua)
---
--- Installs the `coconut` binary and `create-coconut-app` scaffolding script.
---
--- Usage:
---   luajit install.lua
---   curl -fsSL https://raw.githubusercontent.com/lakubuDavid/coconut-milk/main/scripts/install.lua | luajit
---
--- Options (env vars):
---   COCONUT_VERSION   Tag to install (default: latest release)
---   COCONUT_HOME      Install directory (default: ~/.coconut)
---   COCONUT_DRY_RUN   Set to "1" to preview without downloading
---
--- Assumes: LuaJIT (or Lua 5.2+), and a way to fetch HTTP (curl / wget / PowerShell)
---

local REPO = "lakubuDavid/coconut-milk"
local VERSION = os.getenv("COCONUT_VERSION") or "latest"
local INSTALL_DIR = os.getenv("COCONUT_HOME") or (os.getenv("HOME") or os.getenv("USERPROFILE") or "~") .. "/.coconut"
local BIN_DIR = INSTALL_DIR .. "/bin"
local DRY_RUN = os.getenv("COCONUT_DRY_RUN") == "1"

-- ── Helpers ────────────────────────────────────────────────────────────────

local function path_join(...)
  local parts = { ... }
  return table.concat(parts, "/")
end

local function die(msg)
  io.stderr:write("Error: " .. msg .. "\n")
  os.exit(1)
end

local function log(msg)
  io.write("  " .. msg .. "\n")
end

local function info(msg)
  io.write("  \xE2\x9C\x93 " .. msg .. "\n")  -- ✓
end

local function warn(msg)
  io.write("  \xE2\x9A\xA0 " .. msg .. "\n")  -- ⚠
end

local function is_windows()
  return package.config:sub(1,1) == "\\"
end

local function has_cmd(name)
  local f = io.popen((is_windows() and "where " or "which ") .. name .. " 2>/dev/null")
  if not f then return false end
  local ok = f:read("*l")
  f:close()
  return ok and ok ~= ""
end

local function run(cmd)
  local ok = os.execute(cmd)
  if not ok then die("command failed: " .. cmd) end
  return ok
end

local function mkdir_p(dir)
  if is_windows() then
    os.execute("mkdir \"" .. dir .. "\" 2>NUL")
  else
    os.execute("mkdir -p \"" .. dir .. "\"")
  end
end

local function rm_f(path)
  if is_windows() then
    os.execute("del /f /q \"" .. path .. "\" 2>NUL")
  else
    os.execute("rm -f \"" .. path .. "\"")
  end
end

-- ── HTTP fetch ─────────────────────────────────────────────────────────────

local function http_fetch(url)
  --- Returns (content, exit_code) or (nil, error_msg).

  -- Try curl first (macOS, Linux, modern Windows)
  if has_cmd("curl") then
    local tmp = os.tmpname()
    local ok = os.execute("curl -sSfLo \"" .. tmp .. "\" \"" .. url .. "\" 2>/dev/null")
    if ok then
      local f = io.open(tmp, "r")
      if f then
        local content = f:read("*a")
        f:close()
        rm_f(tmp)
        if content and #content > 0 then return content end
      end
    end
    rm_f(tmp)
  end

  -- Try wget (Linux)
  if has_cmd("wget") then
    local tmp = os.tmpname()
    local ok = os.execute("wget -qO \"" .. tmp .. "\" \"" .. url .. "\" 2>/dev/null")
    if ok then
      local f = io.open(tmp, "r")
      if f then
        local content = f:read("*a")
        f:close()
        rm_f(tmp)
        if content and #content > 0 then return content end
      end
    end
    rm_f(tmp)
  end

  -- Try PowerShell (Windows)
  if is_windows() then
    local tmp = os.tmpname()
    local ok = os.execute(
      "powershell -NoProfile -Command \"Invoke-WebRequest -Uri '" .. url ..
      "' -OutFile '" .. tmp .. "' 2>$null\" 2>/dev/null"
    )
    if ok then
      local f = io.open(tmp, "r")
      if f then
        local content = f:read("*a")
        f:close()
        rm_f(tmp)
        if content and #content > 0 then return content end
      end
    end
    rm_f(tmp)
  end

  return nil, "no HTTP tool found (tried: curl, wget, powershell)"
end

-- ── Platform detection ─────────────────────────────────────────────────────

local function detect_platform()
  local os_name
  local arch

  -- Detect OS
  local f = io.popen("uname -s 2>/dev/null")
  if f then
    os_name = f:read("*l")
    f:close()
  end

  if not os_name or os_name == "" then
    if is_windows() then
      os_name = "Windows"
    else
      die("Could not detect operating system")
    end
  end

  os_name = os_name:lower()

  local platform
  if os_name:find("linux") then
    platform = "linux"
  elseif os_name:find("darwin") or os_name:find("mac") then
    platform = "macos"
  elseif os_name:find("windows") or os_name:find("mingw") or os_name:find("msys") then
    platform = "windows"
  else
    die("Unsupported OS: " .. os_name)
  end

  -- Detect architecture
  local f2 = io.popen("uname -m 2>/dev/null")
  if f2 then
    arch = f2:read("*l")
    f2:close()
  end
  if not arch or arch == "" then
    -- On Windows, try PROCESSOR_ARCHITECTURE env var
    arch = os.getenv("PROCESSOR_ARCHITECTURE") or "x86_64"
  end

  if arch:find("x86_64") or arch:find("amd64") then
    arch = "x86_64"
  elseif arch:find("aarch64") or arch:find("arm64") then
    arch = "arm64"
  else
    die("Unsupported architecture: " .. arch)
  end

  -- Warn about unavailable builds
  if platform == "linux" and arch == "arm64" then
    warn("Linux ARM64 builds are not yet published. Falling back to x86_64.")
    arch = "x86_64"
  end
  if platform == "windows" and arch == "arm64" then
    warn("Windows ARM64 builds are not yet published. Using x86_64.")
    arch = "x86_64"
  end

  return platform, arch
end

-- ── Resolve version ───────────────────────────────────────────────────────

local function resolve_version()
  if VERSION ~= "latest" then return VERSION end

  io.write("  Resolving latest release... ")

  local api_url = "https://api.github.com/repos/" .. REPO .. "/releases?per_page=1"
  local body, err = http_fetch(api_url)
  if not body then
    die("Could not determine latest release: " .. (err or "unknown") ..
        "\n  Set COCONUT_VERSION manually (e.g. v0.1.0.alpha-1)")
  end

  -- Crude JSON parse: extract first "tag_name" value
  local tag = body:match('"tag_name"%s*:%s*"([^"]+)"')
  if not tag then
    die("Could not parse release tag from GitHub API")
  end

  VERSION = tag
  io.write(VERSION .. "\n")
  return VERSION
end

-- ── Install ────────────────────────────────────────────────────────────────

local function do_install()
  local platform, arch = detect_platform()
  local version = resolve_version()

  io.write("\n")
  io.write("  Coconut Milk " .. version .. " for " .. platform .. " (" .. arch .. ")\n")
  io.write("  Installing to: " .. BIN_DIR .. "\n")
  io.write("\n")

  if DRY_RUN then
    info("Dry run — would download:")
    info("  https://github.com/" .. REPO .. "/releases/download/" .. version .. "/coconut-" .. platform .. "-" .. arch .. ".zip")
    info("  https://raw.githubusercontent.com/" .. REPO .. "/main/scripts/create-coconut-app")
    return
  end

  mkdir_p(BIN_DIR)

  -- Build download URLs
  local archive_name = "coconut-" .. platform .. "-" .. arch .. ".zip"
  local inner_name
  local final_name

  if platform == "windows" then
    inner_name = "coconut-windows-" .. arch .. ".exe"
    final_name = "coconut.exe"
  else
    inner_name = "coconut-" .. platform .. "-" .. arch
    final_name = "coconut"
  end

  local bin_url = "https://github.com/" .. REPO .. "/releases/download/" .. version .. "/" .. archive_name

  -- Download binary archive
  io.write("  Downloading " .. archive_name .. " ... ")
  io.flush()
  local zip_data, err = http_fetch(bin_url)
  if not zip_data then
    die("Failed to download " .. bin_url .. ": " .. (err or "unknown"))
  end
  io.write("done\n")

  -- Write zip to tempfile
  local tmp_zip = os.tmpname()
  local f = io.open(tmp_zip, "wb")
  if not f then die("Cannot write temp file") end
  f:write(zip_data)
  f:close()

  -- Extract
  io.write("  Extracting ... ")
  io.flush()
  local tmp_dir = os.tmpname()
  -- Remove the temp file path so we can use it as a directory
  os.remove(tmp_dir)
  mkdir_p(tmp_dir)

  if is_windows() then
    -- Use PowerShell to expand the zip
    local ok = os.execute(
      "powershell -NoProfile -Command \"Expand-Archive -Path '" .. tmp_zip ..
      "' -DestinationPath '" .. tmp_dir .. "' -Force 2>$null\" 2>/dev/null"
    )
    if not ok then
      ok = os.execute("tar -xf \"" .. tmp_zip .. "\" -C \"" .. tmp_dir .. "\" 2>/dev/null")
    end
    if not ok then
      die("Could not extract zip. Install tar or 7zip and try again.")
    end
  else
    local ok
    if has_cmd("unzip") then
      ok = os.execute("unzip -qo \"" .. tmp_zip .. "\" -d \"" .. tmp_dir .. "\" 2>/dev/null")
    else
      ok = os.execute("tar -xf \"" .. tmp_zip .. "\" -C \"" .. tmp_dir .. "\" 2>/dev/null")
    end
    if not ok then
      die("Could not extract zip. Install unzip and try again.")
    end
  end
  rm_f(tmp_zip)
  io.write("done\n")

  -- Find the extracted binary — look for any executable file
  local function find_binary(dir, patterns)
    for _, pat in ipairs(patterns) do
      local p
      if is_windows() then
        p = io.popen("dir /s /b \"" .. dir .. "\\" .. pat .. "\" 2>NUL")
      else
        p = io.popen("find \"" .. dir .. "\" -type f -name \"" .. pat .. "\" 2>/dev/null | head -1")
      end
      if p then
        local line = p:read("*l")
        p:close()
        if line and line ~= "" then return line end
      end
    end
    -- Last resort: return any file (should be the binary)
    local p = io.popen("ls -1 \"" .. dir .. "\" 2>/dev/null | head -1")
    if p then
      local name = p:read("*l")
      p:close()
      if name and name ~= "" then return dir .. "/" .. name end
    end
    return nil
  end

  local search_patterns = {
    inner_name,                          -- coconut-macos-x86_64
    inner_name .. "-unstripped",         -- coconut-macos-x86_64-unstripped (CI artifacts)
    (platform == "windows") and "coconut.exe" or "coconut",
    (platform == "windows") and "*.exe" or "*",
  }
  local extracted = find_binary(tmp_dir, search_patterns)

  if not extracted then
    local contents = ""
    local p = io.popen("ls -la \"" .. tmp_dir .. "\" 2>/dev/null")
    if p then contents = p:read("*a") or ""; p:close() end
    die("Binary not found in archive. Contents:\n" .. contents)
  end

  -- Install binary
  io.write("  Installing binary ... ")
  io.flush()
  os.execute("cp \"" .. extracted .. "\" \"" .. BIN_DIR .. "/" .. final_name .. "\"")
  os.execute("chmod +x \"" .. BIN_DIR .. "/" .. final_name .. "\"")
  -- Clean up temp dir
  if is_windows() then
    os.execute("rmdir /s /q \"" .. tmp_dir .. "\" 2>NUL")
  else
    os.execute("rm -rf \"" .. tmp_dir .. "\"")
  end
  io.write("done\n")

  -- Install create-coconut-app script
  io.write("  Installing create-coconut-app ... ")
  io.flush()
  local script_url = "https://raw.githubusercontent.com/" .. REPO .. "/main/scripts/create-coconut-app"
  local script_content, script_err = http_fetch(script_url)
  if not script_content then
    warn("Failed to download create-coconut-app: " .. (script_err or "unknown"))
  else
    local script_path = BIN_DIR .. "/create-coconut-app"
    local sf = io.open(script_path, "w")
    if sf then
      sf:write(script_content)
      sf:close()
      os.execute("chmod +x \"" .. script_path .. "\"")
      io.write("done\n")
    else
      warn("Cannot write " .. script_path)
    end
  end

  -- Summary
  io.write("\n")
  info("Installed coconut " .. version .. " to " .. BIN_DIR .. "/" .. final_name)
  info("Installed create-coconut-app to " .. BIN_DIR .. "/create-coconut-app")

  io.write("\n")
  io.write("  \xE2\x94\x80\xE2\x94\x80 Add to PATH\n")
  io.write("\n")
  io.write("      export PATH=\"$PATH:" .. BIN_DIR .. "\"\n")
  io.write("\n")
  io.write("  Then run:\n")
  io.write("\n")
  io.write("      coconut --version\n")
  io.write("      create-coconut-app --help\n")
  io.write("\n")

  -- Offer to auto-add to shell rc
  local path_env = 'export PATH="$PATH:' .. BIN_DIR .. '"'
  if not is_windows() then
    for _, rc in ipairs({ os.getenv("HOME") .. "/.bashrc", os.getenv("HOME") .. "/.zshrc", os.getenv("HOME") .. "/.profile" }) do
      local rf = io.open(rc, "r")
      if rf then
        local content = rf:read("*a")
        rf:close()
        if not content:find(BIN_DIR) then
          warn("To auto-add to PATH, run:")
          warn("  echo '" .. path_env .. "' >> " .. rc)
          break
        end
      end
    end
  end

  io.write("\n")
  info("Done \xE2\x80\x94 happy building!\n")
end

-- ── Main ───────────────────────────────────────────────────────────────────

do_install()
