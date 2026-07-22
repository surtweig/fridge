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
#include <memory>

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

    // Recursive type-expr. Replaces the (CPMDataType type; bool isPtr; int count)
    // triple that was scattered across CPMDataSymbol / CPMArgumentSignature /
    // CPMFunctionSymbol. Note: `string` stays a special primitive
    // (CPM_DATATYPE_STRING); an explicit `(char ptr)` typed by the user is a
    // different CPMType and does NOT share the string literal pool.
    enum CPMTypeKind
    {
        CPM_TYPE_VOID,
        CPM_TYPE_BASE,    // a primitive or named struct
        CPM_TYPE_PTR,     // (T ptr)
        CPM_TYPE_ARRAY,   // (T array n)
    };

    struct CPMType
    {
        CPMTypeKind              kind;
        CPMDataType              base;        // valid for CPM_TYPE_BASE; CPM_DATATYPE_VOID for CPM_TYPE_VOID
        int                      arrayCount;  // valid for CPM_TYPE_ARRAY (const-folded)
        std::unique_ptr<CPMType> child;       // target type for CPM_TYPE_PTR / CPM_TYPE_ARRAY

        CPMType();
        ~CPMType();
        CPMType(const CPMType& other);
        CPMType& operator=(const CPMType& other);
        CPMType& operator=(CPMType&& other) = default;
        CPMType(CPMType&& other) = default;

        bool operator==(const CPMType& o) const;
        bool operator!=(const CPMType& o) const { return !(*this == o); }
        bool operator<(const CPMType& o) const;   // for map keys (signatures)

        bool isVoid()      const { return kind == CPM_TYPE_VOID; }
        bool isPtr()       const { return kind == CPM_TYPE_PTR; }
        bool isArray()     const { return kind == CPM_TYPE_ARRAY; }
        bool isBase()      const { return kind == CPM_TYPE_BASE; }
        bool isPrimitive() const { return kind == CPM_TYPE_BASE && base != CPM_DATATYPE_USER && base != CPM_DATATYPE_VOID; }
        bool isStruct()    const { return kind == CPM_TYPE_BASE && base == CPM_DATATYPE_USER; }
        bool isString()    const { return kind == CPM_TYPE_BASE && base == CPM_DATATYPE_STRING; }
    };

    const string R_INCLUDE = "include";
    const string R_USING = "using";
    const string R_IMPORT = "imports";
    const string R_NAMESPACE = "namespace";
    const string R_STRUCT = "struct";
    const string R_UNION = "union";
    const string R_STATIC = "static";
	const string R_CONST = "const";
    const string R_ARRAY = "array";
    const string R_PTR = "ptr";
    const string R_AT = "at";
    const string R_ADDR = "addr";
    const string R_CAST = "cast";
    const string R_NULL = "null";
    const string R_REF = "ref";          // LEGACY: removed in step 7
    const string GlobalNamespace = "global";
    const char PtrPrefix = '#';          // LEGACY: removed in step 7
    const char RefPrefix = '&';          // LEGACY: removed in step 7
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
        CPMType typeExpr;          // was: CPMDataType type; bool isPtr; int count;
        FRIDGE_DWORD offset;
        FRIDGE_RAM_ADDR globalAddress;
        CPMNamespace* owner;

        CPMDataSymbol();
        //FRIDGE_RAM_ADDR serialize(vector<FRIDGE_WORD>& output);
        ~CPMDataSymbol();

        // ---- Legacy accessors (transitional, to be removed in step 4b) ----
        // These compute the old flat view from the recursive CPMType. They
        // only make sense for the legacy-supported shapes (BASE, PTR, ARRAY
        // of BASE/PTR). For the new fully-nested forms they return a best-
        // effort approximation; new code should prefer typeExpr directly.
        CPMDataType legacyType() const;   // the "element" base type id
        bool        legacyIsPtr() const;  // true iff top-level kind is PTR
        int         legacyCount() const;  // 1 for non-arrays, arrayCount for arrays
        // Reconstruct typeExpr from the legacy flat triple. Used by the
        // un-migrated detect/read functions; once they call parseTypeExpr
        // directly this helper disappears.
        void setLegacyType(CPMDataType base, bool isPtr, int count);
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
        CPMType typeExpr;          // was: CPMDataType type; int count; bool isPtr;

        // ---- Legacy accessors (transitional) ----
        CPMDataType legacyType() const;
        bool        legacyIsPtr() const;
        int         legacyCount() const;
        void setLegacyType(CPMDataType base, bool isPtr, int count);
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
        CPMType returnType;          // was: bool isPtr; CPMDataType type;
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
        void computeStructDataSize(CPMStructSymbol* structSymbol);
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
        // Recursive type-expr parser per spec §3. Recognises:
        //   - bare primitive/struct name (CPM_ID)
        //   - qualified NS.Name (CPM_REF with two ID children)
        //   - (T ptr)        (CPM_EXPR with children [type-expr, "ptr"])
        //   - (T array n)    (CPM_EXPR with children [type-expr, "array", n])
        // On success returns true and fills `out`; on failure returns false
        // and emits an error to compilerLog. Does not modify `out` on failure.
        // Exists in parallel with the legacy resolveDataTypeName during the
        // migration; step 4 replaces the legacy walker and deletes it.
        bool parseTypeExpr(CPMSyntaxTreeNode* node, CPMType& out, CPMSourceFile* sourceFile, CPMNamespace* currentNS = NULL);
        // Helper for parseTypeExpr: resolve a bare type name (from a CPM_ID
        // already split out of any NS.Name qualifier) to a primitive or a
        // user-defined struct CPMDataType, walking the regular scope chain.
        // Returns true and sets outBase/outOwnerNS on success.
        bool resolveBaseTypeName(const string& typeName, CPMSourceFile* sourceFile,
                                 CPMNamespace* currentNS,
                                 CPMDataType& outBase, CPMNamespace*& outOwnerNS);
        // Recursive size-of for the new CPMType. Returns the size in bytes
        // (1 for uint8/char/bool, 2 for uint16/int16/string/pointers,
        // sum-of-fields for structs, count*childSize for arrays).
        int sizeOfTypeExpr(const CPMType& type, CPMNamespace* currentNS = NULL);
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
