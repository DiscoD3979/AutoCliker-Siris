#pragma once
#include <windows.h>
#include <string>

struct ClickSettings {
    double cps = 10.0;          // кликов в секунду
    int button = 0;             // 0 = ЛКМ, 1 = ПКМ, 2 = средняя
    int clickType = 0;          // 0 = один, 1 = двойной, 2 = тройной, 3 = удержание
    bool randomInterval = false;
    int minMs = 100;
    int maxMs = 300;
    bool fixedCount = false;
    int count = 100;
    UINT hotkeyVk = VK_F6;
    int hotkeyMod = 0;
};

std::wstring SettingsPath();
void SaveSettings(const ClickSettings& s);
void LoadSettings(ClickSettings& s);
