#include "ui.h"
#include "clicker.h"
#include "settings.h"
#include "theme.h"

#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace Gdiplus;

namespace {

constexpr int kWndW = 380;
constexpr int kWndH = 720;
constexpr UINT_PTR kTimerId = 1;
constexpr UINT_PTR kPickTimerId = 2;
constexpr int kHotkeyId = 1;
constexpr UINT WM_APP_POS = WM_APP + 1;
constexpr UINT WM_APP_POS_CANCEL = WM_APP + 2;

Color Col(COLORREF c) {
    return Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
}

bool PtIn(const RECT& r, POINT p) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

struct App {
    HINSTANCE inst = nullptr;
    HWND hwnd = nullptr;
    Clicker clicker;
    ClickSettings set;
    bool running = false;
    bool capturing = false;
    bool picking = false;
    HHOOK hook = nullptr;
    HWND hint = nullptr;
    bool hoverMin = false;
    bool hoverClose = false;
    bool hoverStart = false;
    bool hoverStop = false;
    bool hoverPick = false;
    bool hoverSlider = false;
    int hoverRow = 0;
    int pulse = 0;

    RECT rcPanels[4]{}, rcCk{}, rcRandMin{}, rcRandMax{}, rcFix{}, rcX{}, rcY{}, rcPickBtn{};
    RECT rcBtn[3]{}, rcType[4]{}, rcMode[3]{};
    RECT rcEdCount{}, rcEdTime{}, rcEdDelay{}, rcEdHot{};
    RECT rcBtnStart{}, rcBtnStop{}, rcMinBtn{}, rcCloseBtn{};

    HFONT hFontBase = nullptr, hFontBold = nullptr;
    Font* fBase = nullptr;
    Font* fBold = nullptr;
    Font* fSmall = nullptr;
    Font* fTitle = nullptr;
    HBRUSH brBg = nullptr, brField = nullptr;

