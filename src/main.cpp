#include <conio.h>
#include <vector>
#include "Console.h"
#include "Utils.h"
#include "UninstallEntry.h"
#include "RegistryScanner.h"
#include "BackupManager.h"
#include "RestorePointManager.h"

class App {
public:
    int Run() {
        RegistryScanner scanner;
        std::vector<UninstallEntry> found = scanner.Scan();

        Console::Print(L"Проверено записей: " + Utils::Num(scanner.TotalChecked()) + L"\n\n");

        size_t confident = PrintGroup(found, true,  L"=== Высокая уверенность: ");
        PrintGroup(found, false, L"=== Нужна проверка (системные установщики): ");

        if (confident == 0) return 0;
        return BackupFlow(found);
    }

private:
    size_t PrintGroup(const std::vector<UninstallEntry>& list, bool confident,
                      const std::wstring& title) {
        size_t count = 0;
        for (const auto& e : list) if (e.IsConfident() == confident) ++count;
        Console::Print(title + Utils::Num(count) + L" ===\n\n");

        size_t n = 0;
        for (const auto& e : list) {
            if (e.IsConfident() != confident) continue;
            Console::Print(L"[" + Utils::Num(++n) + L"] " + e.Name() + L"\n");
            Console::Print(L"    Раздел: " + e.HiveLabel() + L"\\...\\Uninstall\\" + e.KeyName() + L"\n");
            Console::Print(L"    Причина: " + e.Reason() + L"\n\n");
        }
        return count;
    }

    int BackupFlow(const std::vector<UninstallEntry>& list) {
        Console::Print(L"Создать точку восстановления и резервные копии для группы \"Высокая уверенность\"?\n"
                       L"Ничего удалено не будет. (y/n): ");
        int ch = _getch();
        if (ch != 'y' && ch != 'Y') { Console::Print(L"\nОтмена.\n"); return 0; }
        Console::Print(L"\n\n");

        Console::Print(L"Создаю точку восстановления... ");
        std::wstring err;
        if (RestorePointManager::Create(L"Очистка реестра: перед удалением записей", err))
            Console::Print(L"готово\n");
        else
            Console::Print(L"не удалось: " + err + L"\n");

        BackupManager backup;
        if (!backup.Prepare()) {
            Console::Print(L"Не удалось создать папку для резервных копий.\n");
            return 1;
        }
        size_t ok = 0, fail = 0;
        for (const auto& e : list) {
            if (!e.IsConfident()) continue;
            if (backup.Backup(e)) ++ok; else ++fail;
        }
        Console::Print(L"Резервные копии: сохранено " + Utils::Num(ok) +
                       L", ошибок " + Utils::Num(fail) + L"\n");
        Console::Print(L"Папка: " + backup.Directory() + L"\n");
        return 0;
    }
};

int main() {
    App app;
    return app.Run();
}