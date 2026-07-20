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
    tick_timer_us_   = NowUs();
    active_timer_us_ = NowUs();
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
    active_.store(active, std::memory_order_relaxed);
    if (active)
    {
        tick_timer_us_   = NowUs();
        active_timer_us_ = NowUs();
        active_ticks_counter_ = 0;
        sys_timer_counter_ = 0;
    }
    cv_.notify_all();
}

void EmuWorker::SetTargetFrequency(int targetFrequency, int tickSeriesLength)
{
    std::lock_guard<std::mutex> lk(loop_mutex_);
    target_frequency_   = targetFrequency;
    if (tickSeriesLength > 0)
        tick_series_length_ = tickSeriesLength;
    sys_timer_interval_ = (target_frequency_ > 256) ? (target_frequency_ / 256) : 1;
    sys_timer_counter_  = 0;
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

    if (++sys_timer_counter_ >= sys_timer_interval_)
    {
        FRIDGE_sys_timer_tick(sys_);
        sys_timer_counter_ = 0;
        if (FRIDGE_cpu_flag_PANIC(sys_->cpu))
        {
            active_.store(false, std::memory_order_relaxed);
            if (on_panic_) on_panic_();
            return false;
        }
    }
    return true;
}

void EmuWorker::RunLoop()
{
    tick_timer_us_   = NowUs();
    active_timer_us_ = NowUs();

    while (!stop_.load(std::memory_order_relaxed))
    {
        bool active = active_.load(std::memory_order_relaxed);
        if (!active || !thread_.joinable())
        {
            // Park until woken. Use sleep fallback to keep polls cheap.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        long long now          = NowUs();
        long long elapsed_us   = now - tick_timer_us_;
        long long target_us    = 1000000LL * tick_series_length_ / target_frequency_;
        if (elapsed_us < target_us)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(target_us - elapsed_us));
            now        = NowUs();
            elapsed_us = now - tick_timer_us_;
        }

        long long series_count = elapsed_us * tick_series_length_ /
                                  (1000000LL * tick_series_length_);
        if (series_count <= 0)
            series_count = 1;

        bool panicked = false;

        {
            std::lock_guard<std::mutex> lk(loop_mutex_);
            tick_timer_us_ = NowUs();

            for (long long si = 0; si < series_count; ++si)
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

                    if (++sys_timer_counter_ >= sys_timer_interval_)
                    {
                        FRIDGE_sys_timer_tick(sys_);
                        sys_timer_counter_ = 0;
                        if (FRIDGE_cpu_flag_PANIC(sys_->cpu))
                        {
                            active_.store(false, std::memory_order_relaxed);
                            panicked = true;
                            break;
                        }
                    }
                }
                if (panicked) break;
                active_ticks_counter_ += tick_series_length_;
            }

            long long active_elapsed = NowUs() - active_timer_us_;
            if (active_elapsed > 0)
                measured_freq_.store(1000000.0 * (double)active_ticks_counter_ / (double)active_elapsed,
                                   std::memory_order_relaxed);
        }

        if (panicked)
        {
            if (on_panic_) on_panic_();
        }
        if (on_frame_) on_frame_();
    }
}