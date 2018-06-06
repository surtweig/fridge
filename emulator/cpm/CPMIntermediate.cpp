#include "stdafx.h"
#include "CPMIntermediate.h"
#include "CPMCompiler.h"

namespace CPM
{

    CPMRelativeCodeChunk::CPMRelativeCodeChunk()
    {
        code = NULL;
        size = 0;
    }

    CPMRelativeCodeChunk::CPMRelativeCodeChunk(vector<CPMRelativeCodeChunk*> merge)
    {
        size_t totalSize = size;

        for (int i = 0; i < merge.size(); ++i)
            totalSize += merge[i]->size;

        XCM2_WORD* mergedCode = (XCM2_WORD*)malloc(totalSize);
        int offset = 0;

        memcpy(mergedCode, code, size);
        offset += size;
        
        for (int i = 0; i < merge.size(); ++i)
        {
            for (map<int, CPMStaticSymbol*>::iterator ssi = merge[i]->staticReferences.begin(); ssi != merge[i]->staticReferences.end(); ++ssi)
                staticReferences[ssi->first + offset] = ssi->second;

            for (map<int, CPMFunctionSymbol*>::iterator fsi = merge[i]->functionReferences.begin(); fsi != merge[i]->functionReferences.end(); ++fsi)
                functionReferences[fsi->first + offset] = fsi->second;

            memcpy(mergedCode+offset, merge[i]->code, merge[i]->size);

            for (int ir = 0; ir < merge[i]->internalReferences.size(); ++ir)
            {
                int relPos = merge[i]->internalReferences[ir];
                XCM2_DWORD ref = XCM2_DWORD_HL(merge[i]->code[relPos], merge[i]->code[relPos + 1]);
                internalReferences.push_back(offset + relPos);
                ref += offset;
                mergedCode[offset + relPos] = XCM2_HIGH_WORD(ref);
                mergedCode[offset + relPos + 1] = XCM2_LOW_WORD(ref);
            }

            offset += merge[i]->size;
        }

        delete code;
        code = mergedCode;
    }

    CPMRelativeCodeChunk::~CPMRelativeCodeChunk()
    {
        delete code;
    }

    CPMExecutableSemanticNode::CPMExecutableSemanticNode(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction)
    {
        this->parent = parent;
        this->syntaxNode = syntaxNode;
        this->ownerFunction = ownerFunction;
    }

    CPMExecutableSemanticNode::~CPMExecutableSemanticNode()
    {
        for (int i = 0; i < children.size(); ++i)
            delete children[i];
    }

    CPMSemanticBlock::CPMSemanticBlock(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode) : CPMExecutableSemanticNode(parent, syntaxNode, parent->OwnerFunction())
    {
    }

    CPMSemanticBlock::CPMSemanticBlock(CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction) : CPMExecutableSemanticNode(NULL, syntaxNode, ownerFunction)
    {
    }

    void CPMSemanticBlock::procreate()
    {
        for (int childIndex = 0; childIndex < SyntaxNode()->children.size(); ++childIndex)
        {
            CPMSyntaxTreeNode* line = SyntaxNode()->children[childIndex];
            if (line->type == CPM_LINE)
            {
                if (line->children.size() > 0)
                {
                    CPMSyntaxTreeNode* opname = line->children[0];
                    if (opname->type == CPM_ID)
                    {
                        // operator lookup (opname):
                        // --- search in types
                        // --- search in intristic
                        // --- search in functions (OwnerFunction()->compiler->resolveFunctionSymbol(opname)
                        // 
                        // create node (line, ownerFunction)
                        //
                        // node->procreate

                        //OwnerFunction()->compiler->resolveDataTypeName()
                    }
                    else
                    {
                        CompilerLog()->Add(LOG_ERROR, "Operator line must start with an operator identifier.", line->sourceFileName, line->lineNumber);
                        return;
                    }
                }
            }
        }
    }

    CPMFunctionSemanticBlock::CPMFunctionSemanticBlock(CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction) : CPMSemanticBlock(syntaxNode, ownerFunction)
    {
    }

    // Semantic tree constructor
    CPMSemanticOperator::CPMSemanticOperator(CPMFunctionSymbol* proto, CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode)
        : CPMExecutableSemanticNode(parent, syntaxNode, parent->OwnerFunction())
    {
        signature = proto->signature;
        ns = proto->owner;
    }

    CPMOperator_Alloc::CPMOperator_Alloc(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode) : CPMExecutableSemanticNode(parent, syntaxNode)
    {
    
    }

    void CPMOperator_Alloc::GenerateCode(CPMRelativeCodeChunk& code)
    {

    }

    CPMIntermediate::CPMIntermediate()
    {
    }


    CPMIntermediate::~CPMIntermediate()
    {
    }

}