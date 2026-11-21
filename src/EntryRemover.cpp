#include "EntryRemover.h"
#include "Platform.h"
#include "Utils.h"

// RegDeleteTreeW подгружаем динамически: в некоторых версиях заголовков MinGW она не объявлена
typedef LONG (WINAPI *RegDeleteTreeW_t)(HKEY, LPCWSTR);

static RegDeleteTreeW_t LoadRegDeleteTree() {
    HMODULE lib = GetModuleHandleW(L"advapi32.dll");
    if (!lib) lib = LoadLibraryW(L"advapi32.dll");
    if (!lib) return nullptr;
    return reinterpret_cast<RegDeleteTreeW_t>(
        reinterpret_cast<void*>(GetProcAddress(lib, "RegDeleteTreeW")));
}

bool EntryRemover::Remove(const UninstallEntry& e, std::wstring& error) {
    // защита: имя подраздела не должно быть пустым или содержать путь
    if (e.KeyName().empty() || e.KeyName().find(L'\\') != std::wstring::npos) {
        error = L"некорректное имя ключа";
        return false;
    }

    RegDeleteTreeW_t deleteTree = LoadRegDeleteTree();
    if (!deleteTree) {
        error = L"функция RegDeleteTreeW недоступна";
        return false;
    }

    HKEY root = (e.Hive() == RegHive::CurrentUser) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    REGSAM view = (e.Hive() == RegHive::LocalMachine32) ? KEY_WOW64_32KEY : KEY_WOW64_64KEY;

    HKEY parent;
    LONG r = RegOpenKeyExW(root,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        0, KEY_READ | KEY_WRITE | view, &parent);
    if (r != ERROR_SUCCESS) {
        error = L"не удалось открыть раздел Uninstall, код " + Utils::Num(r) +
                (r == ERROR_ACCESS_DENIED ? L" (нужен запуск от имени администратора)" : L"");
        return false;
    }

    r = deleteTree(parent, e.KeyName().c_str());
    RegCloseKey(parent);
    if (r != ERROR_SUCCESS) {
        error = L"код ошибки " + Utils::Num(r);
        return false;
    }
    return true;
}