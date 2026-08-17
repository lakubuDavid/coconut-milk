#include "icon_gen.h"
#include "embeds/default_icon_png.h"

#include "error.h"
#include "fs.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#if defined(_MSC_VER)
#  include <intrin.h>
#endif
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

// ── Third-party library implements ───────────────────────────────────────

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <lunasvg.h>

namespace coconut::icon_gen {

namespace fs = std::filesystem;

// ══════════════════════════════════════════════════════════════════════════
// Internal types & helpers
// ══════════════════════════════════════════════════════════════════════════

/// A single RGBA pixel buffer at a given size.
struct RenderedImage {
  int width  = 0;
  int height = 0;
  std::vector<uint8_t> pixels;  // RGBA, row-major, 4 bytes per pixel
};

/// A PNG-encoded icon at a given nominal size (width == height).
struct PngIcon {
  int size;
  std::vector<uint8_t> data;  // raw PNG file bytes
};

// ── Write callback for stb_image_write (appends to a vector<uint8_t>) ──

struct AppendBuf {
  std::vector<uint8_t>* buf;
};

static void stbAppend(void* ctx, void* data, int size) {
  auto* ab = static_cast<AppendBuf*>(ctx);
  auto* bytes = static_cast<uint8_t*>(data);
  ab->buf->insert(ab->buf->end(), bytes, bytes + size);
}

// ── Bilinear RGBA scaler ────────────────────────────────────────────────

static RenderedImage scaleImage(const RenderedImage& src, int dstW, int dstH) {
  RenderedImage dst;
  dst.width  = dstW;
  dst.height = dstH;
  dst.pixels.resize(static_cast<size_t>(dstW) * static_cast<size_t>(dstH) * 4);

  if (src.width == 0 || src.height == 0) return dst;

  double xRatio = static_cast<double>(src.width)  / dstW;
  double yRatio = static_cast<double>(src.height) / dstH;

  for (int y = 0; y < dstH; ++y) {
    for (int x = 0; x < dstW; ++x) {
      double srcX = (x + 0.5) * xRatio - 0.5;
      double srcY = (y + 0.5) * yRatio - 0.5;

      // Clamp to edges
      int x0 = std::max(0, static_cast<int>(std::floor(srcX)));
      int y0 = std::max(0, static_cast<int>(std::floor(srcY)));
      int x1 = std::min(src.width  - 1, x0 + 1);
      int y1 = std::min(src.height - 1, y0 + 1);

      double fx = srcX - x0;
      double fy = srcY - y0;

      for (int c = 0; c < 4; ++c) {
        auto get = [&](int px, int py) -> double {
          return src.pixels[static_cast<size_t>(py) * src.width * 4 + px * 4 + c];
        };
        double top    = get(x0, y0) * (1.0 - fx) + get(x1, y0) * fx;
        double bottom = get(x0, y1) * (1.0 - fx) + get(x1, y1) * fx;
        double val    = top * (1.0 - fy) + bottom * fy;
        dst.pixels[static_cast<size_t>(y) * dstW * 4 + x * 4 + c] =
            static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
      }
    }
  }
  return dst;
}

// ── RGBA → in-memory PNG ────────────────────────────────────────────────

static std::expected<std::vector<uint8_t>, Error> encodePng(const RenderedImage& img) {
  std::vector<uint8_t> buf;
  AppendBuf ctx{&buf};
  int result = stbi_write_png_to_func(stbAppend, &ctx,
                                      img.width, img.height, 4,
                                      img.pixels.data(),
                                      img.width * 4);
  if (result == 0) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = "stb_image_write: failed to encode PNG",
    });
  }
  return buf;
}

// ══════════════════════════════════════════════════════════════════════════
// Format detection
// ══════════════════════════════════════════════════════════════════════════

