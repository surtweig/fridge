#pragma once

#include <string>
#include <ostream>
#include <sstream>

typedef unsigned int   XCM2_UINT;
typedef unsigned char  XCM2_WORD;
typedef unsigned short XCM2_DWORD;
typedef unsigned short XCM2_RAM_ADDR;

#define XCM2_RAM_SIZE 65536 // bytes
#define XCM2_ROM_MAX_SEGMENTS 65536
#define XCM2_ROM_SEGMENT_SIZE 256 // bytes (16Mb maximum)
#define XCM2_WORD_BITS 8 // number of bits in a word
#define XCM2_ROM_ADDR XCM2_RAM_ADDR // both are using 16 bit addressing for a segment and a byte respectively
#define XCM2_MAX_IO_DEVICES 256
#define XCM2_MAX_INSTRUCTIONS 256

typedef struct XCM2_ROM_SEGMENT { XCM2_WORD segment[XCM2_ROM_SEGMENT_SIZE]; } XCM2_ROM_SEGMENT;

#define XCM2_DWORD_HL(H, L) (((XCM2_DWORD)H) << XCM2_WORD_BITS) | L // big endian
#define XCM2_HIGH_WORD(DW) (XCM2_WORD)(DW >> XCM2_WORD_BITS)
#define XCM2_LOW_WORD(DW) (XCM2_WORD)DW
#define XCM2_DWORD_TO_WORDS(DW) XCM2_HIGH_WORD(DW), XCM2_LOW_WORD(DW)
#define XCM2_HIGHBIT_MASK 0x80
#define XCM2_LOWBIT_MASK 0x01

typedef enum XCM2_VIDEO_MODE
{
	XCM2_VIDEO_EGA,
	XCM2_VIDEO_TEXT
} XCM2_VIDEO_MODE;

typedef struct XCM2_Instruction
{
	XCM2_WORD ircode;
	XCM2_WORD arg0;
	XCM2_WORD arg1;
} XCM2_Instruction;

class IInterruptionListener
{
public:
	virtual void in_contact_INTERRUPT(XCM2_Instruction ir) = 0;
};

class IIODevice
{
protected:
	XCM2_WORD in_data_IN;
	bool in_data_RESET;
	XCM2_WORD out_data_OUT;
public:
	virtual void in_contact_IN(XCM2_WORD data) { in_data_IN = data; };
	virtual void in_contact_RESET() { in_data_RESET = true; };
	virtual XCM2_WORD out_contact_OUT() { return out_data_OUT; };
};

class ITickDevice
{
public:
	virtual void in_contact_TICK() = 0;
};

class XCM2GRAM;

class IGraphicAdapter
{
public:
	virtual void switchMode(XCM2_VIDEO_MODE mode) = 0;
	virtual void present() = 0;
	virtual void readPallette() = 0;
	virtual XCM2GRAM* back() = 0;
	virtual XCM2GRAM* sprite() = 0;
};

struct TextModeCharInfo
{
	XCM2_WORD ascii;
	XCM2_WORD fcolor;
	XCM2_WORD bcolor;
};

class ITextStreamWrapper
{
public:
    virtual ITextStreamWrapper* Append(const char* value) = 0;
    virtual ITextStreamWrapper* Append(const int value) = 0;
    virtual ITextStreamWrapper* Append(const std::string& value) = 0;
    virtual std::string GetText() = 0;
};

class coutStreamWrapper : public ITextStreamWrapper
{
private:
    std::ostream* cout;
public:
    coutStreamWrapper(std::ostream* cout);
    ITextStreamWrapper* Append(const char* value) override;
    ITextStreamWrapper* Append(const int value) override;
    ITextStreamWrapper* Append(const std::string& value) override;
    std::string GetText() override;
};

class sstreamWrapper : public ITextStreamWrapper
{
private:
    std::ostringstream* ss;
public:
    sstreamWrapper(std::ostringstream* ss);
    ITextStreamWrapper* Append(const char* value) override;
    ITextStreamWrapper* Append(const int value) override;
    ITextStreamWrapper* Append(const std::string& value) override;
    std::string GetText() override;
};
