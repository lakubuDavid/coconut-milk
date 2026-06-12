#ifndef COCONUT_DEBUG_H
#define COCONUT_DEBUG_H

#include <string>

namespace coconut {
namespace debug {

/// Log levels, ordered from most verbose to most severe.
/// Messages below the current level are filtered out.
enum class Level { Debug, Info, Warn, Error };

/// Set the minimum log level.  Anything below this threshold is silenced.
/// Default: Level::Info (so Debug messages are off by default).
void setLevel(Level lvl);
Level getLevel();

/// Parse a log level from a string ("debug", "info", "warn", "error").
/// Returns Level::Info for unknown strings.
Level levelFromString(const std::string& name);

/// Structured logging with ANSI colour and level-prefixed output to stderr.
///   log()   → grey  [DEBUG]
///   info()  → cyan  [INFO]
///   warn()  → yellow[WARN]
///   error() → red   [ERROR]
void log(const std::string& msg);
void info(const std::string& msg);
void warn(const std::string& msg);
void error(const std::string& msg);

} // namespace debug
} // namespace coconut

#endif // COCONUT_DEBUG_H
