#pragma once

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

const std::string WHITESPACE = " \n\r\t\f\v";

bool isWhitespace(std::string c){
  return WHITESPACE.contains(c);
}
bool isWhitespace(char c){
  return WHITESPACE.contains(c);
}
// Trim from the start (left)
std::string ltrim(std::string s) {
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

// Trim from the end (right)
std::string rtrim(std::string s) {
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

// Trim from both ends
std::string trim(std::string s) { return rtrim(ltrim(s)); }

