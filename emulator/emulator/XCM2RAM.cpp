#include "stdafx.h"
#include "XCM2RAM.h"
#include <fstream>
#include <iostream>
using namespace std;

XCM2RAM::XCM2RAM()
{
	clear();
}

void XCM2RAM::clear()
{
	for (int i = 0; i < XCM2_RAM_SIZE; i++)
		data[i] = 0;
}

void XCM2RAM::readSnapshot(XCM2_WORD* snapshot, size_t size)
{
	memcpy(data, snapshot, size);
}

void XCM2RAM::readSnapshot(const char* filename)
{
	ifstream f;
	streampos size;
	f.open(filename, ios::in | ios::binary | ios::ate);
	if (f.is_open())
	{
		size = f.tellg();
		f.seekg(0, ios::beg);
		f.read((char*)data, size);
		f.close();
        cout << "RAM snapshot " << filename << " has been loaded." << std::flush;
	}
    else
        cout << "[ERORR] Cannot load a RAM snapshot from " << filename << std::flush;
}

void XCM2RAM::store(XCM2_RAM_ADDR addr, XCM2_WORD value)
{
	if (addr < XCM2_RAM_SIZE)
		data[addr] = value;
}

void XCM2RAM::store(XCM2_WORD haddr, XCM2_WORD laddr, XCM2_WORD value)
{
	store(XCM2_DWORD_HL(haddr, laddr), value);
}

XCM2_WORD XCM2RAM::load(XCM2_RAM_ADDR addr)
{
	if (addr < XCM2_RAM_SIZE)
		return data[addr];
	return 0;
}

XCM2_WORD XCM2RAM::load(XCM2_WORD haddr, XCM2_WORD laddr)
{
	return load(XCM2_DWORD_HL(haddr, laddr));
}

XCM2RAM::~XCM2RAM()
{
}

void XCM2GRAM::storePixel(XCM2_DWORD position, XCM2_WORD color)
{
	XCM2_RAM_ADDR addr = position / 2;
	XCM2_WORD byte = load(addr);
	if (position % 2 == 0)
		store(addr, (byte & 0x0F) | ((color & 0x0F) << 4));
	else
		store(addr, (byte & 0xF0) | (color & 0x0F));
}

XCM2_WORD XCM2GRAM::loadPixel(XCM2_DWORD position)
{
	XCM2_RAM_ADDR addr = position / 2;
	XCM2_WORD byte = load(addr);
	if (position % 2 == 0)
		return (byte & 0xF0) >> 4;
	else
		return (byte & 0x0F);
}
