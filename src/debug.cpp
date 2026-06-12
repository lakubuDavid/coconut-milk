#include "debug.h"

#include <iostream>
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
}

// ── Level helpers ─────────────────────────────────────────────────────

static void emit(Level lvl, const char* colour, const char* label,
                 const std::string& msg) {
  if (lvl < g_level) return; // filtered out

  if (stderrIsTty()) {
    std::cerr << colour << BOLD << label << RESET << ' '
              << msg << '\n';
  } else {
    std::cerr << label << ' ' << msg << '\n';
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
