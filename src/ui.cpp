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
constexpr int kWndH = 608;
constexpr UINT_PTR kTimerId = 1;
constexpr int kHotkeyId = 1;

constexpr double kMinCps = 1.0;
constexpr double kMaxCps = 1000.0;
constexpr int kSlideL = 32;
constexpr int kSlideR = 348;

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
    bool hoverMin = false;
    bool hoverClose = false;
    bool hoverStart = false;
    bool hoverStop = false;
    bool hoverSlider = false;
    bool dragging = false;
    int hoverRow = 0;
    int pulse = 0;

    RECT rcSlider{}, rcCk{}, rcEdMin{}, rcEdMax{}, rcEdCount{}, rcEdHot{};
    RECT rcBtn[3]{}, rcType[4]{}, rcMode[2]{};
    RECT rcBtnStart{}, rcBtnStop{}, rcMinBtn{}, rcCloseBtn{};

    HFONT hFontBase = nullptr, hFontBold = nullptr;
    Font* fBase = nullptr;
    Font* fBold = nullptr;
    Font* fSmall = nullptr;
    Font* fTitle = nullptr;
    HBRUSH brBg = nullptr, brField = nullptr;

    HWND edMin = nullptr, edMax = nullptr, edCount = nullptr, edHot = nullptr;
    HWND focusedEdit = nullptr;
} app;

WNDPROC g_origEdit = nullptr;

double SliderToCps(int x) {
    double t = std::clamp((double)(x - kSlideL) / (kSlideR - kSlideL), 0.0, 1.0);
    return std::exp(std::log(kMinCps) + t * (std::log(kMaxCps) - std::log(kMinCps)));
}

int CpsToSliderX(double cps) {
    double t = (std::log(cps) - std::log(kMinCps)) / (std::log(kMaxCps) - std::log(kMinCps));
    t = std::clamp(t, 0.0, 1.0);
    return kSlideL + (int)std::lround(t * (kSlideR - kSlideL));
}

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
    app.rcSlider = { kSlideL, 100, kSlideR, 128 };
    app.rcCk = { 24, 148, 356, 168 };
    app.rcEdMin = { 62, 178, 138, 210 };
    app.rcEdMax = { 186, 178, 262, 210 };
    app.rcBtn[0] = { 24, 248, 116, 272 };
    app.rcBtn[1] = { 120, 248, 212, 272 };
    app.rcBtn[2] = { 216, 248, 308, 272 };
    app.rcType[0] = { 24, 312, 180, 336 };
    app.rcType[1] = { 200, 312, 356, 336 };
    app.rcType[2] = { 24, 338, 180, 362 };
    app.rcType[3] = { 200, 338, 356, 362 };
    app.rcMode[0] = { 24, 400, 150, 424 };
    app.rcMode[1] = { 164, 400, 356, 424 };
    app.rcEdCount = { 276, 400, 356, 424 };
    app.rcEdHot = { 24, 466, 356, 498 };
    app.rcBtnStart = { 24, 552, 182, 596 };
    app.rcBtnStop = { 198, 552, 356, 596 };
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