    HWND edH = nullptr, edM = nullptr, edS = nullptr, edMs = nullptr;
    HWND edRandMin = nullptr, edRandMax = nullptr;
    HWND edCount = nullptr, edTime = nullptr, edDelay = nullptr;
    HWND edX = nullptr, edY = nullptr, edHot = nullptr;
    HWND focusedEdit = nullptr;
} app;

WNDPROC g_origEdit = nullptr;

void RoundedRect(Graphics& g, const RECT& r, int rad, const Color& fill, const Color& border, float bw) {
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    int d = std::min(rad * 2, std::min(w, h));
    if (d < 2) {
        if (fill.GetAlpha()) {
            SolidBrush b(fill);
            g.FillRectangle(&b, (REAL)r.left, (REAL)r.top, (REAL)w, (REAL)h);
        }
        if (border.GetAlpha() && bw > 0) {
            Pen p(border, bw);
            g.DrawRectangle(&p, (REAL)r.left, (REAL)r.top, (REAL)(w - 1), (REAL)(h - 1));
        }
        return;
    }
    GraphicsPath path;
    path.AddArc((REAL)r.left, (REAL)r.top, (REAL)d, (REAL)d, 180.0f, 90.0f);
    path.AddArc((REAL)(r.left + w - d), (REAL)r.top, (REAL)d, (REAL)d, 270.0f, 90.0f);
    path.AddArc((REAL)(r.left + w - d), (REAL)(r.top + h - d), (REAL)d, (REAL)d, 0.0f, 90.0f);
    path.AddArc((REAL)r.left, (REAL)(r.top + h - d), (REAL)d, (REAL)d, 90.0f, 90.0f);
    path.CloseFigure();
    if (fill.GetAlpha()) {
        SolidBrush b(fill);
        g.FillPath(&b, &path);
    }
    if (border.GetAlpha() && bw > 0) {
        Pen p(border, bw);
        g.DrawPath(&p, &path);
    }
}

void InitRects() {
    app.rcPanels[0] = { 24, 78, 86, 110 };
    app.rcPanels[1] = { 95, 78, 157, 110 };
    app.rcPanels[2] = { 166, 78, 228, 110 };
    app.rcPanels[3] = { 237, 78, 299, 110 };
    app.rcCk = { 24, 132, 356, 152 };
    app.rcRandMin = { 48, 160, 118, 192 };
    app.rcRandMax = { 150, 160, 220, 192 };
    app.rcBtn[0] = { 24, 230, 116, 254 };
    app.rcBtn[1] = { 120, 230, 212, 254 };
    app.rcBtn[2] = { 216, 230, 308, 254 };
    app.rcType[0] = { 24, 292, 180, 316 };
    app.rcType[1] = { 200, 292, 356, 316 };
    app.rcType[2] = { 24, 318, 180, 342 };
    app.rcType[3] = { 200, 318, 356, 342 };
    app.rcFix = { 24, 380, 356, 400 };
    app.rcX = { 48, 404, 118, 436 };
    app.rcY = { 150, 404, 220, 436 };
    app.rcPickBtn = { 228, 404, 356, 436 };
    app.rcMode[0] = { 24, 474, 150, 498 };
    app.rcMode[1] = { 164, 474, 356, 498 };
    app.rcMode[2] = { 24, 500, 150, 524 };
    app.rcEdCount = { 276, 474, 356, 498 };
    app.rcEdTime = { 164, 500, 244, 524 };
    app.rcEdDelay = { 164, 526, 244, 550 };
    app.rcEdHot = { 24, 588, 356, 620 };
    app.rcBtnStart = { 24, 676, 182, 716 };
    app.rcBtnStop = { 198, 676, 356, 716 };
    app.rcMinBtn = { kWndW - 70, 0, kWndW - 38, theme::kTitleH };
    app.rcCloseBtn = { kWndW - 38, 0, kWndW, theme::kTitleH };
}

std::wstring HotkeyName() {
    wchar_t n[128];
    UINT sc = MapVirtualKeyW(app.set.hotkeyVk, MAPVK_VK_TO_VSC);
    if (GetKeyNameTextW(sc << 16, n, 128) > 0)
        return n;
    swprintf_s(n, L"VK 0x%X", app.set.hotkeyVk);
    return n;
}

void RegisterHotkeyNoBox() {
    UnregisterHotKey(app.hwnd, kHotkeyId);
    RegisterHotKey(app.hwnd, kHotkeyId, (UINT)app.set.hotkeyMod | MOD_NOREPEAT, app.set.hotkeyVk);
}

void UpdateHotkeyText() {
    if (app.edHot)
        SetWindowTextW(app.edHot, app.capturing ? L"Нажмите клавишу..." : HotkeyName().c_str());
}

int ReadEditInt(HWND e, int def) {
    wchar_t b[32];
    if (GetWindowTextW(e, b, 32) > 0)
        return (int)wcstol(b, nullptr, 10);
    return def;
}

void ApplyEditsToSettings() {
    const double h = std::clamp(ReadEditInt(app.edH, 0), 0, 99);
    const double m = std::clamp(ReadEditInt(app.edM, 0), 0, 99);
    const double s = std::clamp(ReadEditInt(app.edS, 0), 0, 99);
    const double ms = std::clamp(ReadEditInt(app.edMs, 0), 0, 999);
    app.set.intervalMs = std::clamp(h * 3600000.0 + m * 60000.0 + s * 1000.0 + ms, 1.0, 356400000.0);

    app.set.minMs = std::clamp(ReadEditInt(app.edRandMin, 100), 1, 3600000);
    app.set.maxMs = std::clamp(ReadEditInt(app.edRandMax, 300), 1, 3600000);
    if (app.set.maxMs < app.set.minMs)
        std::swap(app.set.minMs, app.set.maxMs);

    app.set.count = std::clamp(ReadEditInt(app.edCount, 100), 1, 1000000000);
    app.set.runSeconds = std::clamp(ReadEditInt(app.edTime, 0), 0, 86400);
    app.set.startDelayMs = std::clamp(ReadEditInt(app.edDelay, 0), 0, 3600) * 1000;
}

void FillIntervalPanels() {
    int total = (int)app.set.intervalMs;
    const int h = total / 3600000;
    total %= 3600000;
    const int m = total / 60000;
    total %= 60000;
    const int s = total / 1000;
    total %= 1000;
    wchar_t b[16];
    swprintf_s(b, L"%d", h);
    SetWindowTextW(app.edH, b);
    swprintf_s(b, L"%d", m);
    SetWindowTextW(app.edM, b);
    swprintf_s(b, L"%d", s);
    SetWindowTextW(app.edS, b);
    swprintf_s(b, L"%d", total);
    SetWindowTextW(app.edMs, b);
}

void Toggle() {
    ApplyEditsToSettings();
    if (app.running) {
        app.clicker.Stop();
        app.running = false;
    } else {
        app.clicker.Start(app.set);
        app.running = true;
        app.pulse = 0;
    }
    SaveSettings(app.set);
    InvalidateRect(app.hwnd, nullptr, FALSE);
}

// ---------- выбор позиции клика ----------

LRESULT CALLBACK HintProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        Bitmap bmp(rc.right, rc.bottom);
        {
            Graphics g(&bmp);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
            g.Clear(Col(theme::kBg));
            RoundedRect(g, rc, 9, Col(theme::kBgPanel), Col(theme::kYellow), 1.5f);
            SolidBrush tb(Col(theme::kYellow));
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(L"Нажмите в точку клика  (Esc — отмена)", -1, app.fBold,
                         RectF(8.0f, 0.0f, (REAL)(rc.right - 16), (REAL)rc.bottom), &sf, &tb);
        }
        Graphics gdc(dc);
        gdc.DrawImage(&bmp, 0, 0);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND)
        return 1;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void CreateHint() {
    POINT c;
    GetCursorPos(&c);
    app.hint = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"AutoClikerSirisHint", L"",
                               WS_POPUP, c.x + 24, c.y + 24, 300, 40, nullptr, nullptr, app.inst, nullptr);
    ShowWindow(app.hint, SW_SHOWNOACTIVATE);
}

