#include "debug.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>

namespace coconut::debug {

// ── Log level (thread-safe enough for a single-threaded UI runtime) ────

static Level g_level = Level::Info;

void setLevel(Level lvl) { g_level = lvl; }
Level getLevel() { return g_level; }

Level levelFromString(const std::string& name) {
  if (name == "debug") return Level::Debug;
  if (name == "info")  return Level::Info;
  if (name == "warn")  return Level::Warn;
  if (name == "error") return Level::Error;
  return Level::Info; // safe default
}

// ── ANSI colour helpers ───────────────────────────────────────────────

namespace {
  constexpr auto RESET   = "\033[0m";
  constexpr auto GREY    = "\033[90m";
  constexpr auto CYAN    = "\033[96m";
  constexpr auto YELLOW  = "\033[93m";
  constexpr auto RED     = "\033[91m";
  constexpr auto BOLD    = "\033[1m";

  // stderr is a TTY?  We check once and cache.
  bool stderrIsTty() {
    static const bool is_tty = [] {
      return isatty(STDERR_FILENO) != 0;
    }();
    return is_tty;
  }

  std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) % 1000;
    std::tm tm;
    localtime_r(&tt, &tm);
    std::ostringstream oss;
    oss << "[" << std::put_time(&tm, "%H:%M:%S") << "."
        << std::setfill('0') << std::setw(3) << ms.count() << "]";
    return oss.str();
  }
}

// ── Level helpers ─────────────────────────────────────────────────────

static void emit(Level lvl, const char* colour, const char* label,
                 const std::string& msg) {
  if (lvl < g_level) return; // filtered out

  auto ts = timestamp();
  if (stderrIsTty()) {
    std::cerr << colour << ts << ' ' << BOLD << label << RESET << ' '
              << msg << '\n';
  } else {
    std::cerr << ts << ' ' << label << ' ' << msg << '\n';
  }
}

// ── Public API ────────────────────────────────────────────────────────

void log(const std::string& msg) {
  emit(Level::Debug, GREY, "[DEBUG]", msg);
}

void info(const std::string& msg) {
  emit(Level::Info, CYAN, "[INFO]", msg);
}

void warn(const std::string& msg) {
  emit(Level::Warn, YELLOW, "[WARN]", msg);
}

void error(const std::string& msg) {
  emit(Level::Error, RED, "[ERROR]", msg);
}

} // namespace coconut::debug
