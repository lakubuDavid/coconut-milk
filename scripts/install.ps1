# ── Coconut Milk Installer (PowerShell) ─────────────────────────────────────
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -Command "iex ((New-Object Net.WebClient).DownloadString('https://raw.githubusercontent.com/lakubuDavid/coconut-milk/main/scripts/install.ps1'))"
#
# Or download and run:
#   curl.exe -fsSLo install.ps1 https://raw.githubusercontent.com/lakubuDavid/coconut-milk/main/scripts/install.ps1
#   powershell -NoProfile -ExecutionPolicy Bypass -File install.ps1
#
# Options (env vars):
#   $env:COCONUT_VERSION   Tag to install (default: latest release)
#   $env:COCONUT_HOME      Install directory (default: ~\.coconut)
#   $env:COCONUT_DRY_RUN   Set to "1" to preview without downloading
# ─────────────────────────────────────────────────────────────────────────────

$Repo = "lakubuDavid/coconut-milk"
$Version = if ($env:COCONUT_VERSION) { $env:COCONUT_VERSION } else { "latest" }
$InstallDir = if ($env:COCONUT_HOME) { $env:COCONUT_HOME } else { "$env:USERPROFILE\.coconut" }
$BinDir = "$InstallDir\bin"
$DryRun = $env:COCONUT_DRY_RUN -eq "1"

function Write-Info($msg) { Write-Host "  ✓ $msg" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "  ⚠ $msg" -ForegroundColor Yellow }
function Write-Err($msg) { Write-Host "  ✗ $msg" -ForegroundColor Red; exit 1 }
function Write-Step($msg) { Write-Host "  $msg" -NoNewline }

# ── Platform detection ─────────────────────────────────────────────────────

function Get-Platform {
  $arch = $env:PROCESSOR_ARCHITECTURE
  if ($arch -eq "AMD64") { $arch = "x86_64" }
  elseif ($arch -eq "ARM64") { $arch = "arm64" }
  else { Write-Err "Unsupported architecture: $arch (only x86_64, arm64)" }

  if ($arch -eq "arm64") {
    Write-Warn "Windows ARM64 builds are not yet published. Using x86_64."
    $arch = "x86_64"
  }

  return @{ Platform = "windows"; Arch = $arch }
}

# ── Resolve version ────────────────────────────────────────────────────────

function Resolve-Version {
  if ($Version -ne "latest") { return $Version }

  Write-Host "  Resolving latest release... " -NoNewline
  try {
    $apiUrl = "https://api.github.com/repos/$Repo/releases?per_page=1"
    $json = Invoke-RestMethod -Uri $apiUrl -UseBasicParsing -ErrorAction Stop
    $tag = $json[0].tag_name
    if ($tag) {
      $script:Version = $tag
      Write-Host $tag -ForegroundColor Cyan
      return $tag
    }
  } catch {
    # fall through
  }

  Write-Err "Could not determine latest release.`n  Set COCONUT_VERSION manually (e.g. v0.1.0.alpha-1)"
}

# ── Install ─────────────────────────────────────────────────────────────────

