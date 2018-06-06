#pragma once
#include "XCM2.h"
#include "XCM2RAM.h"
#include "XCM2ROM.h"
#include "XCM2CPU.h"
#include "d3dwindow.h"
#include <WinBase.h>
#include <iostream>

#define XCM2_SYS_MAXCYCLESPERUPDATE 2000

#define XCM2_SYS_TIMER_INTERVAL_MCS 3906 // System timer emits an IRQ every 1/256th of a second or every 3906.25 microseconds
#define XCM2_IRQ_SYS_TIMER (XCM2_RAM_ADDR)0x0004

class XCM2System
{
public:
	XCM2System(LONGLONG targetFrequency, d3dwindow* gpu);
    inline XCM2CPU* CPU() { return cpu; }
	inline XCM2ROM* ROM() { return rom; }
	inline XCM2RAM* RAM() { return ram; }
    inline XCM2CPU_DebugFrame* debug() { return debugFrames; }
    inline LONGLONG CurrentCPUFrequency() { return currentFrequency; }
	void reset();
	void update();

	~XCM2System();

private:
    int debugFramesCounter;
    XCM2CPU_DebugFrame debugFrames[XCM2_SYS_MAXCYCLESPERUPDATE+1];
	XCM2CPU* cpu;
	XCM2RAM* ram;
	XCM2ROM* rom;
	XCM2ROMResetInterface* rom_reset;
	d3dwindow* gpu;

    LONGLONG currentFrequency;
	LONGLONG guestTicksCounter;
	LONGLONG hostToGuestFreqRatio;
	LONGLONG hostCycleCounter;
    LONGLONG guestCycleCounter;
    LONGLONG elapsedGuestTicksPerSecond;
    LONGLONG oneSecondTimer;
    LONGLONG sysTimerTickTimer;
	LARGE_INTEGER hostPrevTickTimeStamp;
	LARGE_INTEGER hostFrequency;
};

