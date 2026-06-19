#include "clipboard.h"
#include <windows.h>
#include <string>

namespace coconut::clipboard {

std::string platformReadText() {
  if (!OpenClipboard(nullptr)) return {};

  HANDLE h = GetClipboardData(CF_UNICODETEXT);
  if (!h) {
    CloseClipboard();
    return {};
  }

  wchar_t* p = static_cast<wchar_t*>(GlobalLock(h));
  if (!p) {
    CloseClipboard();
    return {};
  }

  std::wstring ws(p);
  GlobalUnlock(h);
  CloseClipboard();

  // Convert UTF-16 to UTF-8
  int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
                                nullptr, 0, nullptr, nullptr);
  std::string result(len, 0);
  WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
                      &result[0], len, nullptr, nullptr);
  return result;
}

bool platformWriteText(const std::string& text) {
  // Convert UTF-8 to UTF-16
  int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(),
                                 nullptr, 0);
  std::wstring wtext(wlen, 0);
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(),
                      &wtext[0], wlen);

  if (!OpenClipboard(nullptr)) return false;
  if (!EmptyClipboard()) {
    CloseClipboard();
    return false;
  }

  // Allocate global memory for the clipboard data
  HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (wtext.size() + 1) * sizeof(wchar_t));
  if (!h) {
    CloseClipboard();
    return false;
  }

  wchar_t* p = static_cast<wchar_t*>(GlobalLock(h));
  if (!p) {
    GlobalFree(h);
    CloseClipboard();
    return false;
  }

  wcscpy_s(p, wtext.size() + 1, wtext.c_str());
  GlobalUnlock(h);

  if (!SetClipboardData(CF_UNICODETEXT, h)) {
    GlobalFree(h);
    CloseClipboard();
    return false;
  }

  CloseClipboard();
  return true;
}

} // namespace coconut::clipboard
