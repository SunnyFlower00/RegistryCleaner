#pragma once
#include <string>
#include "UninstallEntry.h"

class EntryRemover {
public:
    // Удаляет ключ записи из реестра. При неудаче пишет причину в error.
    static bool Remove(const UninstallEntry& entry, std::wstring& error);
};