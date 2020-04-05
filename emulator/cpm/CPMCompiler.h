#pragma once
#include <string>
#include <ostream>
#include <sstream>
#include <iterator>
//#include "../emulator/XCM2.h"
#include "../../include/fridge.h"
#include "Logger.h"
#include "CPMParser.h"
#include <map>
#include <vector>
#include <unordered_set>
#include <stack>

using namespace std;

namespace CPM
{
    typedef int CPMDataType;
    typedef unsigned char CPM_UINT8;
    typedef unsigned short CPM_UINT16;
    typedef signed char CPM_INT8;
    typedef signed short CPM_INT16;

    const CPMDataType CPM_DATATYPE_UNDEFINED = -2;
    const CPMDataType CPM_DATATYPE_AMBIGUOUS = -1;
    const CPMDataType CPM_DATATYPE_VOID = 0;
    const CPMDataType CPM_DATATYPE_BOOL = 1;
    const CPMDataType CPM_DATATYPE_CHAR = 2;
    const CPMDataType CPM_DATATYPE_UINT8 = 3;
    const CPMDataType CPM_DATATYPE_UINT16 = 4;
    const CPMDataType CPM_DATATYPE_INT8 = 5;
    const CPMDataType CPM_DATATYPE_INT16 = 6;
    const CPMDataType CPM_DATATYPE_STRING = 7;
    const CPMDataType CPM_DATATYPE_USER = 8;

    const int INT8_Min = -128;
    const int INT8_Max = 127;
    const int UINT8_Min = 0;
    const int UINT8_Max = 255;
    const int INT16_Min = -32768;
    const int INT16_Max = 32767;
    const int UINT16_Min = 0;
    const int UINT16_Max = 65535;
    const int DataMaxSize = 65535 - 256;

    bool IsIntDataType(CPMDataType dtype);

    const string R_INCLUDE = "include";
    const string R_USING = "using";
    const string R_IMPORT = "imports";
    const string R_NAMESPACE = "namespace";
    const string R_STRUCT = "struct";
    const string R_UNION = "union";
    const string R_STATIC = "static";
	const string R_CONST = "const";
    const string R_ARRAY = "array";
    const string R_NULL = "null";
    const string R_REF = "ref";
    const string GlobalNamespace = "global";
    const char PtrPrefix = '#';
    const char RefPrefix = '&';
    const char ServiceSymbol = '$';
    const string OP_ASSIGN = "=";

    struct CPMSourceFile
    {
        string name;
        
        CPMParser* parser;
        vector<string> usingNamespaces;
    };

    struct CPMNamespace;

    struct CPMDataSymbol
    {
        string name;
        CPMDataType type;
        bool isPtr;
        int count;
        FRIDGE_DWORD offset;
        vector<FRIDGE_WORD*> data;
        CPMNamespace* owner;
        FRIDGE_RAM_ADDR globalAddress;

        CPMDataSymbol();
        FRIDGE_RAM_ADDR serialize(vector<FRIDGE_WORD>& output);
        ~CPMDataSymbol();
    };

    struct CPMStaticSymbol
    {
        CPMDataSymbol field;
        bool isconst;
        int importSource;

        CPMStaticSymbol();
    };

    struct CPMStructSymbol
    {
        string name;
        CPMDataType type;
        CPMSyntaxTreeNode* node;
        bool isUnion;
        int size;
        map<string, CPMDataSymbol> fields;
        CPMNamespace* owner;

        CPMStructSymbol();
    };

    struct CPMArgumentSignature
    {
        string name;
        CPMDataType type;
        int count;
        bool isPtr;
        //bool isRef;
    };

    struct CPMFunctionSignature
    {
        string name;
        vector<CPMArgumentSignature> arguments;

        bool operator< (const CPMFunctionSignature& other) const; // other < this
    };

    struct CPMNamespace;
    class CPMCompiler;

    struct CPMFunctionSymbol
    {
        CPMFunctionSignature signature;
        vector<CPMDataSymbol> arguments;
        bool isPtr;
        CPMDataType type;
        CPMNamespace* owner;
        CPMCompiler* compiler;
        FRIDGE_RAM_ADDR globalAddress;
        CPMSyntaxTreeNode* bodyNode;
    };

    struct CPMNamespace
    {
        string name;
        string importSource;
        vector<CPMSyntaxTreeNode*> nodes;
        map<string, CPMStaticSymbol> statics;
        map<string, CPMStructSymbol> structs;
        map<CPMFunctionSignature, CPMFunctionSymbol> functions;
        map<string, CPMDataType> datatypes;
        FRIDGE_WORD* staticBuffer;
    };

