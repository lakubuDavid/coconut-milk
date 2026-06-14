#ifndef COCONUT_ICON_GEN_H
#define COCONUT_ICON_GEN_H

/// @file icon_gen.h
///
/// Auto-generate platform-specific icon files from a single source image.
///
/// The bundle pipeline calls generateIcons() when `icon.source` is set in the
/// config.  The source file can be:
///   - SVG   → rasterized via LunaSVG at every required size
///   - PNG   → decoded via stb_image, scaled to every required size
///   - JPEG  → decoded via stb_image, scaled to every required size
///   - ICNS  → largest embedded PNG extracted, scaled down for other outputs
///   - ICO   → largest embedded PNG extracted, scaled down for other outputs
///
/// Outputs produced:
///   macOS  → .icns  (Apple Icon Image, 10 sizes from 16×16 to 1024×1024)
///   Windows→ .ico   (Windows Icon, 6 sizes from 16×16 to 256×256)
///   Linux  → .png   (512×512 + scalable SVG if source was SVG)
///         plus freedesktop directory tree under share/icons/hicolor/

#include "error.h"

#include <expected>
#include <string>

namespace coconut::icon_gen {

/// Icon source format, auto-detected from file extension / magic bytes.
enum class SourceFormat {
  Unknown,
  Svg,
  Png,
  Jpeg,
  Icns,
  Ico,
};

/// Detect the icon source format from a file path.
/// Checks extension first, then reads magic bytes on ambiguity.
SourceFormat detectFormat(const std::string& path);

/// Paths to the icon files that were generated.
struct GeneratedIcons {
  std::string icns;   ///< path to generated .icns (empty if generation skipped)
  std::string ico;    ///< path to generated .ico
  std::string png;    ///< path to the largest generated .png (usually 512×512)
};

/// Generate all platform icon files from a single source.
///
/// @param source_path  Path to the source file (SVG, PNG, JPEG, ICNS, or ICO)
/// @param out_dir      Output directory (e.g. bundle output dir)
/// @param app_id       App identifier used for freedesktop naming on Linux
///
/// On success, returns a GeneratedIcons with paths to the created files.
/// On failure, returns an Error describing what went wrong.
/// Missing / unreadable source is a soft error (caller decides whether to abort).
std::expected<GeneratedIcons, Error>
generateIcons(const std::string& source_path,
              const std::string& out_dir,
              const std::string& app_id);

} // namespace coconut::icon_gen

#endif // COCONUT_ICON_GEN_H
