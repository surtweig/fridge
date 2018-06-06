#pragma once
#include "CPMCompiler.h"
#include "CPMParser.h"

using namespace CPM;

namespace CPM
{
    struct CPMRelativeCodeChunk
    {
        XCM2_WORD* code;
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
        vector<CPMExecutableSemanticNode*> children;

    protected:
        CPMExecutableSemanticNode(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction);
        virtual void procreate() = 0;
    public:
        virtual void GenerateCode(CPMRelativeCodeChunk& code) = 0;
        inline CPMFunctionSymbol* OwnerFunction() { return ownerFunction; }
        inline CPMSyntaxTreeNode* SyntaxNode() { return syntaxNode; }
        inline CPMExecutableSemanticNode* Parent() { return parent; }
        inline Logger* CompilerLog() { return OwnerFunction()->compiler->CompilerLog(); }
        ~CPMExecutableSemanticNode();
    };

    class CPMSemanticOperator : CPMExecutableSemanticNode
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

    class CPMSemanticBlock : CPMExecutableSemanticNode
    {
    protected:
        map<string, CPMDataSymbol> locals;

        CPMSemanticBlock(CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction);
        CPMSemanticBlock(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode);
        virtual void procreate();
    public:
        ~CPMSemanticBlock();
    };

    class CPMFunctionSemanticBlock : CPMSemanticBlock
    {
    public:
        CPMFunctionSemanticBlock(CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction);
    };

    class CPMOperator_Alloc : CPMExecutableSemanticNode
    {
    protected:
        CPMOperator_Alloc(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode);
    public:
        virtual void GenerateCode(CPMRelativeCodeChunk& code);
    };

    class CPMIntermediate
    {
    public:
        CPMIntermediate();
        ~CPMIntermediate();
    };

}