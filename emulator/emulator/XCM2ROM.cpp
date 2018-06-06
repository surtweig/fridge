#include "stdafx.h"
#include <stdlib.h>
#include "XCM2ROM.h"
#include <iostream>
#include <fstream>
using namespace std;


XCM2ROM::XCM2ROM(IInterruptionListener* listener)
{
	//this->segmentsNumber = segmentsNumber;
	//data = (XCM2_ROM_SEGMENT*)malloc(sizeof(XCM2_ROM_SEGMENT)*segmentsNumber);
	data = nullptr;

	this->listener = listener;

	reset();
}

void XCM2ROM::loadSnapshot(const char* filename)
{
	//memcpy(data, source, sizeof(XCM2_ROM_SEGMENT)*segmentsNumber);
	ifstream f;
	streampos size;
	f.open(filename, ios::in | ios::binary | ios::ate);
	if (f.is_open())
	{
		size = f.tellg();
		data = (XCM2_ROM_SEGMENT*)malloc(size);
		f.seekg(0, ios::beg);
		f.read((char*)data, size);
		f.close();
		segmentsNumber = size / sizeof(XCM2_ROM_SEGMENT);
	}
}

void XCM2ROM::saveSnapshot(const char* filename)
{
	//memcpy(dest, data, sizeof(XCM2_ROM_SEGMENT)*segmentsNumber);
}

void XCM2ROM::reset()
{
	in_data_RESET = false;
	state = XCM2ROM_MODE;
	nextstate = XCM2ROM_MODE;
	mode = 0;
	activeSegment = 0;
	streamPos = 0;
}

void XCM2ROM::generateInterrupt()
{
	listener->in_contact_INTERRUPT({NOP, 0, 0});
}

void XCM2ROM::in_contact_IN(XCM2_WORD indata)
{
	IIODevice::in_contact_IN(indata);
	
	if (state == XCM2ROM_MODE)
	{
		if (in_data_IN == XCM2ROM_MODE_STORE || in_data_IN == XCM2ROM_MODE_LOAD)
		{
			mode = in_data_IN;
			nextstate = XCM2ROM_SEGHIGH;
		}
	}

	else if (state == XCM2ROM_SEGHIGH)
	{
		activeSegment = in_data_IN << XCM2_WORD_BITS;
		nextstate = XCM2ROM_SEGLOW;
	}

	else if (state == XCM2ROM_SEGLOW)
	{
		activeSegment |= (XCM2_ROM_ADDR)in_data_IN;
		if (mode == XCM2ROM_MODE_LOAD)
			nextstate = XCM2ROM_OPERATE;
		else if (mode == XCM2ROM_MODE_STORE)
			nextstate = XCM2ROM_STREAMING;
	}

	else if (state == XCM2ROM_STREAMING)
	{
		if (mode == XCM2ROM_MODE_STORE)
		{
			if (data)
				data[activeSegment].segment[streamPos] = in_data_IN;
			streamPos++;
			if (streamPos == XCM2_ROM_SEGMENT_SIZE)
			{
				streamPos = 0;
				nextstate = XCM2ROM_OPERATE;
			}
		}
	}
}

XCM2_WORD XCM2ROM::out_contact_OUT()
{
	if (state == XCM2ROM_STREAMING)
	{
		if (mode == XCM2ROM_MODE_LOAD)
		{
			if (data)
				out_data_OUT = data[activeSegment].segment[streamPos];
			//generateInterrupt();
			streamPos++;
			if (streamPos == XCM2_ROM_SEGMENT_SIZE)
			{
				streamPos = 0;
				nextstate = XCM2ROM_MODE;
			}
		}
	}
	return out_data_OUT;
}

void XCM2ROM::in_contact_TICK()
{
	if (in_data_RESET)
		reset();

	if (state == XCM2ROM_OPERATE)
	{
		nextstate = XCM2ROM_STREAMING;
		generateInterrupt();
	}

	state = nextstate;
}


XCM2ROM::~XCM2ROM()
{
	free(data);
}
