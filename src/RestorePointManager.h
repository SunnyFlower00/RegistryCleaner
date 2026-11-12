#pragma once
#include <string>

class RestorePointManager {
public:
    // Создаёт точку восстановления. При неудаче записывает причину в error.
    static bool Create(const std::wstring& description, std::wstring& error);
};