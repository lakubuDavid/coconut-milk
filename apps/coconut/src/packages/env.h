#ifndef COCONUT_ENV_H
#define COCONUT_ENV_H

#include <string>

namespace coconut::env {

/// Read a single environment variable.
/// Returns the value, or empty string if the variable is not set.
std::string get(const std::string& name);

/// Get the current working directory.
std::string cwd();

/// Get the user's home directory.
std::string homedir();

/// Get the OS path separator (';' on Windows, ':' elsewhere).
char pathSeparator();

} // namespace coconut::env

#endif // COCONUT_ENV_H