    struct CPMUnfoldedExpressionNode
    {
        int operandsNumber;
        CPMSyntaxTreeNode* syntaxNode;
    };

    class CPMCompiler
    {
    private:
        map<string, CPMSourceFile> sources;
        map<string, CPMNamespace> namespaces;
        map<CPMDataType, CPMStructSymbol*> structTypes;
        string outputFileName;
        vector<string> includeFolders;
        bool noErrors;
        Logger compilerLog;
        Logger asmDebugOutput;
        CPMDataType dataTypeCounter;
        FRIDGE_RAM_ADDR globalOffset;

        void preprocessSourceFile(string rootFolder, string filename);
        void readNamespaces();
        CPMStaticSymbol* addStatic(string name, CPMNamespace* ns, bool isConst, CPMDataType type = CPM_DATATYPE_UNDEFINED, int data = NULL);

        void readStructs();
        void detectStruct(CPMSyntaxTreeNode* node, CPMNamespace* owner);
        CPMDataType registerStructDataType(CPMStructSymbol* structSymbol);
        void readStructFields(CPMStructSymbol* structSymbol);
        void computeStructDataSize(CPMStructSymbol* structSymbol, unordered_set<CPMDataType> &visitedStructs);
        void buildStructLayout(CPMStructSymbol* structSymbol);

        void readStatics(bool isConst);
        void detectStatic(CPMSyntaxTreeNode* node, CPMNamespace* owner, bool isConst);

        void readFunctions();
        void detectFunction(CPMSyntaxTreeNode* node, CPMNamespace* owner);

        //void writeStaticNum(CPMStaticSymbol* symbol, int value);
        //int readStaticNum(CPMStaticSymbol* symbol);

        string printStaticValue(CPMDataSymbol* symbol);
        string printStaticNumber(CPMDataSymbol* symbol, int index = 0);
        string printStaticString(CPMDataSymbol* symbol, int index = 0);
        string printStaticChar(CPMDataSymbol* symbol, int index = 0);
        string printStaticStruct(CPMDataSymbol* symbol, int index = 0);
    public:
        CPMCompiler(string sourceRootFolder, string sourceFileName, string outputFile, vector<string> includeFolders);
        inline Logger* CompilerLog() { return &compilerLog; }
        inline void Error() { noErrors = false; }
        inline bool NoErrors() { return noErrors; }
        inline Logger* AsmDebugOutput() { return &asmDebugOutput; }
        void PrintStaticData();
        ~CPMCompiler();

        int sizeOfType(CPMDataType type);
        int sizeOfData(CPMDataSymbol* dataSymbol);

        CPMSourceFile* getSourceFile(const string& sourceFileName) { return &sources[sourceFileName]; }
        void getNamespaces(vector<CPMNamespace*>& nslist);
        FRIDGE_RAM_ADDR getGlobalOffset() { return globalOffset; }
        string getOutputFileName() { return outputFileName; }
        int parseArraySizeDecl(CPMSyntaxTreeNode* countNode, CPMNamespace* currentNS = NULL);
        bool parseLiteralValue(CPMDataSymbol* symbol, CPMSyntaxTreeNode* valueNode);
        CPMDataType resolveDataTypeName(CPMSyntaxTreeNode* nameNode, bool& isPtr, CPMSourceFile* sourceFile, CPMNamespace* currentNS = NULL);
        CPMStaticSymbol* resolveStaticSymbolName(CPMSyntaxTreeNode* nameNode, CPMSourceFile* sourceFile, CPMSyntaxTreeNode* syntaxNode, CPMNamespace* currentNS = NULL);
        CPMFunctionSymbol* resolveFunctionSymbolName(const string &name, CPMSourceFile* sourceFile, CPMNamespace* currentNS = NULL);
        int parseNum(const string& num);
        bool parseExpression(CPMSyntaxTreeNode* root, vector<CPMUnfoldedExpressionNode>& unfolded);
        int evalNum(vector<CPMUnfoldedExpressionNode>& unfolded, bool& ok, CPMNamespace* currentNS = NULL, bool silent = false);
        bool parseLiteralNumber(CPMDataSymbol* symbol, CPMSyntaxTreeNode* valueNode, int index = 0, bool autoType = false);
        bool parseLiteralString(CPMDataSymbol* symbol, CPMSyntaxTreeNode* valueNode, int index = 0);
        bool parseLiteralChar(CPMDataSymbol* symbol, CPMSyntaxTreeNode* valueNode, int index = 0);
        bool parseLiteralStruct(CPMDataSymbol* symbol, CPMSyntaxTreeNode* valueNode, int index = 0);
        //static vector<string> ParseSymbolName(const string& name);
        string GetTypeName(CPMDataType typeId, CPMNamespace* ns = nullptr);
    };

}