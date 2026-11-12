#include "Console.h"
#include "Platform.h"

void Console::Print(const std::wstring& s) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    DWORD written;
    if (GetConsoleMode(h, &mode)) {
        WriteConsoleW(h, s.c_str(), (DWORD)s.size(), &written, nullptr);
    } else {
        // вывод перенаправлен в файл или канал: пишем в UTF-8
        int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string out(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
        WriteFile(h, out.data(), (DWORD)out.size(), &written, nullptr);
    }
}