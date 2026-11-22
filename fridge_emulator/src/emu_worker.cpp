#include "emu_worker.h"

#include <chrono>

static long long NowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

EmuWorker::EmuWorker(FRIDGE_SYSTEM* sys)
    : sys_(sys)
{
}

EmuWorker::~EmuWorker()
{
    Stop();
    if (thread_.joinable())
        thread_.join();
}

void EmuWorker::Start()
{
    if (thread_.joinable())
    {
        stop_.store(false, std::memory_order_relaxed);
        cv_.notify_all();
        return;
    }
    stop_.store(false, std::memory_order_relaxed);
    thread_ = std::thread(&EmuWorker::RunLoop, this);
}

void EmuWorker::Stop()
{
    stop_.store(true, std::memory_order_relaxed);
    active_.store(false, std::memory_order_relaxed);
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
}

void EmuWorker::SetActive(bool active)
{
    if (active)
        restart_timing_.store(true);
    active_.store(active);
    cv_.notify_all();
}

void EmuWorker::SetTargetFrequency(int targetFrequency, int tickSeriesLength)
{
    std::lock_guard<std::mutex> lk(loop_mutex_);
    target_frequency_   = targetFrequency;
    if (tickSeriesLength > 0)
        tick_series_length_ = tickSeriesLength;
}

void EmuWorker::Lock()
{
    loop_mutex_.lock();
}

void EmuWorker::Unlock()
{
    loop_mutex_.unlock();
}

bool EmuWorker::StepOnce()
{
    if (breakpoints_ && breakpoints_->count(sys_->cpu->PC))
    {
        active_.store(false, std::memory_order_relaxed);
        return true;
    }

    FRIDGE_sys_tick(sys_);

    if (FRIDGE_cpu_flag_PANIC(sys_->cpu))
    {
        active_.store(false, std::memory_order_relaxed);
        if (on_panic_) on_panic_();
        return false;
    }

    return true;
}

void EmuWorker::RunLoop()
{
    long long tick_timer_us = NowUs();
    long long active_timer_us = tick_timer_us;
    long long active_ticks_counter = 0;
    long long timer_remainder_us = 0;

    while (!stop_.load(std::memory_order_relaxed))
    {
        if (!active_.load())
        {
            // Park until woken. Use sleep fallback to keep polls cheap.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        if (restart_timing_.exchange(false))
        {
            tick_timer_us = NowUs();
            active_timer_us = tick_timer_us;
            active_ticks_counter = 0;
            timer_remainder_us = 0;
        }

        long long target_us;
        {
            std::lock_guard<std::mutex> lk(loop_mutex_);
            target_us = 1000000LL * tick_series_length_ / target_frequency_;
        }
        const long long elapsed_us = NowUs() - tick_timer_us;
        if (elapsed_us < target_us)
            std::this_thread::sleep_for(std::chrono::microseconds(target_us - elapsed_us));

        bool panicked = false;

        {
            std::lock_guard<std::mutex> lk(loop_mutex_);
            if (stop_.load(std::memory_order_relaxed) || !active_.load() || restart_timing_.load())
                continue;

            const long long now = NowUs();
            const long long batch_elapsed_us = now - tick_timer_us;
            tick_timer_us = now;
            const long long timer_elapsed_us = batch_elapsed_us + timer_remainder_us;
            FRIDGE_sys_timer_tick(sys_, static_cast<FRIDGE_SIZE_T>(timer_elapsed_us / 1000));
            timer_remainder_us = timer_elapsed_us % 1000;

            const long long series_count = batch_elapsed_us < 1000000 ? 1 : batch_elapsed_us / 1000000;
            panicked = FRIDGE_cpu_flag_PANIC(sys_->cpu);
            if (panicked)
                active_.store(false, std::memory_order_relaxed);
            for (long long si = 0; si < series_count && !panicked; ++si)
            {
                for (int i = 0; i < tick_series_length_; ++i)
                {
                    if (breakpoints_ && breakpoints_->count(sys_->cpu->PC))
                    {
                        active_.store(false, std::memory_order_relaxed);
                        panicked = true;
                        break;
                    }

                    FRIDGE_sys_tick(sys_);

                    if (FRIDGE_cpu_flag_PANIC(sys_->cpu))
                    {
                        active_.store(false, std::memory_order_relaxed);
                        panicked = true;
                        break;
                    }
                }
                if (panicked) break;
                active_ticks_counter += tick_series_length_;
            }

            long long active_elapsed = NowUs() - active_timer_us;
            if (active_elapsed > 0)
                measured_freq_.store(1000000.0 * (double)active_ticks_counter / (double)active_elapsed,
                                   std::memory_order_relaxed);
        }

        if (panicked)
        {
            if (on_panic_) on_panic_();
        }
        if (on_frame_) on_frame_();
    }
}
