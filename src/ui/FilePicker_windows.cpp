#ifdef _WIN32
#include "FilePicker.h"
#include <windows.h>
#include <shobjidl.h>
#include <string>

static std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == 0) w.pop_back();
    return w;
}

static std::string fromWide(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    if (!s.empty() && s.back() == 0) s.pop_back();
    return s;
}

std::string pickVideoFile() {
    std::string result;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg)))) {
        CoUninitialize();
        return {};
    }

    COMDLG_FILTERSPEC filter[] = {
        { L"Video files", L"*.mp4;*.mkv;*.mov;*.avi;*.flv;*.webm" },
        { L"All files",   L"*.*" }
    };
    dlg->SetFileTypes(2, filter);
    dlg->SetTitle(L"Choose a video file");

    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = fromWide(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
    CoUninitialize();
    return result;
}

std::string pickSaveFile(const std::string& suggestedName) {
    std::string result;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IFileSaveDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg)))) {
        CoUninitialize();
        return {};
    }

    COMDLG_FILTERSPEC filter[] = {
        { L"MP4 video", L"*.mp4" },
        { L"All files", L"*.*" }
    };
    dlg->SetFileTypes(1, filter);
    dlg->SetDefaultExtension(L"mp4");
    dlg->SetTitle(L"Save recording as");
    if (!suggestedName.empty())
        dlg->SetFileName(toWide(suggestedName).c_str());

    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = fromWide(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dlg->Release();
    CoUninitialize();
    return result;
}
#endif
