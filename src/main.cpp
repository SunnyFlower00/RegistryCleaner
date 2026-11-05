// main.cpp - Шаг 1: поиск устаревших записей в разделе Uninstall (только чтение)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#ifndef KEY_WOW64_64KEY
#define KEY_WOW64_64KEY (0x0100)
#endif
#ifndef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY (0x0200)
#endif
#include <stdio.h>
#include <string>
#include <vector>

struct Entry {
    std::wstring hive;       // где найдено
    std::wstring keyName;    // имя подраздела
    std::wstring name;       // DisplayName
    std::wstring reason;     // почему считаем устаревшей
    bool confident;          // true = высокая уверенность
};

// ---------- вспомогательные функции ----------

static bool ReadString(HKEY key, const wchar_t* value, std::wstring& out) {
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

static bool ReadDword(HKEY key, const wchar_t* value, DWORD& out) {
    DWORD type = 0, size = sizeof(DWORD);
    return RegQueryValueExW(key, value, nullptr, &type,
                            reinterpret_cast<LPBYTE>(&out), &size) == ERROR_SUCCESS
           && type == REG_DWORD;
}

static bool MissingExact(const std::wstring& p) {
    if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) return false;
    DWORD e = GetLastError();
    // "нет файла" считаем только при явной ошибке "не найден";
    // "доступ запрещён" и прочее не считаем доказательством
    return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND;
}

static bool PathMissing(const std::wstring& p) {
    if (!MissingExact(p)) return false;
    // перенаправление System32 -> SysWOW64 для 32-битных программ
    std::wstring low = p;
    for (auto& c : low) c = towlower(c);
    size_t pos = low.find(L"\\system32\\");
    if (pos != std::wstring::npos) {
        std::wstring alt = p;
        alt.replace(pos, 10, L"\\SysWOW64\\");
        if (!MissingExact(alt)) return false;
    }
    return true;
}

static bool DriveMissing(const std::wstring& p) {
    if (p.size() < 3 || p[1] != L':') return false;
    return MissingExact(p.substr(0, 3));   // например "D:\"
}

static std::wstring Trim(std::wstring s) {
    const wchar_t* ws = L" \t\r\n\"";
    size_t a = s.find_first_not_of(ws);
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

// Достаёт путь к exe из командной строки вида:
//   "C:\Program Files\App\unins000.exe" /silent
//   C:\App\uninstall.exe -x
static std::wstring ExtractExePath(const std::wstring& cmd) {
    std::wstring s = cmd;
    size_t start = s.find_first_not_of(L" \t");
    if (start == std::wstring::npos) return L"";
    s = s.substr(start);

    if (s[0] == L'"') {
        size_t end = s.find(L'"', 1);
        if (end == std::wstring::npos) return s.substr(1);
        return s.substr(1, end - 1);
    }
    // без кавычек: ищем ".exe" (без учёта регистра)
    std::wstring low = s;
    for (auto& c : low) c = towlower(c);
    size_t pos = low.find(L".exe");
    if (pos != std::wstring::npos) return s.substr(0, pos + 4);
    return s;
}

static bool IsMsiCommand(const std::wstring& cmd) {
    std::wstring low = cmd;
    for (auto& c : low) c = towlower(c);
    return low.find(L"msiexec") != std::wstring::npos
        || low.find(L"rundll32") != std::wstring::npos;
}

// ---------- сканирование ----------

static void ScanKey(HKEY root, const wchar_t* hiveName, REGSAM view,
                    std::vector<Entry>& result, int& total) {
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

        std::wstring displayName, installLoc, uninstStr;
        DWORD sysComp = 0;
        ReadString(sub, L"DisplayName", displayName);
        ReadString(sub, L"InstallLocation", installLoc);
        ReadString(sub, L"UninstallString", uninstStr);
        std::wstring iconPath;
        ReadString(sub, L"DisplayIcon", iconPath);
        ReadDword(sub, L"SystemComponent", sysComp);
        RegCloseKey(sub);

        // пропускаем системные компоненты и записи без имени
        if (sysComp == 1 || displayName.empty()) continue;
        ++total;

        installLoc = Trim(installLoc);
        std::wstring reason;
        std::wstring checked;       // какой путь проверяли
        bool partial = false;       // файла нет, а папка программы есть

        if (!installLoc.empty() && PathMissing(installLoc)) {
            reason = L"папка установки не существует: " + installLoc;
            checked = installLoc;
        } else if (!uninstStr.empty() && !IsMsiCommand(uninstStr)) {
            std::wstring exe = ExtractExePath(uninstStr);
            if (!exe.empty() && PathMissing(exe)) {
                reason = L"деинсталлятор не найден: " + exe;
                checked = exe;
                size_t p = exe.find_last_of(L"\\/");
                if (p != std::wstring::npos && !PathMissing(exe.substr(0, p)))
                    partial = true;
            }
        }
        
        bool aliveByIcon = false;
        if (!iconPath.empty()) {
            std::wstring ip = ExtractExePath(iconPath);
            if (!ip.empty() && !MissingExact(ip)) aliveByIcon = true;
        }

        if (!reason.empty() && !aliveByIcon) {
            std::wstring low = reason;
            for (auto& c : low) c = towlower(c);
            bool risky = partial
                      || DriveMissing(checked)
                      || low.find(L"package cache") != std::wstring::npos
                      || low.find(L"\\windows\\") != std::wstring::npos;
            result.push_back({ hiveName, subName, displayName, reason, !risky });
        }
    }
    RegCloseKey(uninstall);
}

static void Print(const std::wstring& s) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    if (GetConsoleMode(h, &mode)) {
        DWORD written;
        WriteConsoleW(h, s.c_str(), (DWORD)s.size(), &written, nullptr);
    } else {
        // вывод перенаправлен в файл или канал: пишем в UTF-8
        int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string out(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
        DWORD written;
        WriteFile(h, out.data(), (DWORD)out.size(), &written, nullptr);
    }
}

static std::wstring Num(size_t n) {
    if (n == 0) return L"0";
    std::wstring s;
    while (n > 0) {
        s.insert(s.begin(), static_cast<wchar_t>(L'0' + (n % 10)));
        n /= 10;
    }
    return s;
}

int main() {
    std::vector<Entry> found;
    int total = 0;

    ScanKey(HKEY_LOCAL_MACHINE, L"HKLM (64-bit)", KEY_WOW64_64KEY, found, total);
    ScanKey(HKEY_LOCAL_MACHINE, L"HKLM (32-bit)", KEY_WOW64_32KEY, found, total);
    ScanKey(HKEY_CURRENT_USER,  L"HKCU",          KEY_WOW64_64KEY, found, total);

    Print(L"Проверено записей: " + Num(total) + L"\n\n");

    for (int pass = 0; pass < 2; ++pass) {
        bool wantConfident = (pass == 0);
        size_t count = 0;
        for (const Entry& e : found) if (e.confident == wantConfident) ++count;

        Print(wantConfident
            ? L"=== Высокая уверенность: " + Num(count) + L" ===\n\n"
            : L"=== Нужна проверка (системные установщики): " + Num(count) + L" ===\n\n");

        size_t n = 0;
        for (const Entry& e : found) {
            if (e.confident != wantConfident) continue;
            Print(L"[" + Num(++n) + L"] " + e.name + L"\n");
            Print(L"    Раздел: " + e.hive + L"\\...\\Uninstall\\" + e.keyName + L"\n");
            Print(L"    Причина: " + e.reason + L"\n\n");
        }
    }
    return 0;
}