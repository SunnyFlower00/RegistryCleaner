#include <conio.h>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include "Console.h"
#include "Utils.h"
#include "UninstallEntry.h"
#include "RegistryScanner.h"
#include "BackupManager.h"
#include "RestorePointManager.h"
#include "EntryRemover.h"

class App {
public:
    int Run() {
        RegistryScanner scanner;
        std::vector<UninstallEntry> found = scanner.Scan();

        Console::Print(L"Проверено записей: " + Utils::Num(scanner.TotalChecked()) + L"\n\n");

        PrintGroup(found, true,  L"=== Высокая уверенность: ");
        PrintGroup(found, false, L"=== Нужна проверка (системные установщики): ");

        // выбор разрешён только из группы "Высокая уверенность"
        std::vector<const UninstallEntry*> candidates;
        for (const auto& e : found)
            if (e.IsConfident()) candidates.push_back(&e);
        if (candidates.empty()) return 0;

        std::vector<const UninstallEntry*> selected = AskSelection(candidates);
        if (selected.empty()) { Console::Print(L"Ничего не выбрано. Выход.\n"); return 0; }

        return DeleteFlow(selected);
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

    std::vector<const UninstallEntry*> AskSelection(
            const std::vector<const UninstallEntry*>& candidates) {
        Console::Print(L"Введите номера записей из группы \"Высокая уверенность\" для удаления\n"
                       L"(через пробел, например: 1 2 4). Пустая строка - выход.\n> ");
        std::string line;
        std::getline(std::cin, line);

        std::set<size_t> nums;
        size_t cur = 0;
        bool inNum = false;
        for (size_t i = 0; i <= line.size(); ++i) {
            char c = (i < line.size()) ? line[i] : ' ';
            if (c >= '0' && c <= '9') {
                cur = cur * 10 + (c - '0');
                inNum = true;
            } else {
                if (inNum && cur >= 1 && cur <= candidates.size()) nums.insert(cur);
                cur = 0;
                inNum = false;
            }
        }

        std::vector<const UninstallEntry*> result;
        for (size_t n : nums) result.push_back(candidates[n - 1]);
        return result;
    }

    int DeleteFlow(const std::vector<const UninstallEntry*>& selected) {
        Console::Print(L"\nБудут удалены записи реестра:\n");
        for (const auto* e : selected)
            Console::Print(L"  - " + e->Name() + L"\n");
        Console::Print(L"\nПодтвердить удаление? Сначала будут созданы точка восстановления\n"
                       L"и резервные копии. (y/n): ");
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
            Console::Print(L"Не удалось создать папку для резервных копий. Удаление отменено.\n");
            return 1;
        }

        size_t removed = 0, failed = 0, skipped = 0;
        for (const auto* e : selected) {
            if (!backup.Backup(*e)) {
                Console::Print(L"  [пропущено] " + e->Name() + L": бэкап не создан\n");
                ++skipped;
                continue;
            }
            std::wstring rerr;
            if (EntryRemover::Remove(*e, rerr)) {
                Console::Print(L"  [удалено]   " + e->Name() + L"\n");
                ++removed;
            } else {
                Console::Print(L"  [ошибка]    " + e->Name() + L": " + rerr + L"\n");
                ++failed;
            }
        }

        Console::Print(L"\nУдалено: " + Utils::Num(removed) +
                       L", ошибок: " + Utils::Num(failed) +
                       L", пропущено: " + Utils::Num(skipped) + L"\n");
        Console::Print(L"Резервные копии: " + backup.Directory() + L"\n");
        Console::Print(L"Для отката дважды щёлкните нужный .reg-файл.\n");
        return 0;
    }
};

int main() {
    App app;
    return app.Run();
}