#include "./command_definition.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

/// Simple field extraction from a string key = "value" pattern.
/// Works for both JSON ("key": "value") and Lua (key = "value") formats.
static std::string configStringField(const std::string& text,
                                     const std::string& key) {
  // Try Lua-style: key = "value"
  {
    std::string search = key + " = \"";
    size_t pos = text.find(search);
    if (pos != std::string::npos) {
      size_t start = pos + search.size();
      size_t end = text.find('"', start);
      if (end != std::string::npos)
        return text.substr(start, end - start);
    }
  }
  // Try JSON-style: "key": "value"
  {
    std::string search = "\"" + key + "\"";
    size_t pos = text.find(search);
    if (pos == std::string::npos)
      return "";
    pos = text.find(':', pos + search.size());
    if (pos == std::string::npos)
      return "";
    pos = text.find_first_of('"', pos);
    if (pos == std::string::npos)
      return "";
    size_t end = text.find_first_of('"', pos + 1);
    if (end == std::string::npos)
      return "";
    return text.substr(pos + 1, end - pos - 1);
  }
}

static std::string deriveModulePath(const std::string& filePath) {
  std::string stem = filePath;
  if (stem.size() >= 4 && stem.substr(stem.size() - 4) == ".lua")
    stem = stem.substr(0, stem.size() - 4);
  size_t slash = stem.find('/');
  if (slash != std::string::npos)
    stem = stem.substr(slash + 1);
  for (auto& c : stem)
    if (c == '/')
      c = '.';
  return stem;
}

static std::string outputStem(const std::string& filePath) {
  return fs::path(filePath).stem().string();
}

static void printUsage(const char* prog) {
  std::cerr << "Usage:\n";
  std::cerr << "  " << prog << " <input.lua> [--out-dir <dir>]\n";
  std::cerr << "  " << prog << " new <app-name> [--dir <dir>]\n";
  std::cerr << "\n";
  std::cerr << "Commands:\n";
  std::cerr
      << "  <input.lua>     Parse @command annotations and generate:\n";
  std::cerr
      << "                    <stem>.g.lua  — Lua command registration\n";
  std::cerr
      << "                    <stem>.d.ts   — TypeScript declarations\n";
  std::cerr
      << "                    <stem>.g.js   — JS wrappers with JSDoc\n";
  std::cerr << "  new <app-name>  Scaffold a new Coconut Milk app\n";
  std::cerr << "\n";
  std::cerr
      << "Options:\n";
  std::cerr
      << "  --out-dir <dir>  Output directory for generated files\n";
  std::cerr
      << "  --dir <dir>      Target directory for scaffold\n";
}

/// Scaffold a new Coconut Milk project.
static int scaffoldNew(const std::string& appName,
                       const std::string& targetDir) {
  fs::path root(targetDir);
  fs::create_directories(root / "views");
  fs::create_directories(root / "commands");
  fs::create_directories(root / "assets");

  // coconut.config.lua
  {
    std::ofstream f(root / "coconut.config.lua");
    f << R"(-- Coconut Milk configuration for )" << appName << R"(

browser = "auto"
window_width = 1280
window_height = 640
initial_view = "home"
view_root = "views"
asset_root = "assets"
command_root = "commands"
output_dir = "generated"
)";
    std::cout << "  wrote " << (root / "coconut.config.lua") << "\n";
  }

  // main.lua
  {
    std::ofstream f(root / "main.lua");
    f << R"(--- )" << appName
      << R"( — Coconut Milk entry point.

function coconut.views()
  return {
    home = View.load("views/home.html"),
  }
end

function coconut.config(ctx)
  ctx:setBrowser("auto")
     :setWindowSize({ w = 1280, h = 640 })
     :setInitialView("home")
  return ctx
end

function coconut.commands(ctx)
  -- Register commands here: ctx:bind("name", handler_fn)
end

function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  end
end
)";
    std::cout << "  wrote " << (root / "main.lua") << "\n";
  }

  // views/home.html
  {
    std::ofstream f(root / "views/home.html");
    f << R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>)" << appName
      << R"(</title>
  <style>
    * { margin:0;padding:0;box-sizing:border-box; }
    body { font-family:-apple-system,sans-serif; background:#0f0f13; color:#e4e4e7; padding:2rem; }
    h1 { color:#a78bfa; margin-bottom:1rem; }
    p { color:#a1a1aa; line-height:1.6; }
  </style>
</head>
<body>
  <h1>)" << appName
      << R"(</h1>
  <p>Your Coconut Milk app is running.</p>
  <script>
    (async () => {
      await coconut.ready();
      console.log('[coconut] bridge ready');
    })();
  </script>
</body>
</html>
)";
    std::cout << "  wrote " << (root / "views/home.html") << "\n";
  }

  // .gitkeep in empty dirs (commands/, assets/)
  {
    auto touch = [](const fs::path& p) {
      std::ofstream f(p);
      f << "";
    };
    touch(root / "commands/.gitkeep");
    touch(root / "assets/.gitkeep");
  }

  // README
  {
    std::ofstream f(root / "README.md");
    f << "# " << appName << "\n\n"
      << "A Coconut Milk desktop app.\n\n"
      << "## Getting Started\n\n"
      << "```bash\n"
      << "# Clone Coconut Milk and symlink or copy this app into its project root.\n"
      << "# Then from the Coconut Milk root:\n"
      << "just run\n"
      << "```\n";
    std::cout << "  wrote " << (root / "README.md") << "\n";
  }

  std::cout << "\nScaffolded '" << appName << "' in " << targetDir << "/\n";
  std::cout << "  views/   — HTML views\n";
  std::cout << "  commands/ — Lua command modules\n";
  std::cout << "  assets/   — Static assets\n";
  return 0;
}

