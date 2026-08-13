#include "settings.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
const wchar_t* kIniName = L"AutoCliker-Siris.ini";
const wchar_t* kSection = L"Click";
}

std::wstring SettingsPath() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    auto pos = p.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        p.erase(pos + 1);
    return p + kIniName;
}

void SaveSettings(const ClickSettings& s) {
    const std::wstring p = SettingsPath();
    wchar_t b[64];

    swprintf_s(b, L"%d", (int)std::lround(s.cps * 100.0));
    WritePrivateProfileStringW(kSection, L"CpsX100", b, p.c_str());

    swprintf_s(b, L"%d", s.button);
    WritePrivateProfileStringW(kSection, L"Button", b, p.c_str());

    swprintf_s(b, L"%d", s.clickType);
    WritePrivateProfileStringW(kSection, L"ClickType", b, p.c_str());

    WritePrivateProfileStringW(kSection, L"Random", s.randomInterval ? L"1" : L"0", p.c_str());

    swprintf_s(b, L"%d", s.minMs);
    WritePrivateProfileStringW(kSection, L"MinMs", b, p.c_str());

    swprintf_s(b, L"%d", s.maxMs);
    WritePrivateProfileStringW(kSection, L"MaxMs", b, p.c_str());

    WritePrivateProfileStringW(kSection, L"FixedCount", s.fixedCount ? L"1" : L"0", p.c_str());

    swprintf_s(b, L"%d", s.count);
    WritePrivateProfileStringW(kSection, L"Count", b, p.c_str());

    swprintf_s(b, L"%u", s.hotkeyVk);
    WritePrivateProfileStringW(kSection, L"HotkeyVk", b, p.c_str());

    swprintf_s(b, L"%d", s.hotkeyMod);
    WritePrivateProfileStringW(kSection, L"HotkeyMod", b, p.c_str());
}

void LoadSettings(ClickSettings& s) {
    const std::wstring p = SettingsPath();
    auto readInt = [&](const wchar_t* key, int def) -> int {
        return (int)GetPrivateProfileIntW(kSection, key, (UINT)def, p.c_str());
    };

    s.cps = std::clamp(readInt(L"CpsX100", (int)std::lround(s.cps * 100.0)) / 100.0, 1.0, 1000.0);
    s.button = std::clamp(readInt(L"Button", s.button), 0, 2);
    s.clickType = std::clamp(readInt(L"ClickType", s.clickType), 0, 3);
    s.randomInterval = readInt(L"Random", s.randomInterval ? 1 : 0) != 0;
    s.minMs = std::clamp(readInt(L"MinMs", s.minMs), 1, 10000);
    s.maxMs = std::clamp(readInt(L"MaxMs", s.maxMs), 1, 10000);
    s.fixedCount = readInt(L"FixedCount", s.fixedCount ? 1 : 0) != 0;
    s.count = std::clamp(readInt(L"Count", s.count), 1, 1000000000);
    s.hotkeyVk = (UINT)readInt(L"HotkeyVk", (int)s.hotkeyVk);
    s.hotkeyMod = readInt(L"HotkeyMod", s.hotkeyMod);
}