void DestroyHint() {
    if (app.hint) {
        DestroyWindow(app.hint);
        app.hint = nullptr;
    }
}

void EndPick(int x, int y) {
    app.picking = false;
    KillTimer(app.hwnd, kPickTimerId);
    DestroyHint();
    ShowWindow(app.hwnd, SW_SHOW);
    SetForegroundWindow(app.hwnd);
    if (x >= 0) {
        app.set.fixedPos = true;
        app.set.posX = x;
        app.set.posY = y;
        wchar_t b[32];
        swprintf_s(b, L"%d", x);
        SetWindowTextW(app.edX, b);
        swprintf_s(b, L"%d", y);
        SetWindowTextW(app.edY, b);
    }
    InvalidateRect(app.hwnd, nullptr, FALSE);
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (wParam == WM_LBUTTONDOWN) {
            auto* mi = (MSLLHOOKSTRUCT*)lParam;
            PostMessageW(app.hwnd, WM_APP_POS, (WPARAM)mi->pt.x, (LPARAM)mi->pt.y);
            UnhookWindowsHookEx(app.hook);
            app.hook = nullptr;
            return 1; // клик не должен попасть в приложение под курсором
        }
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            PostMessageW(app.hwnd, WM_APP_POS_CANCEL, 0, 0);
            UnhookWindowsHookEx(app.hook);
            app.hook = nullptr;
            return 0;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void StartPick() {
    if (app.picking || app.running)
        return;
    app.picking = true;
    ShowWindow(app.hwnd, SW_HIDE);
    app.hook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, GetModuleHandleW(nullptr), 0);
    if (!app.hook) {
        app.picking = false;
        ShowWindow(app.hwnd, SW_SHOW);
        return;
    }
    CreateHint();
    SetTimer(app.hwnd, kPickTimerId, 50, nullptr);
}

// ---------- отрисовка ----------

void Header(Graphics& g, const wchar_t* t, int y) {
    RoundedRect(g, { 24, y + 3, 30, y + 9 }, 3, Col(theme::kYellow), Color(255, 0, 0, 0), 0);
    SolidBrush tb(Col(theme::kText));
    g.DrawString(t, -1, app.fBold, PointF(40.0f, (REAL)y), &tb);
    Pen ln(Col(theme::kBorder));
    g.DrawLine(&ln, 24.0f, (REAL)(y + 20), (REAL)(kWndW - 24), (REAL)(y + 20));
}

