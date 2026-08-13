#pragma once
#include <atomic>
#include <thread>
#include "settings.h"

class Clicker {
public:
    Clicker() = default;
    ~Clicker();
    Clicker(const Clicker&) = delete;
    Clicker& operator=(const Clicker&) = delete;

    void Start(const ClickSettings& s);
    void Stop();
    bool Running() const { return running_.load(); }
    long long Clicks() const { return clicks_.load(); }
    void ResetClicks();

private:
    void Run();

    std::atomic<bool> running_{false};
    std::atomic<long long> clicks_{0};
    ClickSettings s_;
    std::thread thread_;
};
