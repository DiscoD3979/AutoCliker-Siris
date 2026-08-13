#include "settings.h"
#include <windows.h>

std::wstring SettingsPath() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    auto pos = p.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        p.erase(pos + 1);
    return p;
}