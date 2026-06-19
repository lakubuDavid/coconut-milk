#include "dialog.h"
#include "../../debug.h"
#include <windows.h>
#include <shobjidl.h>
#include <string>
#include <vector>

namespace coconut::dialog {

Result platformMessageBox(const std::string& title,
                          const std::string& message,
                          const std::string& kind) {
  UINT type = MB_OK;
  if (kind == "question") type |= MB_YESNO;
  else if (kind == "error") type |= MB_ICONERROR;
  else if (kind == "warning") type |= MB_ICONWARNING;
  else type |= MB_ICONINFORMATION;

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
  std::wstring wmsg = to_utf16(message);

  int result = MessageBoxW(nullptr, wmsg.c_str(), wtitle.c_str(), type);

  Result r;
  r.confirmed = (result == IDOK || result == IDYES);
  return r;
}

Result platformOpenFile(const std::string& title,
                        const std::vector<Filter>& filters,
                        bool multi,
                        bool chooseDir) {
  Result r;
  r.confirmed = false;

  // Initialize COM
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    debug::warn("Win32 dialog::openFile: CoInitializeEx failed");
    return r;
  }

  if (chooseDir) {
    // Use IFileOpenDialog with folder picker
    IFileOpenDialog* pfd = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr)) {
      DWORD flags;
      pfd->GetOptions(&flags);
      pfd->SetOptions(flags | FOS_PICKFOLDERS);

      // Set title
      std::wstring wtitle(title.begin(), title.end());
      pfd->SetTitle(wtitle.c_str());

      hr = pfd->Show(nullptr);
      if (SUCCEEDED(hr)) {
        IShellItem* psi = nullptr;
        hr = pfd->GetResult(&psi);
        if (SUCCEEDED(hr)) {
          wchar_t* path = nullptr;
          psi->GetDisplayName(SIGDN_FILESYSPATH, &path);
          if (path) {
            r.confirmed = true;
            r.is_dir = true;
            int len = WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                          nullptr, 0, nullptr, nullptr);
            r.path.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                &r.path[0], len, nullptr, nullptr);
            r.paths.push_back(r.path);
            CoTaskMemFree(path);
          }
          psi->Release();
        }
      }
      pfd->Release();
    }
  } else {
    // Use IFileOpenDialog for files
    IFileOpenDialog* pfd = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr)) {
      DWORD flags = FOS_FORCEFILESYSTEM;
      if (multi) flags |= FOS_ALLOWMULTISELECT;
      pfd->SetOptions(flags);

      std::wstring wtitle(title.begin(), title.end());
      pfd->SetTitle(wtitle.c_str());

      // Add file filters
      if (!filters.empty()) {
        std::vector<COMDLG_FILTERSPEC> specs;
        for (const auto& f : filters) {
          COMDLG_FILTERSPEC spec;
          std::wstring wname(f.name.begin(), f.name.end());
          spec.pszName = wname.c_str();

          // Combine patterns into semicolon-separated string
          std::wstring wpattern;
          for (size_t i = 0; i < f.patterns.size(); i++) {
            if (i > 0) wpattern += L";";
            wpattern += std::wstring(f.patterns[i].begin(), f.patterns[i].end());
          }
          spec.pszSpec = wpattern.c_str();
          specs.push_back(spec);
        }
        pfd->SetFileTypes((UINT)specs.size(), specs.data());
      }

      hr = pfd->Show(nullptr);
      if (SUCCEEDED(hr)) {
        if (multi) {
          IShellItemArray* items = nullptr;
          hr = pfd->GetResults(&items);
          if (SUCCEEDED(hr)) {
            DWORD count = 0;
            items->GetCount(&count);
            for (DWORD i = 0; i < count; i++) {
              IShellItem* psi = nullptr;
              items->GetItemAt(i, &psi);
              if (psi) {
                wchar_t* path = nullptr;
                psi->GetDisplayName(SIGDN_FILESYSPATH, &path);
                if (path) {
                  r.confirmed = true;
                  if (i == 0) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                                  nullptr, 0, nullptr, nullptr);
                    r.path.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                        &r.path[0], len, nullptr, nullptr);
                  }
                  int len = WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                                nullptr, 0, nullptr, nullptr);
                  std::string spath(len - 1, 0);
                  WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                      &spath[0], len, nullptr, nullptr);
                  r.paths.push_back(spath);
                  CoTaskMemFree(path);
                }
                psi->Release();
              }
            }
            items->Release();
          }
        } else {
          IShellItem* psi = nullptr;
          hr = pfd->GetResult(&psi);
          if (SUCCEEDED(hr)) {
            wchar_t* path = nullptr;
            psi->GetDisplayName(SIGDN_FILESYSPATH, &path);
            if (path) {
              r.confirmed = true;
              int len = WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                            nullptr, 0, nullptr, nullptr);
              r.path.resize(len - 1);
              WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                  &r.path[0], len, nullptr, nullptr);
              r.paths.push_back(r.path);
              CoTaskMemFree(path);
            }
            psi->Release();
          }
        }
      }
      pfd->Release();
    }
  }

  CoUninitialize();
  return r;
}

Result platformSaveFile(const std::string& title,
                        const std::string& defaultName,
                        const std::vector<Filter>& filters) {
  Result r;
  r.confirmed = false;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    debug::warn("Win32 dialog::saveFile: CoInitializeEx failed");
    return r;
  }

  IFileSaveDialog* pfd = nullptr;
  hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&pfd));
  if (SUCCEEDED(hr)) {
    std::wstring wtitle(title.begin(), title.end());
    pfd->SetTitle(wtitle.c_str());

    if (!defaultName.empty()) {
      std::wstring wdefault(defaultName.begin(), defaultName.end());
      pfd->SetFileName(wdefault.c_str());
    }

    // Add file filters
    if (!filters.empty()) {
      std::vector<COMDLG_FILTERSPEC> specs;
      for (const auto& f : filters) {
        COMDLG_FILTERSPEC spec;
        std::wstring wname(f.name.begin(), f.name.end());
        spec.pszName = wname.c_str();

        std::wstring wpattern;
        for (size_t i = 0; i < f.patterns.size(); i++) {
          if (i > 0) wpattern += L";";
          wpattern += std::wstring(f.patterns[i].begin(), f.patterns[i].end());
        }
        spec.pszSpec = wpattern.c_str();
        specs.push_back(spec);
      }
      pfd->SetFileTypes((UINT)specs.size(), specs.data());
    }

    hr = pfd->Show(nullptr);
    if (SUCCEEDED(hr)) {
      IShellItem* psi = nullptr;
      hr = pfd->GetResult(&psi);
      if (SUCCEEDED(hr)) {
        wchar_t* path = nullptr;
        psi->GetDisplayName(SIGDN_FILESYSPATH, &path);
        if (path) {
          r.confirmed = true;
          int len = WideCharToMultiByte(CP_UTF8, 0, path, -1,
                                        nullptr, 0, nullptr, nullptr);
          r.path.resize(len - 1);
          WideCharToMultiByte(CP_UTF8, 0, path, -1,
                              &r.path[0], len, nullptr, nullptr);
          r.paths.push_back(r.path);
          CoTaskMemFree(path);
        }
        psi->Release();
      }
    }
    pfd->Release();
  }

  CoUninitialize();
  return r;
}

} // namespace coconut::dialog
