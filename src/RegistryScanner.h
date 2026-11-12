#pragma once
#include <string>
#include <vector>
#include "Platform.h"
#include "UninstallEntry.h"

class RegistryScanner {
public:
    // Сканирует реестр и возвращает найденные устаревшие записи
    std::vector<UninstallEntry> Scan();

    // Сколько записей проверено при последнем сканировании
    int TotalChecked() const { return total_; }

private:
    void ScanHive(HKEY root, RegHive hive, REGSAM view);
    static bool ReadString(HKEY key, const wchar_t* value, std::wstring& out);
    static bool ReadDword(HKEY key, const wchar_t* value, DWORD& out);

    std::vector<UninstallEntry> found_;
    int total_ = 0;
};