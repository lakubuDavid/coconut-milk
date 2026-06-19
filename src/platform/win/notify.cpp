#include "notify.h"
#include <windows.h>
#include <string>

namespace coconut::notify {

// WM_APP message for notification callback
#define WM_COCONUT_NOTIFY (WM_APP + 100)

bool platformNotify(const std::string& title, const std::string& body) {
  // Convert UTF-8 to UTF-16
  auto to_utf16 = [](const std::string& s) -> std::wstring {
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                  nullptr, 0);
    std::wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(),
                        &ws[0], len);
    return ws;
  };

  std::wstring wtitle = to_utf16(title);
  std::wstring wbody = to_utf16(body);

  // Use Shell_NotifyIcon with NIIF_NONE for a balloon notification.
  // For Windows 10+ toast notifications, use the COM toast API.
  // This implementation uses the balloon fallback which works on all
  // Windows versions and under Wine.

  // Find a suitable window handle for the notification
  HWND hwnd = GetForegroundWindow();
  if (!hwnd) hwnd = GetDesktopWindow();

  NOTIFYICONDATAW nid = {};
  nid.cbSize = sizeof(NOTIFYICONDATAW);
  nid.hWnd = hwnd;
  nid.uID = 1;
  nid.uFlags = NIF_INFO | NIF_SHOWTIP;
  nid.dwInfoFlags = NIIF_INFO;
  nid.uTimeout = 5000; // 5 seconds

  // Copy title and body (limited to 64/256 chars by Win32 API)
  wcsncpy_s(nid.szInfoTitle, wtitle.c_str(), _TRUNCATE);
  wcsncpy_s(nid.szInfo, wbody.c_str(), _TRUNCATE);

  // Set a unique callback message
  nid.uCallbackMessage = WM_COCONUT_NOTIFY;

  return Shell_NotifyIconW(NIM_ADD, &nid) ||
         Shell_NotifyIconW(NIM_MODIFY, &nid);
}

} // namespace coconut::notify
