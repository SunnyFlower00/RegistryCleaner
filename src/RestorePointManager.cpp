#include "RestorePointManager.h"
#include "Platform.h"
#include "Utils.h"

#pragma pack(push, 1)
struct RestorePointInfoW {
    DWORD dwEventType;
    DWORD dwRestorePtType;
    INT64 llSequenceNumber;
    WCHAR szDescription[256];
};
struct StateMgrStatus {
    DWORD nStatus;
    INT64 llSequenceNumber;
};
#pragma pack(pop)

typedef BOOL (WINAPI *SRSetRestorePointW_t)(RestorePointInfoW*, StateMgrStatus*);

bool RestorePointManager::Create(const std::wstring& description, std::wstring& error) {
    HMODULE lib = LoadLibraryW(L"srclient.dll");
    if (!lib) { error = L"не удалось загрузить srclient.dll"; return false; }

    SRSetRestorePointW_t fn =
        reinterpret_cast<SRSetRestorePointW_t>(GetProcAddress(lib, "SRSetRestorePointW"));
    if (!fn) { FreeLibrary(lib); error = L"функция SRSetRestorePointW не найдена"; return false; }

    RestorePointInfoW info = {};
    StateMgrStatus st = {};
    info.dwEventType = 100;        // BEGIN_SYSTEM_CHANGE
    info.dwRestorePtType = 12;     // MODIFY_SETTINGS
    lstrcpynW(info.szDescription, description.c_str(), 256);

    if (!fn(&info, &st)) {
        error = L"код ошибки " + Utils::Num(st.nStatus) +
                (st.nStatus == 5    ? L" (нужен запуск от имени администратора)" :
                 st.nStatus == 1058 ? L" (защита системы отключена)" : L"");
        FreeLibrary(lib);
        return false;
    }
    info.dwEventType = 101;        // END_SYSTEM_CHANGE
    info.llSequenceNumber = st.llSequenceNumber;
    fn(&info, &st);
    FreeLibrary(lib);
    return true;
}