#include "Utils.h"
#include "Platform.h"
#include <cwctype>

namespace Utils {

std::wstring Num(size_t n) {
    if (n == 0) return L"0";
    std::wstring s;
    while (n > 0) {
        s.insert(s.begin(), static_cast<wchar_t>(L'0' + (n % 10)));
        n /= 10;
    }
    return s;
}

std::wstring Pad2(size_t v) {
    std::wstring s = Num(v);
    return s.size() < 2 ? L"0" + s : s;
}

std::wstring ToLower(std::wstring s) {
    for (auto& c : s) c = towlower(c);
    return s;
}

std::wstring Trim(std::wstring s) {
    const wchar_t* ws = L" \t\r\n\"";
    size_t a = s.find_first_not_of(ws);
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : p.substr(0, pos);
}

bool MissingExact(const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return false;
    DWORD e = GetLastError();
    return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND;
}

bool PathMissing(const std::wstring& path) {
    if (!MissingExact(path)) return false;
    size_t pos = ToLower(path).find(L"\\system32\\");
    if (pos != std::wstring::npos) {
        std::wstring alt = path;
        alt.replace(pos, 10, L"\\SysWOW64\\");
        if (!MissingExact(alt)) return false;
    }
    return true;
}

bool DriveMissing(const std::wstring& path) {
    if (path.size() < 3 || path[1] != L':') return false;
    return MissingExact(path.substr(0, 3));
}

std::wstring ExtractExePath(const std::wstring& command) {
    std::wstring s = command;
    size_t start = s.find_first_not_of(L" \t");
    if (start == std::wstring::npos) return L"";
    s = s.substr(start);

    if (s[0] == L'"') {
        size_t end = s.find(L'"', 1);
        if (end == std::wstring::npos) return s.substr(1);
        return s.substr(1, end - 1);
    }
    size_t pos = ToLower(s).find(L".exe");
    if (pos != std::wstring::npos) return s.substr(0, pos + 4);

    // вид "C:\app\icon.ico,0": отрезаем индекс значка
    size_t comma = s.find_last_of(L',');
    size_t slash = s.find_last_of(L"\\/");
    if (comma != std::wstring::npos && (slash == std::wstring::npos || comma > slash))
        s = s.substr(0, comma);
    return s;
}

bool IsMsiCommand(const std::wstring& command) {
    std::wstring low = ToLower(command);
    return low.find(L"msiexec") != std::wstring::npos
        || low.find(L"rundll32") != std::wstring::npos;
}

} // namespace Utils