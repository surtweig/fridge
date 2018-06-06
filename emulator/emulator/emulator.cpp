#include "stdafx.h"
#include "d3dwindow.h"
#include <iostream>
#include "XCM2System.h"
#include "../x2al/XCM2AssemblyLanguageCompiler.cpp"

#define XCME_VERSION_STR "v1.0"
#define XCME_CONSOLE_TITLE L"XCM2 Fridge - emulator console"
#define XCME_VIDEO_TITLE L"XCM2 Fridge - video output"

void loadTestProgram(XCM2System &sys)
{
	XCM2_WORD program[] = {
		VMODE, 1,
		VFCLR, 
		LXI_HL, 0, 0,
		MVI_A, 'A',
		VFSA,

		MOV_AL,
		RAL,
		MOV_LA,
		MOV_AH,
		RAL,
		MOV_HA,

		//LXI_HL, 0, 10,
		INX_HL,
		INX_HL,
		MVI_A, 1,
		VFSAC,
		INX_HL,
		MVI_A, 15,
		VFSAC,

		VPRE,
	};
	sys.RAM()->readSnapshot(program, ARRAYSIZE(program));
}

int _tmain(int argc, _TCHAR* argv[])
{
	SetConsoleTitle(XCME_CONSOLE_TITLE);
	std::cout << "XCM2 Fridge Emulator " << XCME_VERSION_STR << "\n"
		      << "Copyright (c) Plus&Minus Inc. 2008\n\n"
		      << "Starting up directX video output...\n" << std::flush;

	d3dwindow wnd(960, 640, 240, 160, XCME_VIDEO_TITLE);
	//wnd.switchMode(XCM2_VIDEO_TEXT);

	XCM2System sys(1000000, &wnd);
    /*coutStreamWrapper swcout(&std::cout);
    XCM2AssemblyLanguageCompiler x2al("", "testboot.x2al", "testboot.bin", &swcout);
    ofstream fcomp;
    coutStreamWrapper swfcomp(&fcomp);
    fcomp.open("compiled.txt");
    x2al.printCompiled(&swfcomp);
    fcomp.close();*/

    //sys.RAM()->readSnapshot("..\\..\\xcmcode\\test.bin");
    sys.RAM()->readSnapshot("boot.bin");
    //sys.ROM()->loadSnapshot("rom.rom");
	//loadTestProgram(sys);

#ifdef XCM2CPU_DEBUG
    ofstream fdebug;
    fdebug.open("debug.log");
    fdebug << "\nIR\tPC\tSP\tA\tB\tC\tD\tE\tH\tL\tcr\tsg\tzr\n\n";
#endif
    wstringstream wndcaption;
	while (wnd.update())
	{
		sys.update();

        wndcaption.str(L"");
        wndcaption.clear();
        wndcaption << XCME_VIDEO_TITLE << L" [cpu:";
        wndcaption << sys.CurrentCPUFrequency() / 1000 << L" kHz] ";
        if (sys.CPU()->Halted())
            wndcaption << L"[halted]";
        wnd.setWindowTitle(wndcaption.str().c_str());
#ifdef XCM2CPU_DEBUG
        for (int i = 0; i < XCM2_SYS_MAXCYCLESPERUPDATE; i++)
        {
            XCM2CPU_DebugFrame f = sys.debug()[i];
            if (f.ircode == NOP)
                break;
            fdebug << XCM2AssemblyLanguageCompiler::GetIRName(f.ircode) << "\t" << f.PC << "\t" << f.SP << "\t" 
                   << (int)f.rA << "\t" << (int)f.rB << "\t" << (int)f.rC << "\t" << (int)f.rD << "\t" << (int)f.rE << "\t" << (int)f.rH << "\t" << (int)f.rL << "\t"
                   << f.fCarry << "\t" << f.fSign << "\t" << f.fZero << "\n";
        }
#endif
	}

#ifdef XCM2CPU_DEBUG
    fdebug.close();
#endif

	return 0;
}

