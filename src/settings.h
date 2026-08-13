#pragma once
#include <windows.h>
#include <string>

struct ClickSettings {
    double intervalMs = 100.0;  // базовый интервал кликов (1 мс .. 99 ч)
    int button = 0;             // 0 = ЛКМ, 1 = ПКМ, 2 = средняя
    int clickType = 0;          // 0 = один, 1 = двойной, 2 = тройной, 3 = удержание
    bool randomInterval = false;
    int minMs = 100;            // случайный разброс, мс
    int maxMs = 300;
    bool fixedPos = false;      // клик в фиксированной точке
    int posX = 0;
    int posY = 0;
    bool fixedCount = false;
    int count = 100;
    int runSeconds = 0;         // 0 = бесконечно, иначе лимит по времени
    int startDelayMs = 0;       // задержка перед стартом
    UINT hotkeyVk = VK_F6;
    int hotkeyMod = 0;
};

std::wstring SettingsPath();
void SaveSettings(const ClickSettings& s);
void LoadSettings(ClickSettings& s);