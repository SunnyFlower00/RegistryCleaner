#pragma once
#include <string>
#include <utility>

enum class RegHive { LocalMachine64, LocalMachine32, CurrentUser };

class UninstallEntry {
public:
    UninstallEntry(RegHive hive, std::wstring keyName, std::wstring name,
                   std::wstring reason, bool confident)
        : hive_(hive), keyName_(std::move(keyName)), name_(std::move(name)),
          reason_(std::move(reason)), confident_(confident) {}

    RegHive Hive() const { return hive_; }
    const std::wstring& KeyName() const { return keyName_; }
    const std::wstring& Name() const { return name_; }
    const std::wstring& Reason() const { return reason_; }
    bool IsConfident() const { return confident_; }

    // Подпись для вывода, например "HKLM (64-bit)"
    std::wstring HiveLabel() const {
        switch (hive_) {
            case RegHive::LocalMachine64: return L"HKLM (64-bit)";
            case RegHive::LocalMachine32: return L"HKLM (32-bit)";
            default:                      return L"HKCU";
        }
    }

    // Короткий тег для имён файлов
    std::wstring HiveTag() const {
        switch (hive_) {
            case RegHive::LocalMachine64: return L"HKLM64";
            case RegHive::LocalMachine32: return L"HKLM32";
            default:                      return L"HKCU";
        }
    }

    // Полный путь для reg.exe
    std::wstring FullRegPath() const {
        const wchar_t* root = (hive_ == RegHive::CurrentUser) ? L"HKCU" : L"HKLM";
        return std::wstring(root) +
               L"\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + keyName_;
    }

    // Флаг разрядности реестра для reg.exe
    const wchar_t* RegViewFlag() const {
        return hive_ == RegHive::LocalMachine32 ? L"/reg:32" : L"/reg:64";
    }

private:
    RegHive hive_;
    std::wstring keyName_;
    std::wstring name_;
    std::wstring reason_;
    bool confident_;
};