#include "stdafx.h"
#include <iostream>
#include <string>
#include <windows.h>
#include "Shlwapi.h"
#include "XCM2ROMImageBuilder.h"

#define XCME_VERSION_STR "v1.0"
#define XCME_CONSOLE_TITLE L"XCM2 ROM Image Builder"

int _tmain(int argc, _TCHAR* argv[])
{
    SetConsoleTitle(XCME_CONSOLE_TITLE);
    std::cout << "XCM2 ROM Image Builder " << XCME_VERSION_STR << "\n"
              << "Copyright (c) Plus&Minus Inc. 2008\n\n";

	return 0;
}

