#include "BackupManager.h"
#include "Platform.h"
#include "Utils.h"

bool BackupManager::Prepare() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::wstring root = Utils::ExeDir() + L"\\backup";
    CreateDirectoryW(root.c_str(), nullptr);
    std::wstring dir = root + L"\\" + Utils::Num(t.wYear) + L"-" + Utils::Pad2(t.wMonth)
                     + L"-" + Utils::Pad2(t.wDay) + L"_" + Utils::Pad2(t.wHour)
                     + L"-" + Utils::Pad2(t.wMinute) + L"-" + Utils::Pad2(t.wSecond);
    if (!CreateDirectoryW(dir.c_str(), nullptr)) return false;
    dir_ = dir;
    return true;
}

bool BackupManager::Backup(const UninstallEntry& e) {
    if (dir_.empty()) return false;
    std::wstring file = dir_ + L"\\" + e.HiveTag() + L"_" + SafeFileName(e.KeyName()) + L".reg";
    std::wstring args = L"export \"" + e.FullRegPath() + L"\" \"" + file + L"\" /y " + e.RegViewFlag();
    return RunReg(args);
}

std::wstring BackupManager::SafeFileName(const std::wstring& s) {
    std::wstring r;
    for (wchar_t c : s) {
        bool ok = (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') ||
                  (c >= L'A' && c <= L'Z') || c == L'_' || c == L'-' ||
                  c == L'{' || c == L'}' || c == L'.';
        r += ok ? c : L'_';
    }
    return r;
}

bool BackupManager::RunReg(const std::wstring& args) {
    std::wstring cmd = L"reg.exe " + args;
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}