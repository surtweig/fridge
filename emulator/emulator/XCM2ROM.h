#pragma once
#include "XCM2.h"
#include "XCM2IRCodes.h"

const XCM2_WORD XCM2ROM_MODE_STORE = 1;
const XCM2_WORD XCM2ROM_MODE_LOAD = 2;

typedef enum XCM2ROM_STATE
{
	XCM2ROM_MODE,
	XCM2ROM_SEGLOW,
	XCM2ROM_SEGHIGH,
	XCM2ROM_OPERATE,
	XCM2ROM_STREAMING,
} XCM2ROM_STATE;

class XCM2ROM : public IIODevice, public ITickDevice
{
private:
	XCM2_ROM_SEGMENT* data;
	int segmentsNumber;
	XCM2ROM_STATE state;
	XCM2ROM_STATE nextstate;
	XCM2_WORD mode;
	XCM2_ROM_ADDR activeSegment;
	int streamPos;
	IInterruptionListener* listener;

	void reset();
	void generateInterrupt();

public:
	XCM2ROM(IInterruptionListener* listener);
	inline int SegmentsNumber() { return segmentsNumber; };

	void loadSnapshot(const char* filename);
	void saveSnapshot(const char* filename);

	virtual void in_contact_IN(XCM2_WORD data) override;
	virtual XCM2_WORD out_contact_OUT() override;

	void in_contact_TICK() override;

	~XCM2ROM();
};

class XCM2ROMResetInterface : public IIODevice
{
private:
	XCM2ROM* rom;
public:
	XCM2ROMResetInterface(XCM2ROM* rom) { this->rom = rom; }
	virtual void in_contact_IN(XCM2_WORD data) override { if (data > 0) rom->in_contact_RESET(); }
};