void ApplyEditsToSettings() {
    wchar_t b[32];
    if (GetWindowTextW(app.edMin, b, 32) > 0)
        app.set.minMs = std::clamp((int)wcstol(b, nullptr, 10), 1, 10000);
    if (GetWindowTextW(app.edMax, b, 32) > 0)
        app.set.maxMs = std::clamp((int)wcstol(b, nullptr, 10), 1, 10000);
    if (app.set.maxMs < app.set.minMs)
        std::swap(app.set.minMs, app.set.maxMs);
    if (GetWindowTextW(app.edCount, b, 32) > 0)
        app.set.count = std::clamp((int)wcstol(b, nullptr, 10), 1, 1000000000);
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

void Header(Graphics& g, const wchar_t* t, int y) {
    SolidBrush tb(Col(theme::kYellow));
    g.DrawString(t, -1, app.fBold, PointF(24.0f, (REAL)y), &tb);
    Pen ln(Col(theme::kBorder));
    g.DrawLine(&ln, 24.0f, (REAL)(y + 18), (REAL)(kWndW - 24), (REAL)(y + 18));
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

    StringFormat sf;
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF m;
    g.MeasureString(L"AutoCliker", -1, app.fTitle, PointF(18.0f, 0.0f), &sf, &m);
    SolidBrush tb(Col(theme::kText));
    g.DrawString(L"AutoCliker", -1, app.fTitle, PointF(18.0f, 0.0f), &sf, &tb);
    g.DrawString(L"Siris", -1, app.fTitle, PointF(18.0f + m.Width, 0.0f), &sf, &yel);

    DrawTitleBtn(g, app.rcMinBtn, L"\u2013", app.hoverMin);
    DrawTitleBtn(g, app.rcCloseBtn, L"\u00D7", app.hoverClose);
}

void PaintSlider(Graphics& g) {
    const int cx = CpsToSliderX(app.set.cps);
    const int cy = (app.rcSlider.top + app.rcSlider.bottom) / 2;

    Pen track(Col(theme::kBorder), 6.0f);
    track.SetLineCap(LineCapRound, LineCapRound, DashCapFlat);
    g.DrawLine(&track, (REAL)app.rcSlider.left, (REAL)cy, (REAL)app.rcSlider.right, (REAL)cy);

    Pen fill(Col(theme::kYellow), 6.0f);
    fill.SetLineCap(LineCapRound, LineCapRound, DashCapFlat);
    g.DrawLine(&fill, (REAL)app.rcSlider.left, (REAL)cy, (REAL)cx, (REAL)cy);

    SolidBrush thumb(app.hoverSlider ? Col(theme::kYellowHover) : Col(theme::kYellow));
    g.FillEllipse(&thumb, (REAL)(cx - 8), (REAL)(cy - 8), 16.0f, 16.0f);
    Pen ring(Col(theme::kBg), 1.5f);
    g.DrawEllipse(&ring, (REAL)(cx - 8), (REAL)(cy - 8), 16.0f, 16.0f);

    SolidBrush sd(Col(theme::kTextDim));
    g.DrawString(L"1", -1, app.fSmall, PointF(24.0f, 132.0f), &sd);
    StringFormat fr;
    fr.SetAlignment(StringAlignmentFar);
    g.DrawString(L"1000", -1, app.fSmall, RectF(24.0f, 132.0f, (REAL)(kWndW - 48), 16.0f), &fr, &sd);
}

void PaintCheck(Graphics& g, const RECT& r, bool hover) {
    if (hover) {
        SolidBrush hb(Col(theme::kBgPanel));
        RoundedRect(g, r, 6, Col(theme::kBgPanel), Color(255, 0, 0, 0), 0);
    }
    int top = r.top + (r.bottom - r.top - 16) / 2;
    RECT box = { r.left, top, r.left + 16, top + 16 };
    RoundedRect(g, box, 4, Col(theme::kBgField),
                app.set.randomInterval ? Col(theme::kYellow) : Col(theme::kBorder), 1);
    if (app.set.randomInterval) {
        Pen ck(Col(theme::kBg), 2.0f);
        g.DrawLine(&ck, (REAL)(box.left + 4), (REAL)(box.top + 8), (REAL)(box.left + 7), (REAL)(box.top + 11));
        g.DrawLine(&ck, (REAL)(box.left + 7), (REAL)(box.top + 11), (REAL)(box.left + 12), (REAL)(box.top + 5));
    }
    SolidBrush lb(Col(theme::kText));
    g.DrawString(L"Случайный интервал (мс)", -1, app.fBase,
                 PointF((REAL)(box.right + 10), (REAL)(r.top + (r.bottom - r.top - 14) / 2)), &lb);
}

void PaintRadio(Graphics& g, const RECT& r, bool on, const wchar_t* label, bool rowHover) {
    if (rowHover) {
        SolidBrush hb(Col(theme::kBgPanel));
        RoundedRect(g, r, 6, Col(theme::kBgPanel), Color(255, 0, 0, 0), 0);
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
    const int y = 526;
    Color dc = app.running ? (app.pulse ? Col(theme::kYellow) : Col(theme::kYellowDim)) : Col(theme::kGreen);
    SolidBrush db(dc);
    g.FillEllipse(&db, 28.0f, (REAL)(y + 6), 10.0f, 10.0f);

    SolidBrush tb(app.running ? Col(theme::kYellow) : Col(theme::kTextDim));
    g.DrawString(app.running ? L"Работает" : L"Остановлен", -1, app.fBold, PointF(46.0f, (REAL)(y + 2)), &tb);

    wchar_t b[64];
    swprintf_s(b, L"Кликов: %lld", (long long)app.clicker.Clicks());
    SolidBrush cb(Col(theme::kTextDim));
    StringFormat fr;
    fr.SetAlignment(StringAlignmentFar);
    g.DrawString(b, -1, app.fBase, RectF(24.0f, (REAL)(y + 2), (REAL)(W - 48), 18.0f), &fr, &cb);
}

void PaintSections(Graphics& g, int W) {
    Header(g, L"НАСТРОЙКИ КЛИКОВ", 56);
    Header(g, L"КНОПКА МЫШИ", 228);
    Header(g, L"ТИП КЛИКА", 290);
    Header(g, L"РЕЖИМ", 380);
    Header(g, L"ГОРЯЧАЯ КЛАВИША", 444);

    SolidBrush lb(Col(theme::kTextDim));
    g.DrawString(L"Скорость (кликов/сек)", -1, app.fBase, PointF(24.0f, 78.0f), &lb);
    wchar_t vb[32];
    swprintf_s(vb, L"%.1f", app.set.cps);
    SolidBrush yb(Col(theme::kYellow));
    StringFormat fr;
    fr.SetAlignment(StringAlignmentFar);
    g.DrawString(vb, -1, app.fBold, RectF(200.0f, 78.0f, 156.0f, 18.0f), &fr, &yb);
    PaintSlider(g);

    PaintCheck(g, app.rcCk, app.hoverRow == 1);

    g.DrawString(L"Мин:", -1, app.fBase, PointF(24.0f, 187.0f), &lb);
    g.DrawString(L"Макс:", -1, app.fBase, PointF(143.0f, 187.0f), &lb);
    g.DrawString(L"мс", -1, app.fSmall, PointF(145.0f, 189.0f), &lb);
    g.DrawString(L"мс", -1, app.fSmall, PointF(269.0f, 189.0f), &lb);
    PaintEditBorder(g, app.rcEdMin, app.focusedEdit == app.edMin);
    PaintEditBorder(g, app.rcEdMax, app.focusedEdit == app.edMax);

    PaintRadio(g, app.rcBtn[0], app.set.button == 0, L"ЛКМ", app.hoverRow == 2);
    PaintRadio(g, app.rcBtn[1], app.set.button == 1, L"ПКМ", app.hoverRow == 2);
    PaintRadio(g, app.rcBtn[2], app.set.button == 2, L"Средняя", app.hoverRow == 2);

    PaintRadio(g, app.rcType[0], app.set.clickType == 0, L"Один", app.hoverRow == 3);
    PaintRadio(g, app.rcType[1], app.set.clickType == 1, L"Двойной", app.hoverRow == 3);
    PaintRadio(g, app.rcType[2], app.set.clickType == 2, L"Тройной", app.hoverRow == 3);
    PaintRadio(g, app.rcType[3], app.set.clickType == 3, L"Удержание", app.hoverRow == 3);

    PaintRadio(g, app.rcMode[0], !app.set.fixedCount, L"Бесконечно", app.hoverRow == 4);
    PaintRadio(g, app.rcMode[1], app.set.fixedCount, L"Количество:", app.hoverRow == 4);
    PaintEditBorder(g, app.rcEdCount, app.focusedEdit == app.edCount);

    PaintEditBorder(g, app.rcEdHot, app.focusedEdit == app.edHot);
    SolidBrush hb(Col(theme::kTextDim));
    g.DrawString(app.capturing ? L"Нажмите клавишу (Esc — отмена)" : L"Горячая клавиша — старт/стоп",
                 -1, app.fSmall, PointF(24.0f, 502.0f), &hb);

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
        gd.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            InitRects();
            LoadSettings(app.set);

            app.hFontBase = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            app.hFontBold = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            FontFamily ff(L"Segoe UI");
            app.fBase = new Font(&ff, 9.0f, FontStyleRegular, UnitPixel);
            app.fBold = new Font(&ff, 9.0f, FontStyleBold, UnitPixel);
            app.fSmall = new Font(&ff, 8.0f, FontStyleRegular, UnitPixel);
            app.fTitle = new Font(&ff, 12.0f, FontStyleBold, UnitPixel);

            app.brBg = CreateSolidBrush(theme::kBg);
            app.brField = CreateSolidBrush(theme::kBgField);

            auto makeEdit = [&](const wchar_t* text, int x, int y, int w, int h, DWORD extra, HMENU id) {
                HWND e = CreateWindowExW(0, L"EDIT", text,
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL | extra,
                                         x, y, w, h, hwnd, id, app.inst, nullptr);
                SendMessageW(e, WM_SETFONT, (WPARAM)app.hFontBase, TRUE);
                return e;
            };

            wchar_t b[32];
            swprintf_s(b, L"%d", app.set.minMs);
            app.edMin = makeEdit(b, 62, 178, 76, 32, ES_NUMBER, (HMENU)1);
            swprintf_s(b, L"%d", app.set.maxMs);
            app.edMax = makeEdit(b, 186, 178, 76, 32, ES_NUMBER, (HMENU)2);
            swprintf_s(b, L"%d", app.set.count);
            app.edCount = makeEdit(b, 276, 400, 80, 32, ES_NUMBER, (HMENU)3);
            app.edHot = makeEdit(L"", 24, 466, 332, 32, ES_CENTER | ES_READONLY, (HMENU)4);

            g_origEdit = (WNDPROC)SetWindowLongPtrW(app.edMin, GWLP_WNDPROC, (LONG_PTR)EditProc);
            SetWindowLongPtrW(app.edMax, GWLP_WNDPROC, (LONG_PTR)EditProc);
            SetWindowLongPtrW(app.edCount, GWLP_WNDPROC, (LONG_PTR)EditProc);
            SetWindowLongPtrW(app.edHot, GWLP_WNDPROC, (LONG_PTR)EditProc);

            EnableWindow(app.edMin, app.set.randomInterval);
            EnableWindow(app.edMax, app.set.randomInterval);
            EnableWindow(app.edCount, app.set.fixedCount);
            UpdateHotkeyText();

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
            }
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
            if ((c == app.edMin || c == app.edMax) && !app.set.randomInterval)
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
            bool hot = PtIn(app.rcSlider, p) || PtIn(app.rcCk, p) || PtIn(app.rcBtnStart, p) ||
                       PtIn(app.rcBtnStop, p) || PtIn(app.rcMinBtn, p) || PtIn(app.rcCloseBtn, p);
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
            const bool hSlide = PtIn(app.rcSlider, p) || app.dragging;

            int row = 0;
            if (PtIn(app.rcCk, p))
                row = 1;
            else if (PtIn(app.rcMode[0], p) || PtIn(app.rcMode[1], p))
                row = 4;
            else if (PtIn(app.rcType[0], p) || PtIn(app.rcType[1], p) || PtIn(app.rcType[2], p) || PtIn(app.rcType[3], p))
                row = 3;
            else if (PtIn(app.rcBtn[0], p) || PtIn(app.rcBtn[1], p) || PtIn(app.rcBtn[2], p))
                row = 2;

            bool changed = row != app.hoverRow || hMin != app.hoverMin || hClose != app.hoverClose ||
                           hStart != app.hoverStart || hStop != app.hoverStop || hSlide != app.hoverSlider;
            app.hoverRow = row;
            app.hoverMin = hMin;
            app.hoverClose = hClose;
            app.hoverStart = hStart;
            app.hoverStop = hStop;
            app.hoverSlider = hSlide;

            if (app.dragging) {
                app.set.cps = SliderToCps(p.x);
                changed = true;
            }
            if (changed)
                InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSELEAVE:
            app.hoverRow = 0;
            app.hoverMin = app.hoverClose = app.hoverStart = app.hoverStop = app.hoverSlider = false;
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
            if (PtIn(app.rcSlider, p)) {
                app.dragging = true;
                SetCapture(hwnd);
                app.set.cps = SliderToCps(p.x);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PtIn(app.rcCk, p)) {
                app.set.randomInterval = !app.set.randomInterval;
                EnableWindow(app.edMin, app.set.randomInterval);
                EnableWindow(app.edMax, app.set.randomInterval);
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
            if (PtIn(app.rcMode[0], p)) {
                app.set.fixedCount = false;
                EnableWindow(app.edCount, FALSE);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PtIn(app.rcMode[1], p)) {
                app.set.fixedCount = true;
                EnableWindow(app.edCount, TRUE);
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

        case WM_LBUTTONUP:
            if (app.dragging) {
                app.dragging = false;
                ReleaseCapture();
                SaveSettings(app.set);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_CLOSE:
            app.clicker.Stop();
            SaveSettings(app.set);
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, kTimerId);
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
    wc.lpszClassName = L"AutoClikerSirisWnd";
    if (!RegisterClassExW(&wc))
        return 1;

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
