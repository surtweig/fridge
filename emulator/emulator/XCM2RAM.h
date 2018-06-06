#pragma once
#include "XCM2.h"

class XCM2RAM
{
private:
	XCM2_WORD data[XCM2_RAM_SIZE];

public:
	XCM2RAM();
	void clear();
	void readSnapshot(XCM2_WORD* snapshot, size_t size);
    void readSnapshot(const char* filename);
	void store(XCM2_RAM_ADDR addr, XCM2_WORD value);
	void store(XCM2_WORD haddr, XCM2_WORD laddr, XCM2_WORD value);
	XCM2_WORD load(XCM2_RAM_ADDR addr);
	XCM2_WORD load(XCM2_WORD haddr, XCM2_WORD laddr);
	~XCM2RAM();
};

class XCM2GRAM : public XCM2RAM
{
public:
	void storePixel(XCM2_DWORD position, XCM2_WORD color);
	XCM2_WORD loadPixel(XCM2_DWORD position);
};

