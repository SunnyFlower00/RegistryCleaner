#pragma once
#include <string>

namespace Utils {
    std::wstring Num(size_t n);
    std::wstring Pad2(size_t v);
    std::wstring ToLower(std::wstring s);
    std::wstring Trim(std::wstring s);
    std::wstring ExeDir();

    bool MissingExact(const std::wstring& path);   // точно "не найдено"
    bool PathMissing(const std::wstring& path);    // то же + учёт System32/SysWOW64
    bool DriveMissing(const std::wstring& path);   // диск недоступен

    std::wstring ExtractExePath(const std::wstring& command);
    bool IsMsiCommand(const std::wstring& command);
}