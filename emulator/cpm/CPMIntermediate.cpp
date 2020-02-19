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

        FRIDGE_WORD* mergedCode = (FRIDGE_WORD*)malloc(totalSize);
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
                FRIDGE_DWORD ref = FRIDGE_DWORD_HL(merge[i]->code[relPos], merge[i]->code[relPos + 1]);
                internalReferences.push_back(offset + relPos);
                ref += offset;
                mergedCode[offset + relPos] = FRIDGE_HIGH_WORD(ref);
                mergedCode[offset + relPos + 1] = FRIDGE_LOW_WORD(ref);
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

                        CPMDataType dataType = OwnerFunction()->compiler->resolveDataTypeName(
                            opname->text,
                            OwnerFunction()->compiler->getSourceFile(line->sourceFileName),
                            OwnerFunction()->owner);

                        if (dataType > 0)
                        {
                            children.push_back(new CPMOperator_Alloc(dataType, this, line));
                        }
                        else if (dataType == CPM_DATATYPE_AMBIGUOUS)
                        {
                            CompilerLog()->Add(LOG_ERROR, "Type reference '" + opname->text + "' is ambiguous in this context. See the message above.", opname->sourceFileName, opname->lineNumber);
                            return;
                        }
                        else if (dataType == CPM_DATATYPE_VOID)
                        {
                            CompilerLog()->Add(LOG_ERROR, "Void type is not allowed for local symbols.", opname->sourceFileName, opname->lineNumber);
                            return;
                        }
                        else
                        {

                        }
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

    CPMOperator_Alloc::CPMOperator_Alloc(CPMDataType dataType, CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode)
        : CPMExecutableSemanticNode(parent, syntaxNode, parent->OwnerFunction())
    {
        CPM_SEMANTIC_ASSERT(parent->SyntaxNode()->type == CPM_BLOCK);
        
        if (syntaxNode->children.size() >= 2 || syntaxNode->children.size() <= 5)
        {
            CPMSemanticBlock* blockParent = (CPMSemanticBlock*)parent;
            CPMSyntaxTreeNode* nameNode = syntaxNode->children[1];
            CPMSyntaxTreeNode* valueNode = nullptr;
            CPMSyntaxTreeNode* arraySizeNode = nullptr;
            
            if (syntaxNode->children.size() == 3)
                valueNode = syntaxNode->children[2];
            else if (syntaxNode->children.size() == 5)
                valueNode = syntaxNode->children[4];

            if (syntaxNode->children.size() > 3)
            {
                if (syntaxNode->children[2]->text == R_ARRAY)
                {
                    arraySizeNode = syntaxNode->children[3];
                }
                else
                {
                    CompilerLog()->Add(LOG_ERROR, "Missing array keyword for local array symbol declaration.", syntaxNode->sourceFileName, syntaxNode->lineNumber);
                    return;
                }
            }

            if (nameNode->type == CPM_ID)
            {
                vector<string> parsedName = CPMCompiler::ParseSymbolName(nameNode->text);
                if (parsedName.size() == 1)
                {
                    blockParent->locals[nameNode->text].name = nameNode->text;
                    CPMDataSymbol* symbol = &blockParent->locals[nameNode->text];
                    symbol->type = dataType;
                    if (arraySizeNode)
                        symbol->count = OwnerFunction()->compiler->parseArraySizeDecl(arraySizeNode, OwnerFunction()->owner);
                    else
                        symbol->count = 1;
                    if (valueNode)
                    {
                        OwnerFunction()->compiler->parseLiteralValue(symbol, valueNode);
                    }
                }
                else
                    CompilerLog()->Add(LOG_ERROR, "Local symbol name cannot contain namespace references.", syntaxNode->sourceFileName, syntaxNode->lineNumber);
            }
            else
                CompilerLog()->Add(LOG_ERROR, "Local symbol name must be an identificator.", syntaxNode->sourceFileName, syntaxNode->lineNumber);
        }
        else
            CompilerLog()->Add(LOG_ERROR, "Invalid local symbol declaration syntax.", syntaxNode->sourceFileName, syntaxNode->lineNumber);
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