#ifndef COCONUT_DEBUG_H
#define COCONUT_DEBUG_H

#include <string>

namespace coconut {
namespace debug {

/// Structured logging with level-prefixed output to stderr.
void log(const std::string& msg);
void info(const std::string& msg);
void warn(const std::string& msg);
void error(const std::string& msg);

} // namespace debug
} // namespace coconut

#endif // COCONUT_DEBUG_H