// -----------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------

int main(int argc, char** argv) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  std::string first = argv[1];

  // ---- Scaffold a new app ----
  if (first == "new") {
    if (argc < 3) {
      std::cerr << "Error: missing app name. Usage: "
                << argv[0] << " new <app-name>\n";
      return 1;
    }
    std::string appName = argv[2];
    std::string targetDir = appName;
    for (int i = 3; i < argc; ++i) {
      if (std::string(argv[i]) == "--dir" && i + 1 < argc) {
        targetDir = argv[++i];
      }
    }
    return scaffoldNew(appName, targetDir);
  }

  // ---- Parse @command annotations and generate glue ----
  std::string inputPath = first;
  std::string outDir = "generated";

  // Load output_dir from config if present
  {
    std::ifstream luaCfg("coconut.config.lua");
    if (luaCfg.is_open()) {
      std::stringstream buf;
      buf << luaCfg.rdbuf();
      std::string d = configStringField(buf.str(), "output_dir");
      if (!d.empty())
        outDir = d;
    } else {
      std::ifstream jsonCfg("coconut.config.json");
      if (jsonCfg.is_open()) {
        std::stringstream buf;
        buf << jsonCfg.rdbuf();
        std::string d = configStringField(buf.str(), "output_dir");
        if (!d.empty())
          outDir = d;
      }
    }
  }

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--out-dir" && i + 1 < argc) {
      outDir = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      return 1;
    }
  }

  std::ifstream file(inputPath);
  if (!file.is_open()) {
    std::cerr << "Error: could not open " << inputPath << "\n";
    return 1;
  }

  std::stringstream buffer;
  std::string line;
  while (std::getline(file, line))
    buffer << line << "\n";
  file.close();

  auto commands = coconut::generator::commentsFsm(buffer.str());
  if (commands.empty()) {
    std::cerr << "No @command definitions found in " << inputPath << "\n";
    return 0;
  }

  std::string modulePath = deriveModulePath(inputPath);
  std::string stem = outputStem(inputPath);

  fs::create_directories(outDir);

  // .g.lua
  {
    auto luaWrap =
        coconut::generator::generateLuaWrapper(commands, modulePath);
    std::ofstream out(fs::path(outDir) / (stem + ".g.lua"));
    out << luaWrap;
    std::cout << "  wrote " << (fs::path(outDir) / (stem + ".g.lua"))
              << "\n";
  }

  // .d.ts
  {
    auto dts = coconut::generator::generateTSDefinition(commands);
    std::ofstream out(fs::path(outDir) / (stem + ".d.ts"));
    out << dts;
    std::cout << "  wrote " << (fs::path(outDir) / (stem + ".d.ts"))
              << "\n";
  }

  // .g.js
  {
    auto wrappers = coconut::generator::generateJSWrapper(commands);
    std::ofstream out(fs::path(outDir) / (stem + ".g.js"));
    out << wrappers;
    std::cout << "  wrote " << (fs::path(outDir) / (stem + ".g.js"))
              << "\n";
  }

  std::cout << "Generated " << commands.size()
            << " command(s) from " << inputPath << " → " << outDir
            << "/\n";
  return 0;
}