void DrawTitleBtn(Graphics& g, const RECT& r, const wchar_t* glyph, bool hover) {
    SolidBrush hb(hover ? Col(theme::kYellow) : Color(255, 28, 28, 33));
    g.FillRectangle(&hb, (REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
    SolidBrush tb(hover ? Col(theme::kBg) : Col(theme::kTextDim));
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(glyph, -1, app.fTitle,
                 RectF((REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top)),
                 &sf, &tb);
}

void PaintTitle(Graphics& g, int W) {
    SolidBrush panel(Col(theme::kBgPanel));
    g.FillRectangle(&panel, 0.0f, 0.0f, (REAL)W, (REAL)theme::kTitleH);
    SolidBrush yel(Col(theme::kYellow));
    g.FillRectangle(&yel, 0.0f, (REAL)(theme::kTitleH - 3), (REAL)W, 3.0f);
    Pen glow(Col(theme::kYellowDim), 1.0f);
    g.DrawLine(&glow, 0.0f, (REAL)(theme::kTitleH - 4), (REAL)W, (REAL)(theme::kTitleH - 4));

    StringFormat sf;
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF m;
    g.MeasureString(L"AutoCliker", -1, app.fTitle, PointF(18.0f, 0.0f), &sf, &m);
    SolidBrush tb(Col(theme::kText));
    g.DrawString(L"AutoCliker", -1, app.fTitle, PointF(18.0f, 0.0f), &sf, &tb);
    g.DrawString(L"Siris", -1, app.fTitle, PointF(18.0f + m.Width, 0.0f), &sf, &yel);

    SolidBrush vb(Col(theme::kTextDim));
    StringFormat vsf;
    vsf.SetAlignment(StringAlignmentFar);
    vsf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"v1.0.0", -1, app.fSmall, RectF(0.0f, 0.0f, (REAL)(app.rcMinBtn.left - 12), (REAL)theme::kTitleH),
                 &vsf, &vb);

    DrawTitleBtn(g, app.rcMinBtn, L"\u2013", app.hoverMin);
    DrawTitleBtn(g, app.rcCloseBtn, L"\u00D7", app.hoverClose);
}

void PaintCard(Graphics& g, const RECT& r) {
    RoundedRect(g, r, 10, Col(theme::kBgPanel), Col(theme::kBorder), 1);
}

void PaintCheck(Graphics& g, const RECT& r, bool on, const wchar_t* label, bool hover) {
    if (hover) {
        SolidBrush hb(Col(theme::kBgField));
        RoundedRect(g, r, 6, Col(theme::kBgField), Color(255, 0, 0, 0), 0);
    }
    int top = r.top + (r.bottom - r.top - 16) / 2;
    RECT box = { r.left, top, r.left + 16, top + 16 };
    RoundedRect(g, box, 4, Col(theme::kBgField), on ? Col(theme::kYellow) : Col(theme::kBorder), 1);
    if (on) {
        Pen ck(Col(theme::kBg), 2.0f);
        g.DrawLine(&ck, (REAL)(box.left + 4), (REAL)(box.top + 8), (REAL)(box.left + 7), (REAL)(box.top + 11));
        g.DrawLine(&ck, (REAL)(box.left + 7), (REAL)(box.top + 11), (REAL)(box.left + 12), (REAL)(box.top + 5));
    }
    SolidBrush lb(Col(theme::kText));
    g.DrawString(label, -1, app.fBase,
                 PointF((REAL)(box.right + 10), (REAL)(r.top + (r.bottom - r.top - 14) / 2)), &lb);
}

void PaintRadio(Graphics& g, const RECT& r, bool on, const wchar_t* label, bool rowHover) {
    if (rowHover) {
        SolidBrush hb(Col(theme::kBgField));
        RoundedRect(g, r, 6, Col(theme::kBgField), Color(255, 0, 0, 0), 0);
    }
    int cy = (r.top + r.bottom) / 2;
    SolidBrush fb(Col(theme::kBgField));
    g.FillEllipse(&fb, (REAL)r.left, (REAL)(cy - 8), 16.0f, 16.0f);
    Pen bp(on ? Col(theme::kYellow) : Col(theme::kBorder));
    g.DrawEllipse(&bp, (REAL)r.left, (REAL)(cy - 8), 16.0f, 16.0f);
    if (on) {
        SolidBrush db(Col(theme::kYellow));
        g.FillEllipse(&db, (REAL)(r.left + 4), (REAL)(cy - 4), 8.0f, 8.0f);
    }
    SolidBrush lb(Col(theme::kText));
    g.DrawString(label, -1, app.fBase,
                 PointF((REAL)(r.left + 22), (REAL)(r.top + (r.bottom - r.top - 14) / 2)), &lb);
}

void PaintEditBorder(Graphics& g, const RECT& r, bool focused) {
    RoundedRect(g, r, 5, Color(255, 0, 0, 0), focused ? Col(theme::kYellow) : Col(theme::kBorder), 1);
}

void PaintButton(Graphics& g, const RECT& r, const wchar_t* label, bool primary, bool enabled, bool hover) {
    Color fill, border, text;
    if (primary) {
        if (!enabled) {
            fill = Color(255, 58, 50, 24);
            border = Color(255, 90, 74, 24);
            text = Color(255, 140, 120, 60);
        } else if (hover) {
            fill = Col(theme::kYellowHover);
            border = Col(theme::kYellowHover);
            text = Col(theme::kBg);
        } else {
            fill = Col(theme::kYellow);
            border = Col(theme::kYellow);
            text = Col(theme::kBg);
        }
    } else {
        if (!enabled) {
            fill = Color(255, 24, 24, 29);
            border = Col(theme::kBorder);
            text = Col(theme::kTextDim);
        } else if (hover) {
            fill = Color(255, 46, 40, 18);
            border = Col(theme::kYellowHover);
            text = Col(theme::kYellowHover);
        } else {
            fill = Color(255, 28, 28, 34);
            border = Col(theme::kYellow);
            text = Col(theme::kYellow);
        }
    }
    RoundedRect(g, r, 8, fill, border, 1);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    SolidBrush tb(text);
    g.DrawString(label, -1, app.fBold,
                 RectF((REAL)r.left, (REAL)r.top, (REAL)(r.right - r.left), (REAL)(r.bottom - r.top)),
                 &sf, &tb);
}

void PaintStatus(Graphics& g, int W) {
    const int y = 638;
    Color dc = app.running ? (app.pulse ? Col(theme::kYellow) : Col(theme::kYellowDim)) : Col(theme::kGreen);
    SolidBrush db(dc);
    g.FillEllipse(&db, 32.0f, (REAL)(y + 6), 10.0f, 10.0f);

    SolidBrush tb(app.running ? Col(theme::kYellow) : Col(theme::kTextDim));
    g.DrawString(app.running ? L"Работает" : L"Остановлен", -1, app.fBold, PointF(50.0f, (REAL)(y + 2)), &tb);

    wchar_t b[64];
    swprintf_s(b, L"Кликов: %lld", (long long)app.clicker.Clicks());
    SolidBrush cb(Col(theme::kTextDim));
    StringFormat fr;
    fr.SetAlignment(StringAlignmentFar);
    g.DrawString(b, -1, app.fBase, RectF(32.0f, (REAL)(y + 2), (REAL)(W - 64), 18.0f), &fr, &cb);
}

void PaintSections(Graphics& g, int W) {
    // карточки секций
    PaintCard(g, { 16, 48, 364, 200 });
    PaintCard(g, { 16, 208, 364, 262 });
    PaintCard(g, { 16, 270, 364, 350 });
    PaintCard(g, { 16, 358, 364, 444 });
    PaintCard(g, { 16, 452, 364, 558 });
    PaintCard(g, { 16, 566, 364, 666 });
    PaintCard(g, { 16, 674, 364, 718 });

    Header(g, L"ИНТЕРВАЛ КЛИКА", 60);
    Header(g, L"КНОПКА МЫШИ", 220);
    Header(g, L"ТИП КЛИКА", 282);
    Header(g, L"ПОЗИЦИЯ КЛИКА", 370);
    Header(g, L"РЕЖИМ", 464);
    Header(g, L"ГОРЯЧАЯ КЛАВИША", 578);

    SolidBrush lb(Col(theme::kTextDim));

    // панели Ч М С мс
    const wchar_t* units[4] = { L"ч", L"м", L"с", L"мс" };
    for (int i = 0; i < 4; ++i) {
        PaintEditBorder(g, app.rcPanels[i], i == 0 ? app.focusedEdit == app.edH
                              : i == 1 ? app.focusedEdit == app.edM
                              : i == 2 ? app.focusedEdit == app.edS
                                       : app.focusedEdit == app.edMs);
        StringFormat cf;
        cf.SetAlignment(StringAlignmentCenter);
        g.DrawString(units[i], -1, app.fSmall,
                     RectF((REAL)app.rcPanels[i].left, 113.0f, (REAL)(app.rcPanels[i].right - app.rcPanels[i].left), 14.0f),
                     &cf, &lb);
    }

    PaintCheck(g, app.rcCk, app.set.randomInterval, L"Случайный интервал (мс)", app.hoverRow == 1);

    g.DrawString(L"От:", -1, app.fBase, PointF(24.0f, 169.0f), &lb);
    g.DrawString(L"До:", -1, app.fBase, PointF(126.0f, 169.0f), &lb);
    g.DrawString(L"мс", -1, app.fSmall, PointF(228.0f, 171.0f), &lb);
    PaintEditBorder(g, app.rcRandMin, app.focusedEdit == app.edRandMin);
    PaintEditBorder(g, app.rcRandMax, app.focusedEdit == app.edRandMax);

    PaintRadio(g, app.rcBtn[0], app.set.button == 0, L"ЛКМ", app.hoverRow == 2);
    PaintRadio(g, app.rcBtn[1], app.set.button == 1, L"ПКМ", app.hoverRow == 2);
    PaintRadio(g, app.rcBtn[2], app.set.button == 2, L"Средняя", app.hoverRow == 2);

    PaintRadio(g, app.rcType[0], app.set.clickType == 0, L"Один", app.hoverRow == 3);
    PaintRadio(g, app.rcType[1], app.set.clickType == 1, L"Двойной", app.hoverRow == 3);
    PaintRadio(g, app.rcType[2], app.set.clickType == 2, L"Тройной", app.hoverRow == 3);
    PaintRadio(g, app.rcType[3], app.set.clickType == 3, L"Удержание", app.hoverRow == 3);

    PaintCheck(g, app.rcFix, app.set.fixedPos, L"Клик в фиксированной точке", app.hoverRow == 5);
    g.DrawString(L"X:", -1, app.fBase, PointF(24.0f, 413.0f), &lb);
    g.DrawString(L"Y:", -1, app.fBase, PointF(126.0f, 413.0f), &lb);
    PaintEditBorder(g, app.rcX, app.focusedEdit == app.edX);
    PaintEditBorder(g, app.rcY, app.focusedEdit == app.edY);
    PaintButton(g, app.rcPickBtn, L"Выбрать", false, !app.running, app.hoverPick);

    PaintRadio(g, app.rcMode[0], !app.set.fixedCount && app.set.runSeconds == 0, L"Бесконечно", app.hoverRow == 4);
    PaintRadio(g, app.rcMode[1], app.set.fixedCount, L"Количество:", app.hoverRow == 4);
    PaintEditBorder(g, app.rcEdCount, app.focusedEdit == app.edCount);
    PaintRadio(g, app.rcMode[2], app.set.runSeconds > 0, L"Время (сек):", app.hoverRow == 4);
    PaintEditBorder(g, app.rcEdTime, app.focusedEdit == app.edTime);
    g.DrawString(L"Задержка (сек):", -1, app.fBase, PointF(24.0f, 535.0f), &lb);
    PaintEditBorder(g, app.rcEdDelay, app.focusedEdit == app.edDelay);

    PaintEditBorder(g, app.rcEdHot, app.focusedEdit == app.edHot);
    SolidBrush hb(Col(theme::kTextDim));
    g.DrawString(app.capturing ? L"Нажмите клавишу (Esc — отмена)" : L"Горячая клавиша — старт/стоп",
                 -1, app.fSmall, PointF(32.0f, 626.0f), &hb);

    PaintStatus(g, W);

    PaintButton(g, app.rcBtnStart, L"СТАРТ", true, !app.running, app.hoverStart);
    PaintButton(g, app.rcBtnStop, L"СТОП", false, app.running, app.hoverStop);
}

void Paint() {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(app.hwnd, &ps);
    RECT rc;
    GetClientRect(app.hwnd, &rc);
    const int W = rc.right;
    const int H = rc.bottom;

    Bitmap bmp(W, H);
    {
        Graphics gd(&bmp);
        gd.SetSmoothingMode(SmoothingModeAntiAlias);
        gd.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        gd.SetPixelOffsetMode(PixelOffsetModeHalf);
        gd.Clear(Col(theme::kBg));

        SolidBrush edge(Col(theme::kBorder));
        gd.FillRectangle(&edge, 0.0f, 0.0f, (REAL)(W - 1), (REAL)(H - 1));

        PaintTitle(gd, W);
        PaintSections(gd, W);
    }

    Graphics gdc(dc);
    gdc.DrawImage(&bmp, 0, 0);
    EndPaint(app.hwnd, &ps);
}

// ---------- редактирование ----------

LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SETFOCUS) {
        app.focusedEdit = h;
        if (h == app.edHot) {
            app.capturing = true;
            SetWindowTextW(h, L"Нажмите клавишу...");
        }
        InvalidateRect(app.hwnd, nullptr, FALSE);
    } else if (m == WM_KILLFOCUS) {
        if (app.focusedEdit == h) {
            app.focusedEdit = nullptr;
            if (app.capturing) {
                app.capturing = false;
                SetWindowTextW(h, HotkeyName().c_str());
            }
            InvalidateRect(app.hwnd, nullptr, FALSE);
        }
    } else if (m == WM_KEYDOWN) {
        if (h == app.edHot && app.capturing) {
            if (w == VK_ESCAPE) {
                app.capturing = false;
                SetWindowTextW(h, HotkeyName().c_str());
                return 0;
            }
            app.set.hotkeyVk = (UINT)w;
            app.set.hotkeyMod = 0;
            app.capturing = false;
            RegisterHotkeyNoBox();
            SetWindowTextW(h, HotkeyName().c_str());
            SaveSettings(app.set);
            InvalidateRect(app.hwnd, nullptr, FALSE);
            return 0;
        }
    }
    return CallWindowProcW(g_origEdit, h, m, w, l);
}

