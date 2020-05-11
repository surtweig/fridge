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
    const CPMDataType CPM_DATATYPE_POINTER = CPM_DATATYPE_UINT16;

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
        FRIDGE_RAM_ADDR globalAddress;
        CPMNamespace* owner;

        CPMDataSymbol();
        //FRIDGE_RAM_ADDR serialize(vector<FRIDGE_WORD>& output);
        ~CPMDataSymbol();
    };

    struct CPMStaticSymbol
    {
        CPMDataSymbol field;
        bool isconst;
        int importSource;
        FRIDGE_WORD* staticData;
        int immediateData; // This is used to store value of single basic-type constants
        vector<FRIDGE_DWORD> staticStrings; // stores relative addresses of all statically allocated string fields (not data)

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
        FRIDGE_WORD staticBuffer[DataMaxSize];
        size_t staticBufferSize;

    public:
        CPMNamespace(string name)
        {
            this->name = name;
            staticBufferSize = 0;
            for (int i = 0; i < DataMaxSize; ++i)
                staticBuffer[i] = 0;
        }

        size_t getStaticRelativeAddress(FRIDGE_WORD* ptr)
        {
            return (size_t)ptr - (size_t)staticBuffer;
        }

        FRIDGE_WORD* staticAllocate(FRIDGE_WORD* data, size_t typesize, size_t count)
        {
            //CPM_ASSERT(count > 0);
            //CPM_ASSERT(typesize > 0);
            size_t size = count * typesize;
            if (staticBufferSize + size <= DataMaxSize)
            {
                int pos = staticBufferSize;
                for (size_t i = 0; i < count; ++i)
                    if (data)
                        staticBuffer[staticBufferSize++] = data[i];
                    else
                        staticBuffer[staticBufferSize++] = 0;

                return &staticBuffer[pos];
            }
            else
                return nullptr;
        }

        FRIDGE_WORD* staticAllocate(FRIDGE_WORD data, FRIDGE_DWORD count = 1)
        {
            //CPM_ASSERT(count > 0);
            if (staticBufferSize < DataMaxSize-count+1)
            {       
                int pos = staticBufferSize;
                for (size_t i = 0; i < count; ++i)
                    staticBuffer[staticBufferSize++] = data;
                return &staticBuffer[pos];
            }
            else
                return nullptr;
        }

        FRIDGE_WORD* staticAllocate(FRIDGE_DWORD data, FRIDGE_DWORD count = 1)
        {
            //CPM_ASSERT(count > 0);
            if (staticBufferSize < DataMaxSize-count*sizeof(FRIDGE_DWORD)+1)
            {
                int pos = staticBufferSize;
                for (size_t i = 0; i < count; ++i)
                {
                    staticBuffer[staticBufferSize++] = FRIDGE_HIGH_WORD(data);
                    staticBuffer[staticBufferSize++] = FRIDGE_LOW_WORD(data);
                }
                return &staticBuffer[pos];
            }
            else
                return nullptr;
        }

        FRIDGE_WORD* staticAllocate(string data)
        {
            if (staticBufferSize < DataMaxSize - data.size())
            {
                int pos = staticBufferSize;
                for (int i = 0; i < data.size(); ++i)
                {
                    staticBuffer[pos + i] = (FRIDGE_WORD)data[i];
                }
                staticBuffer[pos + data.size()] = 0;
                staticBufferSize += data.size() + 1;
                return &staticBuffer[pos];
            }
            else
                return nullptr;
        }

        void staticWrite(FRIDGE_DWORD addr, FRIDGE_WORD data)
        {
            staticBuffer[addr] = data;
        }

        void staticWrite(FRIDGE_DWORD addr, FRIDGE_DWORD data)
        {
            staticBuffer[addr] = FRIDGE_HIGH_WORD(data);
            staticBuffer[addr + 1] = FRIDGE_LOW_WORD(data);
        }

        void staticWrite(FRIDGE_DWORD addr, string data)
        {
            for (int i = 0; i < data.size(); ++i)
                staticBuffer[addr + i] = (FRIDGE_WORD)data[i];

            staticBuffer[addr + data.size()] = 0;
        }

        CPMNamespace(CPMNamespace& source)
        {
            throw logic_error("CPMNamespace copy constructor is not implemented!");
        }

        CPMNamespace& operator=(const CPMNamespace& other)
        {
            throw logic_error("CPMNamespace copy assignment operator is not implemented!");
        }
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
        map<string, CPMNamespace*> namespaces;
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
        CPMStaticSymbol* addNumStatic(string name, CPMNamespace* ns, bool isConst, CPMDataType type, int data);
        CPMStaticSymbol* addStrStatic(string name, CPMNamespace* ns, bool isConst, string data);
        CPMStaticSymbol* addStatic(string name, CPMNamespace* ns, bool isConst, bool isPtr, CPMDataType type, FRIDGE_DWORD count = 1, int importSource = -1);

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

        string printStaticValue(CPMStaticSymbol* symbol, CPMDataSymbol& field);
        string printStaticNumber(CPMStaticSymbol* symbol, CPMDataSymbol& field, int index = 0);
        string printStaticString(CPMStaticSymbol* symbol, CPMDataSymbol& field, int index = 0);
        string printStaticChar(CPMStaticSymbol* symbol, CPMDataSymbol& field, int index = 0);
        string printStaticStruct(CPMStaticSymbol* symbol, CPMDataSymbol& field, int index = 0);
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
        CPMDataType resolveDataTypeName(CPMSyntaxTreeNode* nameNode, bool& isPtr, CPMSourceFile* sourceFile, CPMNamespace* currentNS = NULL);
        CPMStaticSymbol* resolveStaticSymbolName(CPMSyntaxTreeNode* nameNode, CPMSourceFile* sourceFile, CPMSyntaxTreeNode* syntaxNode, CPMNamespace* currentNS = NULL);
        CPMFunctionSymbol* resolveFunctionSymbolName(const string &name, CPMSourceFile* sourceFile, CPMNamespace* currentNS = NULL);
        int parseNum(const string& num);
        bool parseExpression(CPMSyntaxTreeNode* root, vector<CPMUnfoldedExpressionNode>& unfolded);
        int staticEvalNum(vector<CPMUnfoldedExpressionNode>& unfolded, bool& ok, CPMNamespace* currentNS = NULL, bool silent = false);
        bool parseLiteralValue(CPMStaticSymbol* symbol, CPMDataSymbol& field, CPMSyntaxTreeNode* valueNode);
        bool parseLiteralNumber(CPMStaticSymbol* symbol, CPMDataSymbol& field, CPMSyntaxTreeNode* valueNode, int index = 0, bool autoType = false);
        bool parseAndAllocateLiteralString(CPMStaticSymbol* symbol, CPMDataSymbol& field, CPMSyntaxTreeNode* valueNode, int index = 0);
        bool parseLiteralChar(CPMStaticSymbol* symbol, CPMDataSymbol& field, CPMSyntaxTreeNode* valueNode, int index = 0);
        bool parseLiteralStruct(CPMStaticSymbol* symbol, CPMDataSymbol& field, CPMSyntaxTreeNode* valueNode, int index = 0);
        //static vector<string> ParseSymbolName(const string& name);
        string GetTypeName(CPMDataType typeId, CPMNamespace* ns = nullptr);
    };

}