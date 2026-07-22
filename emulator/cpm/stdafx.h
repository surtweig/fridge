// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>

#ifdef _WIN32
#include <tchar.h>
#else
// Linux compatibility: TCHAR macros are not available.
// Source files using _tmain / _TCHAR should define these as fallbacks.
#ifndef _TCHAR
#define _TCHAR char
#endif
#ifndef _tmain
#define _tmain main
#endif
#endif


// TODO: reference additional headers your program requires here
