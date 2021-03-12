#ifndef EMULATORTHREAD_H
#define EMULATORTHREAD_H

#include <QObject>
#include <QThread>
extern "C"
{
#include <fridgemulib.h>
}
#include <QMutex>
#include <QElapsedTimer>

class EmulatorThread : public QThread
{
    Q_OBJECT

    void run() override;

    FRIDGE_SYSTEM* sys;
    int targetFrequency;
    int tickSeriesLength;
    int sysTimerInterval;
    int sysTimerTicksCounter;
    QMutex* tickMutex;
    QElapsedTimer* tickTimer;
    QElapsedTimer* activeTimer;
    bool isLocked;
    bool isActive;
    bool isStopped;
    double measuredFreq;
    qint64 activeTicksCounter;

public:
    EmulatorThread(FRIDGE_SYSTEM* sys, int targetFrequency, int tickSeriesLength);

    void SetTargetFrequncy(int targetFrequency, int tickSeriesLength = -1);
    void SetActive(bool active);
    bool IsActive() {return isActive;}
    void Stop();
    void SetLock();
    void ReleaseLock();
    qint64 MeasuredFrequency() {return measuredFreq;}

    ~EmulatorThread();

signals:
    void framePresented();
    void corePanic();
};

#endif // EMULATORTHREAD_H