SourceFormat detectFormat(const std::string& path) {
  // 1) Extension-based detection
  auto lower = [](std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  };

  std::string ext;
  auto dot = path.find_last_of('.');
  if (dot != std::string::npos) {
    ext = lower(path.substr(dot));
  }

  if (ext == ".svg")  return SourceFormat::Svg;
  if (ext == ".png")  return SourceFormat::Png;
  if (ext == ".jpg" || ext == ".jpeg") return SourceFormat::Jpeg;
  if (ext == ".icns") return SourceFormat::Icns;
  if (ext == ".ico")  return SourceFormat::Ico;

  // 2) Fallback: read magic bytes
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) return SourceFormat::Unknown;

  uint8_t magic[8]{};
  f.read(reinterpret_cast<char*>(magic), 8);
  if (f.gcount() < 4) return SourceFormat::Unknown;

  if (magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47)
    return SourceFormat::Png;
  if (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF)
    return SourceFormat::Jpeg;
  if (magic[0] == 'i' && magic[1] == 'c' && magic[2] == 'n' && magic[3] == 's')
    return SourceFormat::Icns;
  if (magic[0] == 0x00 && magic[1] == 0x00 && magic[2] == 0x01 && magic[3] == 0x00)
    return SourceFormat::Ico;
  if (magic[0] == 0x00 && magic[1] == 0x00 && magic[2] == 0x02 && magic[3] == 0x00)
    return SourceFormat::Ico;  // CUR format (almost identical)

  // Check for XML/SVG preamble
  if ((magic[0] == '<' && magic[1] == 's' && magic[2] == 'v' && magic[3] == 'g') ||
      (magic[0] == '<' && magic[1] == '?' && magic[2] == 'x' && magic[3] == 'm'))
    return SourceFormat::Svg;

  return SourceFormat::Unknown;
}

// ══════════════════════════════════════════════════════════════════════════
// Loaders
// ══════════════════════════════════════════════════════════════════════════

// ── SVG loader (via LunaSVG) ────────────────────────────────────────────

static std::expected<RenderedImage, Error> loadSvg(const std::string& path, int size) {
  auto doc = lunasvg::Document::loadFromFile(path);
  if (doc == nullptr) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("failed to load SVG: {}", path),
    });
  }

  auto bitmap = doc->renderToBitmap(size, size);
  if (bitmap.isNull()) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("failed to render SVG at {}×{}: {}", size, size, path),
    });
  }

  RenderedImage img;
  img.width  = bitmap.width();
  img.height = bitmap.height();
  img.pixels.assign(bitmap.data(), bitmap.data() + bitmap.stride() * bitmap.height());
  return img;
}

// ── Raster loader (PNG / JPEG via stb_image) ────────────────────────────

static std::expected<RenderedImage, Error> loadRaster(const std::string& path) {
  int w = 0, h = 0, comp = 0;
  unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
  if (data == nullptr) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("failed to load image ({}): {}", path,
                             stbi_failure_reason()),
    });
  }

  RenderedImage img;
  img.width  = w;
  img.height = h;
  img.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
  stbi_image_free(data);
  return img;
}

// ── ICNS parser ─────────────────────────────────────────────────────────

/// Find the largest PNG icon embedded in an ICNS file and return its raw data.
static std::expected<std::vector<uint8_t>, Error> extractLargestPngFromIcns(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = std::format("cannot open ICNS: {}", path),
    });
  }

  // Read header
  char magic[4]{};
  uint32_t totalSize = 0;
  f.read(magic, 4);
  if (std::memcmp(magic, "icns", 4) != 0) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("not a valid ICNS file: {}", path),
    });
  }
  f.read(reinterpret_cast<char*>(&totalSize), 4);
#if defined(_MSC_VER)
    totalSize = _byteswap_ulong(totalSize);
#else
    totalSize = __builtin_bswap32(totalSize);  // big-endian → host
#endif

  // Priority order: larger OSType = larger image (ic10 > ic09 > ...)
  // We prefer ic10 (1024) > ic09 (512) > ic08 (256) > ic07 (128) > ...
  // and also ic14 (512@2x) > ic13 (256@2x) > ...
  // Simplification: just pick the largest data block.

  std::vector<uint8_t> bestData;
  size_t bestSize = 0;

  while (f.tellg() < static_cast<std::streamoff>(totalSize) - 8) {
    char blockType[4]{};
    uint32_t blockSize = 0;
    f.read(blockType, 4);
    f.read(reinterpret_cast<char*>(&blockSize), 4);
#if defined(_MSC_VER)
    blockSize = _byteswap_ulong(blockSize);
#else
    blockSize = __builtin_bswap32(blockSize);
#endif
    if (blockSize < 8) break;  // malformed

    uint32_t dataSize = blockSize - 8;
    // Check if this is a PNG OSType: icp4-icp6, ic07-ic14
    bool isPng = (blockType[0] == 'i' && blockType[1] == 'c');
    if (isPng && dataSize > bestSize) {
      std::vector<uint8_t> buf(dataSize);
      auto saved = f.tellg();
      f.read(reinterpret_cast<char*>(buf.data()), dataSize);
      if (f.gcount() == static_cast<std::streamsize>(dataSize)) {
        bestData = std::move(buf);
        bestSize = dataSize;
      }
      f.seekg(saved);  // rewind to read normally
    }
    f.seekg(dataSize, std::ios::cur);
  }

  if (bestData.empty()) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("no PNG icons found in ICNS: {}", path),
    });
  }

  return bestData;
}

