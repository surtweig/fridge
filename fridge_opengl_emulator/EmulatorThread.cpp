#include "EmulatorThread.h"

EmulatorThread::EmulatorThread(FRIDGE_SYSTEM* sys, int targetFrequency, int tickSeriesLength)
{
    this->sys = sys;
    this->targetFrequency = targetFrequency;
    this->tickSeriesLength = tickSeriesLength;
    tickMutex = new QMutex();
    tickTimer = new QElapsedTimer();
    activeTimer = new QElapsedTimer();
    isLocked = false;
    isStopped = false;
    isActive = false;
    this->measuredFreq = 0.0;
    activeTimer->start();
    activeTicksCounter = 0;
    sysTimerInterval = targetFrequency / 256;
    sysTimerTicksCounter = 0;
}

void EmulatorThread::run()
{
    tickTimer->start();

    while (!isStopped)
    {
        if (!isActive)
        {
            tickTimer->restart();
            activeTimer->restart();
            activeTicksCounter = 0;
            continue;
        }

        bool VPRECalled = false;
        qint64 timeElapsed = tickTimer->elapsed();
        qint64 seriesCount = timeElapsed * targetFrequency / (1000*tickSeriesLength);

        if (seriesCount > 0)
        {
            tickTimer->restart();
            SetLock();
            //for (qint64 si = 0; si < seriesCount; ++si)
            {
                FRIDGE_VIDEO_FRAME visibleFrame = sys->gpu->vframe;
                for (int i = 0; i < tickSeriesLength; ++i)
                {
                    FRIDGE_sys_tick(sys);

                    if (FRIDGE_cpu_flag_PANIC(sys->cpu))
                    {
                        isActive = false;
                        emit corePanic();
                        break;
                    }

                    if (++sysTimerTicksCounter >= sysTimerInterval)
                    {
                        FRIDGE_sys_timer_tick(sys);
                        sysTimerTicksCounter = 0;
                        if (FRIDGE_cpu_flag_PANIC(sys->cpu))
                        {
                            isActive = false;
                            emit corePanic();
                            break;
                        }
                    }

                    /*
                    if (visibleFrame != sys->gpu->vframe || FRIDGE_cpu_flag_PANIC(sys->cpu))
                    {
                        ReleaseLock();
                        VPRECalled = true;
                        emit framePresented();
                        activeTicksCounter += i;
                        break;
                    }
                    */
                }
                if (!VPRECalled)
                    activeTicksCounter += tickSeriesLength;
                //if (VPRECalled)
                //    break;
            }

            //measuredFreq = 1000.0 * (double)tickSeriesLength / (double)(timeElapsed);
            measuredFreq = 1000.0 * (double)activeTicksCounter / (double)(activeTimer->elapsed());

            ReleaseLock();
            if (!VPRECalled)
                emit framePresented();
        }
    }
}

void EmulatorThread::SetLock()
{
    if (!isLocked)
    {
        tickMutex->lock();
        isLocked = true;
    }
}

void EmulatorThread::ReleaseLock()
{
    if (isLocked)
    {
        tickMutex->unlock();
        isLocked = false;
    }
}

void EmulatorThread::SetTargetFrequncy(int targetFrequency, int tickSeriesLength)
{
    SetLock();
    this->targetFrequency = targetFrequency;
    if (tickSeriesLength > 0)
        this->tickSeriesLength = tickSeriesLength;
    sysTimerInterval = targetFrequency / 256;
    sysTimerTicksCounter = 0;
    ReleaseLock();
}

void EmulatorThread::SetActive(bool active)
{
    this->isActive = active;
}

void EmulatorThread::Stop()
{
    isStopped = true;
}

EmulatorThread::~EmulatorThread()
{
    SetLock();
    Stop();
    ReleaseLock();
    delete tickMutex;
    delete tickTimer;
}
