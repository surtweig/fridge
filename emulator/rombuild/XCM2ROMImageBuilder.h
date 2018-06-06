#pragma once

#include "../emulator/XCM2.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "Windows.h"

using namespace std;

const int XCM2_ROM_TOC_SIZE = 1*XCM2_ROM_SEGMENT_SIZE;
const int XCM2_ROM_MAX_SIZE = XCM2_ROM_SEGMENT_SIZE*XCM2_ROM_MAX_SEGMENTS;
const int XCM2_ROM_MAX_SECTIONS = XCM2_ROM_TOC_SIZE / 2 - 1;

bool XCM2ROMImageBuilder(vector<string> sectionFiles, string outputFile, ITextStreamWrapper* errstr);

