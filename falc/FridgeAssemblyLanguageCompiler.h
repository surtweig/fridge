#ifndef FALC_FRIDGEASSEMBLYLANGUAGECOMPILER_H
#define FALC_FRIDGEASSEMBLYLANGUAGECOMPILER_H

#include <fridge.h>
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

#define STD_PATH "..\\x2al_std\\"
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
#define FRIDGE_INSTRUCTION_MAX_EXTRA_SIZE 2
#define FRIDGE_INSTRUCTION_MAX_OPERANDS 2
#define ResourceSizePostfix "_SIZE"
#define HashMemOriginAlias "MEM_ORIGIN"
#define R_UNSAFE_FLOW "unsafe_flow"

struct ParsedLine
{
    vector<string> words;
    FRIDGE_RAM_ADDR address;
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
    vector< array<OperandType, FRIDGE_INSTRUCTION_MAX_OPERANDS> > operands;
    int extraSize;
};

struct StaticResourceInfo
{
    FRIDGE_WORD* pdata;
    FRIDGE_DWORD size;
};

class FridgeAssemblyLanguageCompiler
{
private:
    vector<ParsedLine> lines;

    ostream* errstr;
    vector<string> includeFolders;
    int currentLineNumber;
    string currentSourceFile;
    bool unsafeFlowAllowed;
    map<string, string> aliases;
    map<string, FRIDGE_RAM_ADDR> entries;
    map<string, FRIDGE_RAM_ADDR> subroutines;
    map<string, StaticResourceInfo> resources;
    unordered_set<string> includes;
    FRIDGE_RAM_ADDR offset;
    FRIDGE_RAM_ADDR mainEntry;
    FRIDGE_RAM_ADDR programSize;
    FRIDGE_RAM_ADDR resOrigin;
    FRIDGE_WORD* objectCode;

    static unordered_set<string> KeywordIDs;
    static unordered_set<string> JumpIRIDs;
    static unordered_set<string> RetIRIDs;
    static unordered_set<string> CallIRIDs;
    static map<OperandType, string> RegOperandsIDs;
    static map<string, FRIDGE_WORD> IRIDs;
    static map<FRIDGE_WORD, InstructionSignature> IRSigs;
    static map<FRIDGE_WORD, string> IRNames;

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

    bool parseDec(string* word, FRIDGE_WORD& value);
    bool parseHex(string* word, FRIDGE_WORD& value);
    bool parseHexDouble(string* word, FRIDGE_DWORD& value);
    bool parseChar(string* word, FRIDGE_WORD& value);
    bool parseReg(string* word, OperandType& value);
    bool parseRegPair(string* word, OperandType& value);
    template< typename T > string int_to_hex(T i);
    string int_to_hex_vhd(FRIDGE_WORD i);

public:
    FridgeAssemblyLanguageCompiler(string sourceRootFolder, string sourceFileName, string outputFile, vector<string> includeFolders, ostream* errstr, bool saveVHDL = false);

    void printParsed();
    void printCompiled();
    void printCompiled(ostream* f);
    inline static string GetIRName(FRIDGE_WORD ircode) { return IRNames[ircode]; }

    ~FridgeAssemblyLanguageCompiler();
};

#endif //FALC_FRIDGEASSEMBLYLANGUAGECOMPILER_H
