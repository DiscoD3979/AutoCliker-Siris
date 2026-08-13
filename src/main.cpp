#include "ui.h"
#include <windows.h>
#include <gdiplus.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    Gdiplus::GdiplusStartupInput si;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &si, nullptr) != Gdiplus::Ok)
        return 1;

    const int rc = RunApp(hInstance);
    Gdiplus::GdiplusShutdown(token);
    return rc;
}