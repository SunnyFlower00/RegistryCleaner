#pragma once
#include <string>
#include "UninstallEntry.h"

class BackupManager {
public:
    // Создаёт папку backup\ГГГГ-ММ-ДД_ЧЧ-ММ-СС рядом с exe
    bool Prepare();

    // Экспортирует одну запись в .reg-файл
    bool Backup(const UninstallEntry& entry);

    const std::wstring& Directory() const { return dir_; }

private:
    static bool RunReg(const std::wstring& args);
    static std::wstring SafeFileName(const std::wstring& s);

    std::wstring dir_;
};