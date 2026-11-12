#include "RegistryScanner.h"
#include "Utils.h"
#include <cwchar>

std::vector<UninstallEntry> RegistryScanner::Scan() {
    found_.clear();
    total_ = 0;
    ScanHive(HKEY_LOCAL_MACHINE, RegHive::LocalMachine64, KEY_WOW64_64KEY);
    ScanHive(HKEY_LOCAL_MACHINE, RegHive::LocalMachine32, KEY_WOW64_32KEY);
    ScanHive(HKEY_CURRENT_USER,  RegHive::CurrentUser,    KEY_WOW64_64KEY);
    return found_;
}

bool RegistryScanner::ReadString(HKEY key, const wchar_t* value, std::wstring& out) {
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(key, value, nullptr, &type, nullptr, &size) != ERROR_SUCCESS)
        return false;
    if (type != REG_SZ && type != REG_EXPAND_SZ)
        return false;

    std::wstring buf(size / sizeof(wchar_t) + 1, L'\0');
    DWORD sz = size;
    if (RegQueryValueExW(key, value, nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&buf[0]), &sz) != ERROR_SUCCESS)
        return false;
    buf.resize(wcslen(buf.c_str()));

    if (type == REG_EXPAND_SZ) {
        DWORD n = ExpandEnvironmentStringsW(buf.c_str(), nullptr, 0);
        if (n > 0) {
            std::wstring e(n, L'\0');
            ExpandEnvironmentStringsW(buf.c_str(), &e[0], n);
            e.resize(n - 1);
            buf = e;
        }
    }
    out = buf;
    return true;
}

bool RegistryScanner::ReadDword(HKEY key, const wchar_t* value, DWORD& out) {
    DWORD type = 0, size = sizeof(DWORD);
    return RegQueryValueExW(key, value, nullptr, &type,
                            reinterpret_cast<LPBYTE>(&out), &size) == ERROR_SUCCESS
           && type == REG_DWORD;
}

void RegistryScanner::ScanHive(HKEY root, RegHive hive, REGSAM view) {
    const wchar_t* path = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    HKEY uninstall;
    if (RegOpenKeyExW(root, path, 0, KEY_READ | view, &uninstall) != ERROR_SUCCESS)
        return;

    wchar_t subName[256];
    for (DWORD i = 0;; ++i) {
        DWORD len = 256;
        LONG r = RegEnumKeyExW(uninstall, i, subName, &len, nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;

        HKEY sub;
        if (RegOpenKeyExW(uninstall, subName, 0, KEY_READ | view, &sub) != ERROR_SUCCESS)
            continue;

        std::wstring displayName, installLoc, uninstStr, iconPath;
        DWORD sysComp = 0;
        ReadString(sub, L"DisplayName", displayName);
        ReadString(sub, L"InstallLocation", installLoc);
        ReadString(sub, L"UninstallString", uninstStr);
        ReadString(sub, L"DisplayIcon", iconPath);
        ReadDword(sub, L"SystemComponent", sysComp);
        RegCloseKey(sub);

        if (sysComp == 1 || displayName.empty()) continue;
        ++total_;

        installLoc = Utils::Trim(installLoc);
        std::wstring reason;
        std::wstring checked;       // какой путь проверяли
        bool partial = false;       // файла нет, а папка программы есть

        if (!installLoc.empty() && Utils::PathMissing(installLoc)) {
            reason = L"папка установки не существует: " + installLoc;
            checked = installLoc;
        } else if (!uninstStr.empty() && !Utils::IsMsiCommand(uninstStr)) {
            std::wstring exe = Utils::ExtractExePath(uninstStr);
            if (!exe.empty() && Utils::PathMissing(exe)) {
                reason = L"деинсталлятор не найден: " + exe;
                checked = exe;
                size_t p = exe.find_last_of(L"\\/");
                if (p != std::wstring::npos && !Utils::PathMissing(exe.substr(0, p)))
                    partial = true;
            }
        }

        // если файл из DisplayIcon существует, программа жива
        bool aliveByIcon = false;
        if (!iconPath.empty()) {
            std::wstring ip = Utils::ExtractExePath(iconPath);
            if (!ip.empty() && !Utils::MissingExact(ip)) aliveByIcon = true;
        }

        if (!reason.empty() && !aliveByIcon) {
            std::wstring low = Utils::ToLower(reason);
            bool risky = partial
                      || Utils::DriveMissing(checked)
                      || low.find(L"package cache") != std::wstring::npos
                      || low.find(L"\\windows\\") != std::wstring::npos;
            found_.emplace_back(hive, subName, displayName, reason, !risky);
        }
    }
    RegCloseKey(uninstall);
}