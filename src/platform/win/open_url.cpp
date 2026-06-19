#include "open_url.h"
#include <windows.h>
#include <shellapi.h>
#include <string>

namespace coconut::open_url {

bool platformOpenUrl(const std::string& url) {
  // Convert UTF-8 to UTF-16 for ShellExecuteW
  int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), (int)url.size(),
                                 nullptr, 0);
  std::wstring wurl(wlen, 0);
  MultiByteToWideChar(CP_UTF8, 0, url.c_str(), (int)url.size(),
                      &wurl[0], wlen);

  HINSTANCE result = ShellExecuteW(nullptr, L"open", wurl.c_str(),
                                   nullptr, nullptr, SW_SHOWNORMAL);
  // ShellExecute returns a value > 32 on success
  return reinterpret_cast<INT_PTR>(result) > 32;
}

} // namespace coconut::open_url
