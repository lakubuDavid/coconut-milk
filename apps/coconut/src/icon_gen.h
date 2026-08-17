#ifndef COCONUT_ICON_GEN_H
#define COCONUT_ICON_GEN_H

/// @file icon_gen.h
///
/// Auto-generate platform-specific icon files from a single source image.
///
/// The bundle pipeline calls generateIcons() when `icon.source` is set in the
/// config.  If no icon is configured, generateIcons() can use an embedded
/// default SVG by passing an empty string or calling writeDefaultIcon() first.
///
/// The source file can be:
///   - SVG   → rasterized via LunaSVG at every required size
///   - PNG   → decoded via stb_image, scaled to every required size
///   - JPEG  → decoded via stb_image, scaled to every required size
///   - ICNS  → largest embedded PNG extracted, scaled down for other outputs
///   - ICO   → largest embedded PNG extracted, scaled down for other outputs
///
/// Outputs produced:
///   macOS  → .icns  (Apple Icon Image, 7 sizes from 16×16 to 1024×1024)
///   Windows→ .ico   (Windows Icon, 6 sizes from 16×16 to 256×256)
///   Linux  → .pngs in share/icons/hicolor/ (9 sizes) + scalable SVG copy

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
  std::string svg;    ///< path to the scalable SVG copy (Linux, if source was SVG)
};

/// Write the embedded default Coconut Milk SVG icon to a file.
/// Returns the path it was written to.
std::string writeDefaultIcon(const std::string& dir);

/// Generate all platform icon files from a single source.
///
/// If source_path is empty, uses the embedded default Coconut Milk icon.
///
/// @param source_path  Path to the source file, or empty for default icon
/// @param out_dir      Output directory (e.g. bundle output dir)
/// @param app_id       App identifier used for freedesktop naming on Linux
///
/// On success, returns a GeneratedIcons with paths to the created files.
std::expected<GeneratedIcons, Error>
generateIcons(const std::string& source_path,
              const std::string& out_dir,
              const std::string& app_id);

} // namespace coconut::icon_gen

#endif // COCONUT_ICON_GEN_H
