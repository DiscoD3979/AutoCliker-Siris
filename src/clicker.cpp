#include "clicker.h"
#include <windows.h>
#include <mmsystem.h>
#include <random>

namespace {

DWORD DownFlag(int button) {
    switch (button) {
        case 1: return MOUSEEVENTF_RIGHTDOWN;
        case 2: return MOUSEEVENTF_MIDDLEDOWN;
        default: return MOUSEEVENTF_LEFTDOWN;
    }
}

DWORD UpFlag(int button) {
    switch (button) {
        case 1: return MOUSEEVENTF_RIGHTUP;
        case 2: return MOUSEEVENTF_MIDDLEUP;
        default: return MOUSEEVENTF_LEFTUP;
    }
}

void ClickOnce(int button) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = DownFlag(button);
    SendInput(1, &in, sizeof(INPUT));
    in.mi.dwFlags = UpFlag(button);
    SendInput(1, &in, sizeof(INPUT));
}

void Press(int button, bool down) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = down ? DownFlag(button) : UpFlag(button);
    SendInput(1, &in, sizeof(INPUT));
}

double NowMs() {
    static LARGE_INTEGER freq{};
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return c.QuadPart * 1000.0 / freq.QuadPart;
}

} // namespace

Clicker::~Clicker() { Stop(); }

void Clicker::Start(const ClickSettings& s) {
    if (running_.load())
        return;
    s_ = s;
    clicks_.store(0);
    running_.store(true);
    thread_ = std::thread([this] { Run(); });
}

void Clicker::Stop() {
    running_.store(false);
    if (thread_.joinable())
        thread_.join();
}

void Clicker::ResetClicks() { clicks_.store(0); }

void Clicker::Run() {
    timeBeginPeriod(1); // точность сна ~1 мс

    const bool hold = s_.clickType == 3;
    if (hold)
        Press(s_.button, true);

    std::mt19937 rng(std::random_device{}());
    double next = NowMs();
    long long done = 0;

    while (running_.load()) {
        // высокоточное ожидание следующего тика
        double now = NowMs();
        if (now < next) {
            double rem = next - now;
            if (rem > 3.0)
                Sleep((DWORD)(rem - 2.0));
            while (running_.load() && NowMs() < next) { /* спин до микросекундной точности */ }
        } else {
            next = now;
        }

        if (!hold) {
            const int n = s_.clickType == 2 ? 3 : (s_.clickType == 1 ? 2 : 1);
            for (int i = 0; i < n; ++i) {
                ClickOnce(s_.button);
                if (i + 1 < n)
                    Sleep(30);
            }
        }
        clicks_.fetch_add(1);

        if (s_.fixedCount && ++done >= (long long)s_.count)
            break;

        double interval;
        if (s_.randomInterval && s_.maxMs > s_.minMs) {
            const uint32_t span = (uint32_t)(s_.maxMs - s_.minMs + 1);
            interval = (double)(s_.minMs + (int)(rng() % span));
        } else if (s_.randomInterval) {
            interval = (double)s_.minMs;
        } else {
            interval = 1000.0 / (s_.cps > 0.01 ? s_.cps : 0.01);
        }
        next += interval;
    }

    if (hold)
        Press(s_.button, false);
    timeEndPeriod(1);
}
