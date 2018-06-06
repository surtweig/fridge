#include "stdafx.h"
#include "XCM2System.h"
#include "XCM2IRCodes.h"

XCM2System::XCM2System(LONGLONG targetFrequency, d3dwindow* gpu)
{
	this->gpu = gpu;
	ram = new XCM2RAM();
	cpu = new XCM2CPU(ram, gpu);
	rom = new XCM2ROM(cpu);
	rom_reset = new XCM2ROMResetInterface(rom);

	QueryPerformanceFrequency(&hostFrequency);
	hostToGuestFreqRatio = hostFrequency.QuadPart / targetFrequency;
	if (hostToGuestFreqRatio <= 0)
		hostToGuestFreqRatio = 1;
	hostCycleCounter = 0;
    guestCycleCounter = 0;
	guestTicksCounter = 0;
    oneSecondTimer = 0;
    sysTimerTickTimer = 0;
    elapsedGuestTicksPerSecond = 0;
    currentFrequency = 0;
	QueryPerformanceCounter(&hostPrevTickTimeStamp);
	
	cpu->setDevice(1, rom);
	cpu->setDevice(2, rom_reset);

	reset();

    debugFramesCounter = 0;
}

void XCM2System::reset()
{
	ram->clear();
	rom->in_contact_RESET();
}

void XCM2System::update()
{
	LARGE_INTEGER hostTickCounter;
	QueryPerformanceCounter(&hostTickCounter);
	hostCycleCounter += hostTickCounter.QuadPart - hostPrevTickTimeStamp.QuadPart;

    LONGLONG elapsedMicrosecond;
    elapsedMicrosecond = hostTickCounter.QuadPart - hostPrevTickTimeStamp.QuadPart;
    elapsedMicrosecond *= 1000000;
    elapsedMicrosecond /= hostFrequency.QuadPart;
    oneSecondTimer += elapsedMicrosecond;
    sysTimerTickTimer += elapsedMicrosecond;

    hostPrevTickTimeStamp = hostTickCounter;
    debugFramesCounter = 0;
    
    if (oneSecondTimer >= 1000000)
    {
        currentFrequency = elapsedGuestTicksPerSecond;
        //std::cout << "\nCurrent CPU frequency = " << elapsedGuestTicksPerSecond << std::flush;
        elapsedGuestTicksPerSecond = 0;
        oneSecondTimer = 0;
    }

    if (sysTimerTickTimer >= XCM2_SYS_TIMER_INTERVAL_MCS)
    {
        cpu->in_contact_INTERRUPT({CALL, XCM2_DWORD_TO_WORDS(XCM2_IRQ_SYS_TIMER)});
        sysTimerTickTimer %= XCM2_SYS_TIMER_INTERVAL_MCS;
    }

	if (hostCycleCounter >= hostToGuestFreqRatio)
	{
		LONGLONG cyclesPerUpdate = hostCycleCounter / hostToGuestFreqRatio;
		if (cyclesPerUpdate > XCM2_SYS_MAXCYCLESPERUPDATE)
			cyclesPerUpdate = XCM2_SYS_MAXCYCLESPERUPDATE;
		for (int i = 0; i < cyclesPerUpdate; i++)
		{
			cpu->tick();
#ifdef XCM2CPU_DEBUG
            debugFrames[debugFramesCounter] = cpu->currentDebugFrame;
            debugFramesCounter++;
#endif
		}
        guestCycleCounter += cyclesPerUpdate;
        elapsedGuestTicksPerSecond += cyclesPerUpdate;
		hostCycleCounter %= hostToGuestFreqRatio;
	}

#ifdef XCM2CPU_DEBUG
    debugFrames[debugFramesCounter].ircode = NOP;
#endif


}

XCM2System::~XCM2System()
{
	free(rom);
	free(cpu);
	free(ram);
	free(rom_reset);
}