function Install-Coconut {
  $platform = Get-Platform
  $version = Resolve-Version

  Write-Host "`n  Coconut Milk $version for Windows ($($platform.Arch))"
  Write-Host "  Installing to: $BinDir`n"

  if ($DryRun) {
    Write-Info "Dry run — would download:"
    Write-Info "  https://github.com/$Repo/releases/download/$version/coconut-windows-$($platform.Arch).zip"
    Write-Info "  https://raw.githubusercontent.com/$Repo/main/scripts/create-coconut-app"
    return
  }

  # Create bin dir
  New-Item -ItemType Directory -Force -Path $BinDir | Out-Null

  # Build URLs
  $archiveName = "coconut-windows-$($platform.Arch).zip"
  $binUrl = "https://github.com/$Repo/releases/download/$version/$archiveName"
  $scriptUrl = "https://raw.githubusercontent.com/$Repo/main/scripts/create-coconut-app"

  # ── Download binary archive ────────────────────────────────────────────
  Write-Step "Downloading $archiveName ... "
  $tmpZip = [System.IO.Path]::GetTempFileName()
  try {
    Invoke-WebRequest -Uri $binUrl -OutFile $tmpZip -UseBasicParsing -ErrorAction Stop
  } catch {
    # Fall back to curl.exe if Invoke-WebRequest fails (e.g. Windows Server Core)
    try {
      curl.exe -fsSLo $tmpZip $binUrl 2>$null
    } catch {
      Write-Err "Failed to download $binUrl. Check your internet connection."
    }
  }
  Write-Host "done" -ForegroundColor Green

  # ── Extract ────────────────────────────────────────────────────────────
  Write-Step "Extracting ... "
  $tmpDir = [System.IO.Path]::GetTempPath() + [System.Guid]::NewGuid().ToString()
  New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null
  try {
    Expand-Archive -Path $tmpZip -DestinationPath $tmpDir -Force -ErrorAction Stop
  } catch {
    try {
      tar -xf $tmpZip -C $tmpDir 2>$null
    } catch {
      Write-Err "Could not extract zip. Install 7zip or tar and try again."
    }
  }
  Remove-Item $tmpZip -Force -ErrorAction SilentlyContinue
  Write-Host "done" -ForegroundColor Green

  # ── Find binary ────────────────────────────────────────────────────────
  Write-Step "Installing binary ... "
  $binary = $null
  # Try multiple naming patterns
  $patterns = @(
    "coconut-windows-$($platform.Arch).exe",
    "coconut-windows-$($platform.Arch)-unstripped.exe",
    "coconut.exe",
    "*.exe"
  )
  foreach ($pattern in $patterns) {
    $found = Get-ChildItem -Path $tmpDir -Recurse -Filter $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $binary = $found.FullName; break }
  }

  if (-not $binary) {
    # Last resort: grab any file
    $any = Get-ChildItem -Path $tmpDir -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($any) { $binary = $any.FullName }
  }

  if (-not $binary) {
    $contents = Get-ChildItem -Path $tmpDir -Recurse -ErrorAction SilentlyContinue | Out-String
    Write-Err "Binary not found in archive. Contents:`n$contents"
  }

  # Copy binary
  Copy-Item -Path $binary -Destination "$BinDir\coconut.exe" -Force
  Remove-Item $tmpDir -Recurse -Force -ErrorAction SilentlyContinue
  Write-Host "done" -ForegroundColor Green

  # ── Install create-coconut-app ─────────────────────────────────────────
  Write-Step "Installing create-coconut-app ... "
  try {
    Invoke-WebRequest -Uri $scriptUrl -OutFile "$BinDir\create-coconut-app" -UseBasicParsing -ErrorAction Stop
  } catch {
    try {
      curl.exe -fsSLo "$BinDir\create-coconut-app" $scriptUrl 2>$null
    } catch {
      Write-Warn "Failed to download create-coconut-app. You can manually copy it from the repo."
    }
  }
  Write-Host "done" -ForegroundColor Green

  # ── Summary ─────────────────────────────────────────────────────────────
  Write-Host ""
  Write-Info "Installed coconut $version to $BinDir\coconut.exe"
  Write-Info "Installed create-coconut-app to $BinDir\create-coconut-app"

  Write-Host "`n  ── Add to PATH`n"
  Write-Host "  Run the following in PowerShell:"
  Write-Host ""
  Write-Host "      `$env:Path += `";$BinDir`""
  Write-Host ""
  Write-Host "  To make it permanent, add to your PowerShell profile:"
  Write-Host ""
  Write-Host "      [Environment]::SetEnvironmentVariable(`"Path`","
  Write-Host "        `$env:Path + `";$BinDir`", [EnvironmentVariableTarget]::User)"
  Write-Host ""
  Write-Host "  Then run:"
  Write-Host ""
  Write-Host "      coconut --version"
  Write-Host "      create-coconut-app --help"
  Write-Host ""
  Write-Info "Done — happy building!`n"
}

# ── Main ───────────────────────────────────────────────────────────────────

Install-Coconut
