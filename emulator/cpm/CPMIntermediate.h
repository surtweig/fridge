#pragma once
#include "CPMCompiler.h"
#include "CPMParser.h"

using namespace CPM;

namespace CPM
{
    struct CPMRelativeCodeChunk
    {
        FRIDGE_WORD* code;
        int size;
        vector<int> internalReferences;
        map<int, CPMStaticSymbol*> staticReferences;
        map<int, CPMFunctionSymbol*> functionReferences;

        CPMRelativeCodeChunk();
        CPMRelativeCodeChunk(vector<CPMRelativeCodeChunk*> merge);
        ~CPMRelativeCodeChunk();
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

    class CPMExecutableSemanticNode
    {
    private:
        CPMFunctionSymbol* ownerFunction;
        CPMSyntaxTreeNode* syntaxNode;
        CPMExecutableSemanticNode* parent;

    protected:
        vector<CPMExecutableSemanticNode*> children;

        virtual void procreate() {};
    public:
        CPMExecutableSemanticNode(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction);
        virtual void GenerateCode(CPMRelativeCodeChunk& code) {};
        inline CPMFunctionSymbol* OwnerFunction() { return ownerFunction; }
        inline CPMSyntaxTreeNode* SyntaxNode() { return syntaxNode; }
        inline CPMExecutableSemanticNode* Parent() { return parent; }
        inline Logger* CompilerLog() { return OwnerFunction()->compiler->CompilerLog(); }
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
        map<string, CPMDataSymbol> locals;

        CPMSemanticBlock(CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction);
        virtual void procreate();

        friend class CPMOperator_Alloc;
    public:
        CPMSemanticBlock(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode);
        ~CPMSemanticBlock();
    };

    class CPMFunctionSemanticBlock : public CPMSemanticBlock
    {
    public:
        CPMFunctionSemanticBlock(CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction);
    };

    class CPMOperator_Alloc : public CPMExecutableSemanticNode
    {
    public:
        CPMOperator_Alloc(CPMDataType dataType, CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode);
        virtual void GenerateCode(CPMRelativeCodeChunk& code);
        virtual void procreate() {};
    };

    class CPMIntermediate
    {
    public:
        CPMIntermediate();
        ~CPMIntermediate();
    };

}