/// Parse ICNS → extract largest PNG → decode to RGBA.
static std::expected<RenderedImage, Error> loadIcns(const std::string& path) {
  auto pngData = extractLargestPngFromIcns(path);
  if (!pngData) return std::unexpected(pngData.error());

  int w = 0, h = 0, comp = 0;
  unsigned char* data = stbi_load_from_memory(
      pngData->data(), static_cast<int>(pngData->size()), &w, &h, &comp, 4);
  if (data == nullptr) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("failed to decode PNG from ICNS ({}): {}", path,
                             stbi_failure_reason()),
    });
  }

  RenderedImage img;
  img.width  = w;
  img.height = h;
  img.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
  stbi_image_free(data);
  return img;
}

// ── ICO parser ──────────────────────────────────────────────────────────

/// Parse ICO → extract largest PNG entry → return raw PNG data.
static std::expected<std::vector<uint8_t>, Error> extractLargestPngFromIco(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = std::format("cannot open ICO: {}", path),
    });
  }

  // ICONDIR header (little-endian)
  struct {
    uint16_t reserved;  // must be 0
    uint16_t type;      // 1 = ICO, 2 = CUR
    uint16_t count;
  } hdr;
  f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
  if (hdr.type != 1 && hdr.type != 2) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("not a valid ICO file: {}", path),
    });
  }

  // Read directory entries
  struct DirEntry {
    uint8_t  width;        // 0 = 256
    uint8_t  height;       // 0 = 256
    uint8_t  colorCount;
    uint8_t  reserved;
    uint16_t planes;       // for ICO: color planes; for CUR: hotspot X
    uint16_t bpp;          // for ICO: bits per pixel; for CUR: hotspot Y
    uint32_t dataSize;
    uint32_t dataOffset;
  };

  int bestEntry = -1;
  uint32_t bestArea = 0;

  std::vector<DirEntry> entries(hdr.count);
  for (int i = 0; i < hdr.count; ++i) {
    auto& e = entries[i];
    f.read(reinterpret_cast<char*>(&e), sizeof(e));
    int ew = e.width  == 0 ? 256 : e.width;
    int eh = e.height == 0 ? 256 : e.height;
    uint32_t area = static_cast<uint32_t>(ew) * eh;
    if (area > bestArea) {
      bestArea = area;
      bestEntry = i;
    }
  }

  if (bestEntry < 0) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("no icons found in ICO: {}", path),
    });
  }

  const auto& entry = entries[bestEntry];
  std::vector<uint8_t> buf(entry.dataSize);
  f.seekg(entry.dataOffset);
  f.read(reinterpret_cast<char*>(buf.data()), entry.dataSize);

  if (f.gcount() != static_cast<std::streamsize>(entry.dataSize)) {
    return std::unexpected(Error{
      .code = ErrorCode::IoError,
      .message = std::format("truncated icon data in ICO: {}", path),
    });
  }

  return buf;
}

static std::expected<RenderedImage, Error> loadIco(const std::string& path) {
  auto pngData = extractLargestPngFromIco(path);
  if (!pngData) return std::unexpected(pngData.error());

  int w = 0, h = 0, comp = 0;
  unsigned char* data = stbi_load_from_memory(
      pngData->data(), static_cast<int>(pngData->size()), &w, &h, &comp, 4);
  if (data == nullptr) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("failed to decode PNG from ICO ({}): {}", path,
                             stbi_failure_reason()),
    });
  }

  RenderedImage img;
  img.width  = w;
  img.height = h;
  img.pixels.assign(data, data + static_cast<size_t>(w) * h * 4);
  stbi_image_free(data);
  return img;
}

// ══════════════════════════════════════════════════════════════════════════
// Packers
// ══════════════════════════════════════════════════════════════════════════

// ── Required sizes per platform ─────────────────────────────────────────

// macOS ICNS: 16, 32, 64, 128, 256, 512, 1024
// OSType mapping: icp4=16, icp5=32, icp6=64, ic07=128, ic08=256, ic09=512, ic10=1024

struct IcnsSpec {
  int         size;
  const char* osType;  // 4-char OSType
};