// ---------- главное окно ----------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            InitRects();
            LoadSettings(app.set);

            app.hFontBase = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            app.hFontBold = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            FontFamily ff(L"Segoe UI");
            app.fBase = new Font(&ff, 10.0f, FontStyleRegular, UnitPixel);
            app.fBold = new Font(&ff, 10.0f, FontStyleBold, UnitPixel);
            app.fSmall = new Font(&ff, 9.0f, FontStyleRegular, UnitPixel);
            app.fTitle = new Font(&ff, 13.0f, FontStyleBold, UnitPixel);

            app.brBg = CreateSolidBrush(theme::kBg);
            app.brField = CreateSolidBrush(theme::kBgField);

            auto makeEdit = [&](const wchar_t* text, int x, int y, int w, int h, DWORD extra, HMENU id) {
                HWND e = CreateWindowExW(0, L"EDIT", text,
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL | extra,
                                         x, y, w, h, hwnd, id, app.inst, nullptr);
                SendMessageW(e, WM_SETFONT, (WPARAM)app.hFontBase, TRUE);
                return e;
            };

            app.edH = makeEdit(L"0", 24, 78, 62, 32, ES_NUMBER | ES_CENTER, (HMENU)1);
            app.edM = makeEdit(L"0", 95, 78, 62, 32, ES_NUMBER | ES_CENTER, (HMENU)2);
            app.edS = makeEdit(L"0", 166, 78, 62, 32, ES_NUMBER | ES_CENTER, (HMENU)3);
            app.edMs = makeEdit(L"100", 237, 78, 62, 32, ES_NUMBER | ES_CENTER, (HMENU)4);
            app.edRandMin = makeEdit(L"100", 48, 160, 70, 32, ES_NUMBER, (HMENU)5);
            app.edRandMax = makeEdit(L"300", 150, 160, 70, 32, ES_NUMBER, (HMENU)6);
            app.edCount = makeEdit(L"100", 276, 474, 80, 32, ES_NUMBER, (HMENU)7);
            app.edTime = makeEdit(L"0", 164, 500, 80, 32, ES_NUMBER, (HMENU)8);
            app.edDelay = makeEdit(L"0", 164, 526, 80, 32, ES_NUMBER, (HMENU)9);
            app.edX = makeEdit(L"0", 48, 404, 70, 32, ES_CENTER | ES_READONLY, (HMENU)10);
            app.edY = makeEdit(L"0", 150, 404, 70, 32, ES_CENTER | ES_READONLY, (HMENU)11);
            app.edHot = makeEdit(L"", 24, 588, 332, 32, ES_CENTER | ES_READONLY, (HMENU)12);

            g_origEdit = (WNDPROC)SetWindowLongPtrW(app.edH, GWLP_WNDPROC, (LONG_PTR)EditProc);
            HWND edits[] = { app.edM, app.edS, app.edMs, app.edRandMin, app.edRandMax,
                             app.edCount, app.edTime, app.edDelay, app.edX, app.edY, app.edHot };
            for (HWND e : edits)
                SetWindowLongPtrW(e, GWLP_WNDPROC, (LONG_PTR)EditProc);

            FillIntervalPanels();

            wchar_t b[32];
            swprintf_s(b, L"%d", app.set.minMs);
            SetWindowTextW(app.edRandMin, b);
            swprintf_s(b, L"%d", app.set.maxMs);
            SetWindowTextW(app.edRandMax, b);
            swprintf_s(b, L"%d", app.set.count);
            SetWindowTextW(app.edCount, b);
            swprintf_s(b, L"%d", app.set.runSeconds);
            SetWindowTextW(app.edTime, b);
            swprintf_s(b, L"%d", app.set.startDelayMs / 1000);
            SetWindowTextW(app.edDelay, b);
            swprintf_s(b, L"%d", app.set.posX);
            SetWindowTextW(app.edX, b);
            swprintf_s(b, L"%d", app.set.posY);
            SetWindowTextW(app.edY, b);
            UpdateHotkeyText();

            EnableWindow(app.edRandMin, app.set.randomInterval);
            EnableWindow(app.edRandMax, app.set.randomInterval);
            EnableWindow(app.edCount, app.set.fixedCount);

            if (!RegisterHotKey(hwnd, kHotkeyId, (UINT)app.set.hotkeyMod | MOD_NOREPEAT, app.set.hotkeyVk)) {
                MessageBoxW(hwnd,
                            L"Не удалось зарегистрировать горячую клавишу — она занята другим приложением.",
                            L"AutoCliker-Siris", MB_OK | MB_ICONWARNING);
            }
            SetTimer(hwnd, kTimerId, 100, nullptr);
            return 0;
        }

        case WM_PAINT:
            Paint();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_TIMER:
            if (wParam == kTimerId && app.running) {
                app.pulse ^= 1;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (wParam == kPickTimerId && app.picking) {
                POINT c;
                GetCursorPos(&c);
                if (app.hint)
                    MoveWindow(app.hint, c.x + 24, c.y + 24, 300, 40, TRUE);
            }
            return 0;

        case WM_APP_POS:
            EndPick((int)wParam, (int)lParam);
            return 0;

        case WM_APP_POS_CANCEL:
            EndPick(-1, -1);
            return 0;

        case WM_HOTKEY:
            if (!app.capturing)
                Toggle();
            return 0;

        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE) {
                ApplyEditsToSettings();
                SaveSettings(app.set);
            }
            return 0;

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, theme::kText);
            SetBkColor(hdc, theme::kBg);
            return (LRESULT)app.brBg;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            HWND c = (HWND)lParam;
            bool en = IsWindowEnabled(c);
            if ((c == app.edRandMin || c == app.edRandMax) && !app.set.randomInterval)
                en = false;
            if (c == app.edCount && !app.set.fixedCount)
                en = false;
            SetTextColor(hdc, en ? theme::kText : theme::kTextDim);
            SetBkColor(hdc, theme::kBgField);
            return (LRESULT)app.brField;
        }

        case WM_GETMINMAXINFO: {
            auto* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize = { kWndW, kWndH };
            mmi->ptMaxTrackSize = { kWndW, kWndH };
            return 0;
        }

        case WM_NCHITTEST: {
            LRESULT r = DefWindowProcW(hwnd, WM_NCHITTEST, wParam, lParam);
            POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &p);
            if (p.y >= 0 && p.y < theme::kTitleH) {
                if (PtIn(app.rcMinBtn, p) || PtIn(app.rcCloseBtn, p))
                    return HTCLIENT;
                return HTCAPTION;
            }
            return r;
        }

        case WM_SETCURSOR: {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);
            bool hot = PtIn(app.rcCk, p) || PtIn(app.rcPickBtn, p) || PtIn(app.rcFix, p) ||
                       PtIn(app.rcBtnStart, p) || PtIn(app.rcBtnStop, p) ||
                       PtIn(app.rcMinBtn, p) || PtIn(app.rcCloseBtn, p);
            for (const RECT& r : app.rcBtn) hot = hot || PtIn(r, p);
            for (const RECT& r : app.rcType) hot = hot || PtIn(r, p);
            for (const RECT& r : app.rcMode) hot = hot || PtIn(r, p);
            if (hot) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_MOUSEMOVE: {
            POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);

            const bool hMin = PtIn(app.rcMinBtn, p);
            const bool hClose = PtIn(app.rcCloseBtn, p);
            const bool hStart = PtIn(app.rcBtnStart, p);
            const bool hStop = PtIn(app.rcBtnStop, p);
            const bool hPick = PtIn(app.rcPickBtn, p);

            int row = 0;
            if (PtIn(app.rcCk, p))
                row = 1;
            else if (PtIn(app.rcFix, p))
                row = 5;
            else if (PtIn(app.rcMode[0], p) || PtIn(app.rcMode[1], p) || PtIn(app.rcMode[2], p))
                row = 4;
            else if (PtIn(app.rcType[0], p) || PtIn(app.rcType[1], p) || PtIn(app.rcType[2], p) || PtIn(app.rcType[3], p))
                row = 3;
            else if (PtIn(app.rcBtn[0], p) || PtIn(app.rcBtn[1], p) || PtIn(app.rcBtn[2], p))
                row = 2;

            bool changed = row != app.hoverRow || hMin != app.hoverMin || hClose != app.hoverClose ||
                           hStart != app.hoverStart || hStop != app.hoverStop || hPick != app.hoverPick;
            app.hoverRow = row;
            app.hoverMin = hMin;
            app.hoverClose = hClose;
            app.hoverStart = hStart;
            app.hoverStop = hStop;
            app.hoverPick = hPick;

            if (changed)
                InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSELEAVE:
            app.hoverRow = 0;
            app.hoverMin = app.hoverClose = app.hoverStart = app.hoverStop = app.hoverPick = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONDOWN: {
            POINT p{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtIn(app.rcMinBtn, p)) {
                ShowWindow(hwnd, SW_MINIMIZE);
                return 0;
            }
            if (PtIn(app.rcCloseBtn, p)) {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (PtIn(app.rcCk, p)) {
                app.set.randomInterval = !app.set.randomInterval;
                EnableWindow(app.edRandMin, app.set.randomInterval);
                EnableWindow(app.edRandMax, app.set.randomInterval);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            for (int i = 0; i < 3; ++i) {
                if (PtIn(app.rcBtn[i], p)) {
                    app.set.button = i;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            for (int i = 0; i < 4; ++i) {
                if (PtIn(app.rcType[i], p)) {
                    app.set.clickType = i;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            if (PtIn(app.rcFix, p)) {
                app.set.fixedPos = !app.set.fixedPos;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PtIn(app.rcPickBtn, p)) {
                StartPick();
                return 0;
            }
            if (PtIn(app.rcMode[0], p)) {
                app.set.fixedCount = false;
                app.set.runSeconds = 0;
                EnableWindow(app.edCount, FALSE);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PtIn(app.rcMode[1], p)) {
                app.set.fixedCount = true;
                app.set.runSeconds = 0;
                EnableWindow(app.edCount, TRUE);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PtIn(app.rcMode[2], p)) {
                app.set.fixedCount = false;
                app.set.runSeconds = std::max(ReadEditInt(app.edTime, 0), 1);
                EnableWindow(app.edCount, FALSE);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PtIn(app.rcBtnStart, p)) {
                Toggle();
                return 0;
            }
            if (PtIn(app.rcBtnStop, p) && app.running) {
                Toggle();
                return 0;
            }
            return 0;
        }

        case WM_CLOSE:
            app.clicker.Stop();
            SaveSettings(app.set);
            if (app.hook) {
                UnhookWindowsHookEx(app.hook);
                app.hook = nullptr;
            }
            DestroyHint();
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, kTimerId);
            KillTimer(hwnd, kPickTimerId);
            UnregisterHotKey(hwnd, kHotkeyId);
            DeleteObject(app.hFontBase);
            DeleteObject(app.hFontBold);
            delete app.fBase;
            delete app.fBold;
            delete app.fSmall;
            delete app.fTitle;
            DeleteObject(app.brBg);
            DeleteObject(app.brField);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int RunApp(HINSTANCE inst) {
    app.inst = inst;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(101));
    wc.hIconSm = LoadIconW(inst, MAKEINTRESOURCEW(101));
    wc.lpszClassName = L"AutoClikerSirisWnd";
    if (!RegisterClassExW(&wc))
        return 1;

    WNDCLASSEXW hc{};
    hc.cbSize = sizeof(hc);
    hc.style = CS_DROPSHADOW;
    hc.lpfnWndProc = HintProc;
    hc.hInstance = inst;
    hc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    hc.lpszClassName = L"AutoClikerSirisHint";
    RegisterClassExW(&hc);

    RECT wr = { 0, 0, kWndW, kWndH };
    AdjustWindowRectEx(&wr, WS_POPUP, FALSE, WS_EX_APPWINDOW);
    const int w = wr.right - wr.left;
    const int h = wr.bottom - wr.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    app.hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"AutoCliker-Siris",
                               WS_POPUP | WS_VISIBLE, x, y, w, h, nullptr, nullptr, inst, nullptr);
    if (!app.hwnd)
        return 1;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}