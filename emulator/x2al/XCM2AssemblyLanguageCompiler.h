#pragma once

#include "../emulator/XCM2IRCodes.h"
#include "../emulator/XCM2.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <unordered_set>
#include <iomanip>
#include <sstream>
using namespace std;

#define STD_PATH "..\\..\\x2al_std\\"
#define R_INCLUDE "include"
#define R_ALIAS "alias"
#define R_SUBROUTINE "subroutine"
#define R_ENDSUB "endsub"
#define R_ENDSUB_DEALIAS "RET"
#define R_ENTRY "entry"
#define R_MAIN "main"
#define R_OFFSET "offset"
#define R_STATICRESOURCE "static"
#define IRCodeAliasPrefix "#"
#define XCM2_INSTRUCTION_MAX_EXTRA_SIZE 2 
#define XCM2_INSTRUCTION_MAX_OPERANDS 2 
#define ResourceSizePostfix "_SIZE"
#define HashMemOriginAlias "MEM_ORIGIN"
#define R_UNSAFE_FLOW "unsafe_flow"

struct ParsedLine
{
    vector<string> words;
    XCM2_RAM_ADDR address;
    int lineNumber;
    string sourceFile;
};

typedef enum OperandType
{
    NONE,
    REG_A, REG_B, REG_C, REG_D, REG_E, REG_H, REG_L, REG_M,
    PAIR_AF, PAIR_BC, PAIR_DE, PAIR_HL, PAIR_SP,
    IM_WORD, IM_DOUBLE_WORD,
    //ENTRY,
} OperandType;

struct InstructionSignature
{
    vector< array<OperandType, XCM2_INSTRUCTION_MAX_OPERANDS> > operands;
    int extraSize;
};

struct StaticResourceInfo
{
    XCM2_WORD* pdata;
    XCM2_DWORD size;
};

class XCM2AssemblyLanguageCompiler
{
private:
    vector<ParsedLine> lines;

    ITextStreamWrapper* errstr;
    vector<string> includeFolders;
    int currentLineNumber;
    string currentSourceFile;
    bool unsafeFlowAllowed;
    map<string, string> aliases;
    map<string, XCM2_RAM_ADDR> entries;
    map<string, XCM2_RAM_ADDR> subroutines;
    map<string, StaticResourceInfo> resources;
    unordered_set<string> includes;
    XCM2_RAM_ADDR offset;
    XCM2_RAM_ADDR mainEntry;
    XCM2_RAM_ADDR programSize;
    XCM2_RAM_ADDR resOrigin;
    XCM2_WORD* objectCode;

    static unordered_set<string> KeywordIDs;
    static unordered_set<string> JumpIRIDs;
    static unordered_set<string> RetIRIDs;
    static unordered_set<string> CallIRIDs;
    static map<OperandType, string> RegOperandsIDs;
    static map<string, XCM2_WORD> IRIDs;
    static map<XCM2_WORD, InstructionSignature> IRSigs;
    static map<XCM2_WORD, string> IRNames;

    bool preprocessFile(string sourceRootFolder, string sourceFileName);
    void parseLine(string* line, vector<string> &words);
    void logout(string message);
    void logerrout(string message);
    void logwarnout(string message);
    void initDefaultAliases();
    bool readResources();
    bool dealias();
    bool addressMarkup();
    bool generateObjectCode();
    bool saveObjectCode(string outputFile);
    bool saveObjectCodeVHDL(string outputFile);

    bool parseDec(string* word, XCM2_WORD& value);
    bool parseHex(string* word, XCM2_WORD& value);
    bool parseHexDouble(string* word, XCM2_DWORD& value);
    bool parseChar(string* word, XCM2_WORD& value);
    bool parseReg(string* word, OperandType& value);
    bool parseRegPair(string* word, OperandType& value);
    template< typename T > string int_to_hex(T i);
    string int_to_hex_vhd(XCM2_WORD i);

public:
    XCM2AssemblyLanguageCompiler(string sourceRootFolder, string sourceFileName, string outputFile, vector<string> includeFolders, ITextStreamWrapper* errstr, bool saveVHDL = false);

    void printParsed();
    void printCompiled();
    void printCompiled(ITextStreamWrapper* f);
    inline static string GetIRName(XCM2_WORD ircode) { return IRNames[ircode]; }

    ~XCM2AssemblyLanguageCompiler();
};

