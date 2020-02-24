#pragma once
#include "CPMCompiler.h"
#include "CPMParser.h"
#include <cassert>

using namespace CPM;

namespace CPM
{
    struct CPMRelativeCodeChunk
    {
        FRIDGE_WORD* code;
        size_t size;
        vector<int> internalReferences;
        map<int, CPMStaticSymbol*> staticReferences;
        map<int, CPMFunctionSymbol*> functionReferences;

        CPMRelativeCodeChunk();
        CPMRelativeCodeChunk(size_t size);
        CPMRelativeCodeChunk(vector<FRIDGE_WORD> buffer);
        CPMRelativeCodeChunk(vector<CPMRelativeCodeChunk*> merge);
        ~CPMRelativeCodeChunk();

        void write(vector<FRIDGE_WORD> buffer, size_t offset = 0);
    };

   /* class CPMOperator
    {
    protected:
        string name;
        CPMExecutableSemanticNode* semanticNode;
    public:
        CPMOperator(CPMExecutableSemanticNode* semanticNode);
        inline string Name() { return name; }
        inline CPMExecutableSemanticNode* SemanticNode() { return semanticNode; }
        inline virtual CPMDataType ReturnType() { return CPM_DATATYPE_VOID; }
        virtual void GenerateCode(CPMRelativeCodeChunk& code) = 0;
    };*/
    class CPMSemanticBlock;

    class CPMExecutableSemanticNode
    {
    private:
        CPMFunctionSymbol* ownerFunction;
        CPMSyntaxTreeNode* syntaxNode;
        CPMExecutableSemanticNode* parent;

    protected:
        vector<CPMExecutableSemanticNode*> children;
        CPMSemanticBlock* blockParent;

        virtual void procreate() {};
    public:
        CPMExecutableSemanticNode(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction);
        virtual CPMRelativeCodeChunk* GenerateCode() { return nullptr; }
        inline CPMFunctionSymbol* OwnerFunction() { return ownerFunction; }
        inline CPMSyntaxTreeNode* SyntaxNode() { return syntaxNode; }
        inline CPMExecutableSemanticNode* Parent() { return parent; }
        inline void Error() { OwnerFunction()->compiler->Error(); }
        inline Logger* CompilerLog() { return OwnerFunction()->compiler->CompilerLog(); }
        inline Logger* AsmLog() { return OwnerFunction()->compiler->AsmDebugOutput(); }
        ~CPMExecutableSemanticNode();
    };

    class CPMSemanticOperator : public CPMExecutableSemanticNode
    {
    private:
        CPMFunctionSignature signature;
        CPMNamespace* ns;
    public:
        CPMSemanticOperator(CPMFunctionSymbol* proto, CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode);
        virtual CPMDataType ResultType() { return CPM_UNDEFINED; }
        string Name() { return signature.name; }
        CPMFunctionSignature Signature() { return signature; }
        CPMNamespace* Namespace() { return ns; }

    };

    class CPMSemanticBlock : public CPMExecutableSemanticNode
    {
    protected:
        map<string, CPMDataSymbol*> locals;
        vector<CPMDataSymbol*> literals;
        FRIDGE_DWORD stackOffset;

        CPMSemanticBlock(CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction);
        virtual void procreate();

        friend class CPMOperator_Alloc;
    public:
        CPMSemanticBlock(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode);
        ~CPMSemanticBlock();
        virtual CPMRelativeCodeChunk* GenerateCode();
        CPMDataSymbol* resolveLocalSymbolName(const string &name);
        FRIDGE_DWORD StackOffset() { return stackOffset; }
        void addLiteral(CPMDataSymbol* symbol) { literals.push_back(symbol); }
    };

    class CPMFunctionSemanticBlock : public CPMSemanticBlock
    {
    public:
        CPMFunctionSemanticBlock(CPMFunctionSymbol* ownerFunction);
    };

    class CPMOperator_Alloc : public CPMExecutableSemanticNode
    {
    public:
        CPMOperator_Alloc(CPMDataType dataType, bool isPtr, CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode);
        virtual CPMRelativeCodeChunk* GenerateCode();
        virtual void procreate() {};
    };

    class CPMSematicExpression : public CPMExecutableSemanticNode
    {
    private:
        CPMDataType resultType;
        bool resultIsPtr;
        CPMDataSymbol* resultSymbol;
        int resultIndex;
    public:
        CPMSematicExpression(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode);
        virtual CPMRelativeCodeChunk* GenerateCode();
        virtual void procreate();

        CPMDataType ResultType() { return resultType; }
        bool ResultIsPtr() { return resultIsPtr; }
        CPMDataSymbol* ResultSymbol() { return resultSymbol; }
        int ResultIndex() { return resultIndex; }
    };

    class CPMOperator_Assign : public CPMExecutableSemanticNode
    {
    private:
        CPMDataSymbol* destination;
        CPMSematicExpression* destExpr;
        CPMDataSymbol* source;
        CPMSematicExpression* srcExpr;
        bool staticDest;
        bool literalSource;
        
    public:
        CPMOperator_Assign(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode);
        virtual CPMRelativeCodeChunk* GenerateCode();
        virtual void procreate();
    };

    class CPMIntermediate
    {
    private:
        CPMRelativeCodeChunk* objectCode;
    public:
        CPMIntermediate(CPMCompiler* compiler);
        ~CPMIntermediate();
    };

}