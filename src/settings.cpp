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

    swprintf_s(b, L"%d", (int)std::lround(s.intervalMs));
    WritePrivateProfileStringW(kSection, L"IntervalMs", b, p.c_str());

    swprintf_s(b, L"%d", s.button);
    WritePrivateProfileStringW(kSection, L"Button", b, p.c_str());

    swprintf_s(b, L"%d", s.clickType);
    WritePrivateProfileStringW(kSection, L"ClickType", b, p.c_str());

    WritePrivateProfileStringW(kSection, L"Random", s.randomInterval ? L"1" : L"0", p.c_str());

    swprintf_s(b, L"%d", s.minMs);
    WritePrivateProfileStringW(kSection, L"MinMs", b, p.c_str());

    swprintf_s(b, L"%d", s.maxMs);
    WritePrivateProfileStringW(kSection, L"MaxMs", b, p.c_str());

    WritePrivateProfileStringW(kSection, L"FixedPos", s.fixedPos ? L"1" : L"0", p.c_str());

    swprintf_s(b, L"%d", s.posX);
    WritePrivateProfileStringW(kSection, L"PosX", b, p.c_str());

    swprintf_s(b, L"%d", s.posY);
    WritePrivateProfileStringW(kSection, L"PosY", b, p.c_str());

    WritePrivateProfileStringW(kSection, L"FixedCount", s.fixedCount ? L"1" : L"0", p.c_str());

    swprintf_s(b, L"%d", s.count);
    WritePrivateProfileStringW(kSection, L"Count", b, p.c_str());

    swprintf_s(b, L"%d", s.runSeconds);
    WritePrivateProfileStringW(kSection, L"RunSeconds", b, p.c_str());

    swprintf_s(b, L"%d", s.startDelayMs);
    WritePrivateProfileStringW(kSection, L"StartDelayMs", b, p.c_str());

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

    s.intervalMs = std::clamp((double)readInt(L"IntervalMs", (int)std::lround(s.intervalMs)), 1.0, 356400000.0);
    s.button = std::clamp(readInt(L"Button", s.button), 0, 2);
    s.clickType = std::clamp(readInt(L"ClickType", s.clickType), 0, 3);
    s.randomInterval = readInt(L"Random", s.randomInterval ? 1 : 0) != 0;
    s.minMs = std::clamp(readInt(L"MinMs", s.minMs), 1, 3600000);
    s.maxMs = std::clamp(readInt(L"MaxMs", s.maxMs), 1, 3600000);
    s.fixedPos = readInt(L"FixedPos", s.fixedPos ? 1 : 0) != 0;
    s.posX = readInt(L"PosX", s.posX);
    s.posY = readInt(L"PosY", s.posY);
    s.fixedCount = readInt(L"FixedCount", s.fixedCount ? 1 : 0) != 0;
    s.count = std::clamp(readInt(L"Count", s.count), 1, 1000000000);
    s.runSeconds = std::clamp(readInt(L"RunSeconds", s.runSeconds), 0, 86400);
    s.startDelayMs = std::clamp(readInt(L"StartDelayMs", s.startDelayMs), 0, 3600000);
    s.hotkeyVk = (UINT)readInt(L"HotkeyVk", (int)s.hotkeyVk);
    s.hotkeyMod = readInt(L"HotkeyMod", s.hotkeyMod);
}