static const IcnsSpec ICNS_SIZES[] = {
  {1024, "ic10"},
  { 512, "ic09"},
  { 256, "ic08"},
  { 128, "ic07"},
  {  64, "icp6"},
  {  32, "icp5"},
  {  16, "icp4"},
};

// Windows ICO: 16, 32, 48, 64, 128, 256
static const int ICO_SIZES[] = {256, 128, 64, 48, 32, 16};

// Linux PNG: 512, 256, 128, 64, 48, 32, 24, 16
static const int LINUX_SIZES[] = {512, 256, 128, 64, 48, 32, 24, 16};

// ── Write big-endian uint32 ─────────────────────────────────────────────

static void writeBE32(std::vector<uint8_t>& buf, uint32_t v) {
  buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
  buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
  buf.push_back(static_cast<uint8_t>((v >>  8) & 0xFF));
  buf.push_back(static_cast<uint8_t>((v >>  0) & 0xFF));
}

// ── Write little-endian uint16 ──────────────────────────────────────────

static void writeLE16(std::vector<uint8_t>& buf, uint16_t v) {
  buf.push_back(static_cast<uint8_t>(v & 0xFF));
  buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

// ── Write little-endian uint32 ──────────────────────────────────────────

static void writeLE32(std::vector<uint8_t>& buf, uint32_t v) {
  buf.push_back(static_cast<uint8_t>(v & 0xFF));
  buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
  buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

// ── Pack .icns ──────────────────────────────────────────────────────────

static std::expected<std::vector<uint8_t>, Error>
packIcns(const std::vector<PngIcon>& icons) {
  // Build a lookup from size → data
  std::map<int, const std::vector<uint8_t>*> bySize;
  for (const auto& icon : icons) {
    bySize[icon.size] = &icon.data;
  }

  // Compute total size
  uint32_t total = 8;  // header
  for (const auto& spec : ICNS_SIZES) {
    auto it = bySize.find(spec.size);
    if (it == bySize.end()) continue;
    total += 8 + static_cast<uint32_t>(it->second->size());
  }

  std::vector<uint8_t> out;
  out.reserve(total);

  // Header
  out.push_back('i'); out.push_back('c'); out.push_back('n'); out.push_back('s');
  writeBE32(out, total);

  // Blocks
  for (const auto& spec : ICNS_SIZES) {
    auto it = bySize.find(spec.size);
    if (it == bySize.end()) continue;
    const auto& png = *it->second;

    // OSType
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(spec.osType[i]));
    // Block size
    writeBE32(out, 8 + static_cast<uint32_t>(png.size()));
    // PNG data
    out.insert(out.end(), png.begin(), png.end());
  }

  return out;
}

// ── Pack .ico ───────────────────────────────────────────────────────────

static std::expected<std::vector<uint8_t>, Error>
packIco(const std::vector<PngIcon>& icons) {
  // Build lookup
  std::map<int, const std::vector<uint8_t>*> bySize;
  for (const auto& icon : icons) {
    bySize[icon.size] = &icon.data;
  }

  // Filter to only sizes we want in the ICO
  std::vector<PngIcon> usedIcons;
  for (int s : ICO_SIZES) {
    auto it = std::find_if(icons.begin(), icons.end(),
        [s](const PngIcon& i) { return i.size == s; });
    if (it != icons.end()) {
      usedIcons.push_back(*it);
    }
  }

  if (usedIcons.empty()) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = "no suitable icon sizes for ICO output",
    });
  }

  uint16_t count = static_cast<uint16_t>(usedIcons.size());
  uint32_t dirSize = 6 + count * 16;
  uint32_t dataOffset = dirSize;

  std::vector<uint8_t> out;
  out.reserve(dirSize + 256 * 1024);  // ~256KB should cover all PNGs

  // ICONDIR header (little-endian)
  writeLE16(out, 0);                        // reserved
  writeLE16(out, 1);                        // type = ICO
  writeLE16(out, count);                    // image count

  // Directory entries (reserve space, fill later)
  size_t entryStart = out.size();
  out.resize(out.size() + count * 16);

  // Append PNG data and fill directory entries
  for (int i = 0; i < count; ++i) {
    auto& icon = usedIcons[i];
    uint8_t w = static_cast<uint8_t>(std::min(icon.size, 255));
    if (icon.size == 256) w = 0;  // 0 means 256 in ICO

    size_t entryOff = entryStart + i * 16;
    out[entryOff + 0] = w;                                                       // width
    out[entryOff + 1] = w;                                                       // height
    out[entryOff + 2] = 0;                                                       // color count
    out[entryOff + 3] = 0;                                                       // reserved
    out[entryOff + 4] = 1;  out[entryOff + 5] = 0;                               // planes (LE16)
    out[entryOff + 6] = 32; out[entryOff + 7] = 0;                               // bpp (LE16)
    // data size (LE32)
    uint32_t ds = static_cast<uint32_t>(icon.data.size());
    out[entryOff + 8]  = static_cast<uint8_t>(ds & 0xFF);
    out[entryOff + 9]  = static_cast<uint8_t>((ds >> 8) & 0xFF);
    out[entryOff + 10] = static_cast<uint8_t>((ds >> 16) & 0xFF);
    out[entryOff + 11] = static_cast<uint8_t>((ds >> 24) & 0xFF);
    // data offset (LE32)
    out[entryOff + 12] = static_cast<uint8_t>(dataOffset & 0xFF);
    out[entryOff + 13] = static_cast<uint8_t>((dataOffset >> 8) & 0xFF);
    out[entryOff + 14] = static_cast<uint8_t>((dataOffset >> 16) & 0xFF);
    out[entryOff + 15] = static_cast<uint8_t>((dataOffset >> 24) & 0xFF);

    dataOffset += ds;
  }

  // Append PNG data
  for (auto& icon : usedIcons) {
    out.insert(out.end(), icon.data.begin(), icon.data.end());
  }

  return out;
}

