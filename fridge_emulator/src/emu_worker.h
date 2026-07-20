#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_set>

#include <fridge.h>
#include "fridgemulib.h"

// Worker thread driving FRIDGE_sys_tick at a throttled target frequency.
// Ported from fridge_opengl_emulator/EmulatorThread.cpp — same tick series
// + system-timer-interval (= targetFrequency/256) + Panic handling logic,
// just swapped Qt primitives for std primitives.
//
// All public methods are safe to call from the main thread. While the
// worker holds LoopMutex via Lock(), it is paused at the top of its loop
// iteration (or just before/after a tick batch), so callers can read
// FRIDGE_SYSTEM state without races.

class EmuWorker {
public:
    explicit EmuWorker(FRIDGE_SYSTEM* sys);
    ~EmuWorker();

    EmuWorker(const EmuWorker&) = delete;
    EmuWorker& operator=(const EmuWorker&) = delete;

    // Lifecycle.
    void Start();
    void Stop();

    // Run / pause the worker. When inactive, the loop idles without ticking.
    void SetActive(bool active);
    bool IsActive() const { return active_.load(std::memory_order_relaxed); }

    // Throttle parameters. targetFrequency is in Hz; tickSeriesLength is the
    // number of ticks to run per lock hold. See EmulatorThread::run().
    void SetTargetFrequency(int targetFrequency, int tickSeriesLength);

    // Acquire the loop mutex; blocks the worker between batches. Pair with Unlock().
    void Lock();
    void Unlock();

    // Single-step the CPU exactly one instruction. Caller must already hold Lock()
    // and the worker must be inactive. Triggers system timer if due.
    // Returns true on success, false if the CPU panicked.
    bool StepOnce();

    double MeasuredFrequency() const { return measured_freq_.load(std::memory_order_relaxed); }

    void SetFramePresentedCallback(std::function<void()> cb)  { on_frame_  = std::move(cb); }
    void SetPanicCallback(std::function<void()> cb)            { on_panic_  = std::move(cb); }

    void SetBreakpoints(const std::unordered_set<FRIDGE_RAM_ADDR>* bps) { breakpoints_ = bps; }

private:
    void RunLoop();

    FRIDGE_SYSTEM* sys_;

    std::thread             thread_;
    std::mutex              loop_mutex_;
    std::condition_variable cv_;
    std::atomic<bool>       stop_   {false};
    std::atomic<bool>       active_ {false};

    int target_frequency_   = 1000000; // 1 MHz default
    int tick_series_length_ = 10000;
    int sys_timer_interval_ = target_frequency_ / 256;
    int sys_timer_counter_  = 0;

    std::atomic<double> measured_freq_ {0.0};
    long long active_ticks_counter_ = 0;

    // Timing baseline (chronosteady_clock microseconds).
    long long tick_timer_us_   = 0;
    long long active_timer_us_ = 0;

    std::function<void()> on_frame_;
    std::function<void()> on_panic_;
    const std::unordered_set<FRIDGE_RAM_ADDR>* breakpoints_ = nullptr;
};