// ── Write Linux icon directory ──────────────────────────────────────────

static std::expected<void, Error>
writeLinuxIcons(const std::vector<PngIcon>& icons,
                const std::string& out_dir,
                const std::string& app_id) {
  // Build lookup
  std::map<int, const std::vector<uint8_t>*> bySize;
  for (const auto& icon : icons) {
    bySize[icon.size] = &icon.data;
  }

  for (int s : LINUX_SIZES) {
    auto it = bySize.find(s);
    if (it == bySize.end()) continue;

    std::string dir = out_dir + "/share/icons/hicolor/" +
                      std::to_string(s) + "x" + std::to_string(s) + "/apps";
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
      return std::unexpected(Error{
        .code = ErrorCode::IoError,
        .message = std::format("failed to create icon dir '{}': {}", dir, ec.message()),
      });
    }

    std::string filePath = dir + "/" + app_id + ".png";
    std::ofstream f(filePath, std::ios::binary);
    if (!f.is_open()) {
      return std::unexpected(Error{
        .code = ErrorCode::IoError,
        .message = std::format("failed to write '{}'", filePath),
      });
    }
    f.write(reinterpret_cast<const char*>(it->second->data()),
            static_cast<std::streamsize>(it->second->size()));
  }

  return {};
}

// ══════════════════════════════════════════════════════════════════════════
// Default icon
// ══════════════════════════════════════════════════════════════════════════

std::string writeDefaultIcon(const std::string& dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  std::string path = dir + "/coconut-icon.png";
  std::ofstream f(path, std::ios::binary);
  if (f.is_open()) {
    f.write(reinterpret_cast<const char*>(DEFAULT_ICON_PNG), DEFAULT_ICON_PNG_SIZE);
  }
  return path;
}

// ══════════════════════════════════════════════════════════════════════════
// Main entry point
// ══════════════════════════════════════════════════════════════════════════

std::expected<GeneratedIcons, Error>
generateIcons(const std::string& source_path,
              const std::string& out_dir,
              const std::string& app_id) {

  // Resolve source: empty → use embedded default
  std::string resolved = source_path;
  if (resolved.empty()) {
    resolved = writeDefaultIcon(out_dir);
  }

  // 1) Detect format
  SourceFormat fmt = detectFormat(resolved);
  if (fmt == SourceFormat::Unknown) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("unknown icon format for '{}'", source_path),
    });
  }

  // 2) Load source → RGBA at native resolution
  //    For SVG we defer rendering per-size; for raster sources we load once
  //    at the largest available size and scale down.

  std::optional<RenderedImage> loaded;  // set for raster sources
  std::string svgPath;                  // set for SVG sources
  int nativeSize = 0;                   // largest dimension of the loaded/source image

  switch (fmt) {
    case SourceFormat::Svg: {
      svgPath = resolved;
      // SVG is vector — can render at any size, no "native" limit.
      // Set nativeSize high so the raster loop doesn't skip any size.
      nativeSize = 4096;
      break;
    }
    case SourceFormat::Png:
    case SourceFormat::Jpeg: {
      auto result = loadRaster(resolved);
      if (!result) return std::unexpected(result.error());
      loaded = std::move(*result);
      nativeSize = std::max(loaded->width, loaded->height);
      break;
    }
    case SourceFormat::Icns: {
      auto result = loadIcns(resolved);
      if (!result) return std::unexpected(result.error());
      loaded = std::move(*result);
      nativeSize = std::max(loaded->width, loaded->height);
      break;
    }
    case SourceFormat::Ico: {
      auto result = loadIco(resolved);
      if (!result) return std::unexpected(result.error());
      loaded = std::move(*result);
      nativeSize = std::max(loaded->width, loaded->height);
      break;
    }
    default:
      return std::unexpected(Error{
        .code = ErrorCode::InvalidConfig,
        .message = "unreachable: unknown format after detection",
      });
  }

  if (nativeSize <= 0) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = std::format("empty or zero-size icon: {}", source_path),
    });
  }

  // 3) Collect all unique sizes needed across all platforms
  std::vector<int> allSizes;
  for (const auto& s : {1024, 512, 256, 128, 64, 48, 32, 24, 16}) {
    allSizes.push_back(s);
  }

  // 4) Generate PNG at each size (render SVG directly, scale raster)
  std::vector<PngIcon> icons;
  for (int size : allSizes) {
    if (fmt != SourceFormat::Svg && size > nativeSize) continue;
    // For SVG we can render at any size; for rasters we don't upscale.

    RenderedImage rendered;
    if (fmt == SourceFormat::Svg) {
      // Render SVG at exactly this size
      auto result = loadSvg(resolved, size);
      if (!result) continue;  // skip if render fails at this size
      rendered = std::move(*result);
    } else {
      // Scale from loaded raster
      if (size == nativeSize) {
        rendered = *loaded;  // exact size, no scaling needed
      } else {
        rendered = scaleImage(*loaded, size, size);
      }
    }

    auto png = encodePng(rendered);
    if (!png) continue;

    icons.push_back(PngIcon{size, std::move(*png)});
  }

  if (icons.empty()) {
    return std::unexpected(Error{
      .code = ErrorCode::InvalidConfig,
      .message = "failed to generate any PNG icons from source",
    });
  }

  // 5) Pack and write outputs
  GeneratedIcons result;

  // macOS .icns
  auto icnsData = packIcns(icons);
  if (icnsData) {
    std::string icnsPath = out_dir + "/icon.icns";
    std::ofstream f(icnsPath, std::ios::binary);
    if (f.is_open()) {
      f.write(reinterpret_cast<const char*>(icnsData->data()),
              static_cast<std::streamsize>(icnsData->size()));
      result.icns = icnsPath;
    }
  }

  // Windows .ico
  auto icoData = packIco(icons);
  if (icoData) {
    std::string icoPath = out_dir + "/icon.ico";
    std::ofstream f(icoPath, std::ios::binary);
    if (f.is_open()) {
      f.write(reinterpret_cast<const char*>(icoData->data()),
              static_cast<std::streamsize>(icoData->size()));
      result.ico = icoPath;
    }
  }

  // Linux: largest PNG + freedesktop dir
  {
    auto it = std::find_if(icons.begin(), icons.end(),
        [](const PngIcon& i) { return i.size == 512; });
    if (it == icons.end() && !icons.empty()) it = icons.begin();
    if (it != icons.end()) {
      std::string pngPath = out_dir + "/icon_512.png";
      std::ofstream f(pngPath, std::ios::binary);
      if (f.is_open()) {
        f.write(reinterpret_cast<const char*>(it->data.data()),
                static_cast<std::streamsize>(it->data.size()));
        result.png = pngPath;
      }
    }

    // Linux directory structure
    writeLinuxIcons(icons, out_dir, app_id);

    // If source was SVG, also copy it as scalable icon
    if (fmt == SourceFormat::Svg) {
      std::string scalableDir = out_dir + "/share/icons/hicolor/scalable/apps";
      std::error_code ec;
      fs::create_directories(scalableDir, ec);
      if (!ec) {
        std::string svgDest = scalableDir + "/" + app_id + ".svg";
        fs::copy_file(resolved, svgDest,
                      fs::copy_options::overwrite_existing, ec);
        if (!ec) result.svg = svgDest;
      }
    }
  }

  return result;
}

} // namespace coconut::icon_gen
