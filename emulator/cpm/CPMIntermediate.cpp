#include "stdafx.h"
#include "CPMIntermediate.h"
#include "CPMCompiler.h"
#include <iostream>
#include <fstream>

namespace CPM
{

    CPMRelativeCodeChunk::CPMRelativeCodeChunk()
    {
        code = NULL;
        size = 0;
    }

    CPMRelativeCodeChunk::CPMRelativeCodeChunk(size_t size)
    {
        this->size = size;
        code = (FRIDGE_WORD*)malloc(size);
        assert(code);
        for (size_t i = 0; i < size; ++i)
            code[i] = 0;
    }

    CPMRelativeCodeChunk::CPMRelativeCodeChunk(vector<FRIDGE_WORD> buffer)
    {
        size = buffer.size();
        code = (FRIDGE_WORD*)malloc(size);
        assert(code);
        for (size_t i = 0; i < size; ++i)
            code[i] = buffer[i];
    }

    CPMRelativeCodeChunk::CPMRelativeCodeChunk(vector<CPMRelativeCodeChunk*> merge)
    {
        size = 0;

        for (int i = 0; i < merge.size(); ++i)
            size += merge[i]->size;

        code = (FRIDGE_WORD*)malloc(size);
        assert(code);

        size_t offset = 0;

        //memcpy(mergedCode, code, size);
        //offset += size;
        
        for (int i = 0; i < merge.size(); ++i)
        {
            if (merge[i]->code == nullptr)
                continue;

            for (map<int, CPMStaticSymbol*>::iterator ssi = merge[i]->staticReferences.begin(); ssi != merge[i]->staticReferences.end(); ++ssi)
                staticReferences[ssi->first + offset] = ssi->second;

            for (map<int, CPMFunctionSymbol*>::iterator fsi = merge[i]->functionReferences.begin(); fsi != merge[i]->functionReferences.end(); ++fsi)
                functionReferences[fsi->first + offset] = fsi->second;

            memcpy(code + offset, merge[i]->code, merge[i]->size);

            for (int ir = 0; ir < merge[i]->internalReferences.size(); ++ir)
            {
                int relPos = merge[i]->internalReferences[ir];
                FRIDGE_DWORD ref = FRIDGE_DWORD_HL(merge[i]->code[relPos], merge[i]->code[relPos + 1]);
                internalReferences.push_back(offset + relPos);
                ref += offset;
                code[offset + relPos] = FRIDGE_HIGH_WORD(ref);
                code[offset + relPos + 1] = FRIDGE_LOW_WORD(ref);
            }

            offset += merge[i]->size;
        }
    }

    CPMRelativeCodeChunk::~CPMRelativeCodeChunk()
    {
        free(code);
    }

    void CPMRelativeCodeChunk::write(vector<FRIDGE_WORD> buffer, size_t offset)
    {
        assert(offset + buffer.size() < size);
        for (size_t i = 0; i < buffer.size(); ++i)
            code[i + offset] = buffer[i];
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

    CPMSemanticBlock::CPMSemanticBlock(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode) :
        CPMExecutableSemanticNode(parent, syntaxNode, parent->OwnerFunction())
    {
        procreate();
    }

    CPMSemanticBlock::CPMSemanticBlock(CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction) :
        CPMExecutableSemanticNode(NULL, syntaxNode, ownerFunction)
    {
        procreate();
    }

    CPMSemanticBlock::~CPMSemanticBlock()
    {
        //for (auto i = locals.begin(); i != locals.end(); ++i)
        //    delete i->second;
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

    CPMRelativeCodeChunk* CPMSemanticBlock::GenerateCode()
    {
        FRIDGE_SIZE_T localsSize = 0;
        for (auto i = locals.begin(); i != locals.end(); ++i)
            localsSize += OwnerFunction()->compiler->sizeOfData(i->second);
        if (localsSize <= DataMaxSize)
        {
            FRIDGE_DWORD stackSize = localsSize;
#ifdef FRIDGE_ASCENDING_STACK
            FRIDGE_DWORD pushOffset = stackSize;
            FRIDGE_DWORD popOffset = ~stackSize + 1;
#else
            FRIDGE_DWORD pushOffset = ~stackSize + 1; 
            FRIDGE_DWORD popOffset = stackSize;
#endif
            vector<CPMRelativeCodeChunk*> ccs;

            CPMRelativeCodeChunk* ccReserveLocals = new CPMRelativeCodeChunk(
                {
                    LXI_HL, FRIDGE_HIGH_WORD(pushOffset), FRIDGE_LOW_WORD(pushOffset),
                    DAD_SP
                }
            );
            ccs.push_back(ccReserveLocals);

            for (int i = 0; i < children.size(); ++i)
            {
                CPMRelativeCodeChunk* childcc = children[i]->GenerateCode();
                if (childcc)
                    ccs.push_back(childcc);
            }

            CPMRelativeCodeChunk* ccFlushLocals = new CPMRelativeCodeChunk(
                {
                    LXI_HL, FRIDGE_HIGH_WORD(popOffset), FRIDGE_LOW_WORD(popOffset),
                    DAD_SP
                }
            );
            ccs.push_back(ccFlushLocals);

            CPMRelativeCodeChunk* cc = new CPMRelativeCodeChunk(ccs);
            for (int i = 0; i < ccs.size(); ++i)
                delete ccs[i];

            return cc;
        }
        else
        {
            CompilerLog()->Add(LOG_ERROR, "Block local data size exceeds MaxDataSize.", SyntaxNode()->sourceFileName, SyntaxNode()->lineNumber);
            return nullptr;
        }
    }

    CPMFunctionSemanticBlock::CPMFunctionSemanticBlock(CPMFunctionSymbol* ownerFunction)
        : CPMSemanticBlock(ownerFunction->bodyNode, ownerFunction)
    {
        for (int i = 0; i < ownerFunction->arguments.size(); ++i)
            locals[ownerFunction->arguments[i].name] = &ownerFunction->arguments[i];

        ownerFunction->compiler->AsmDebugOutput()->Add(ownerFunction->signature.name)->Add(" at ")->AddHex(ownerFunction->globalAddress)->Add(" ( ");
        for (int argi = 0; argi < ownerFunction->signature.arguments.size(); ++argi)
            ownerFunction->compiler->AsmDebugOutput()->Add(ownerFunction->signature.arguments[argi].name)->Add(" ");
        ownerFunction->compiler->AsmDebugOutput()->Add(")\n");
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
                    auto i = blockParent->locals.find(nameNode->text);
                    if (i == blockParent->locals.end())
                    {
                        CPMDataSymbol* symbol = new CPMDataSymbol();
                        blockParent->locals[nameNode->text] = symbol;
                        symbol->name = nameNode->text;
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
                        CompilerLog()->Add(LOG_ERROR, "Local symbol '" + nameNode->text + "' is already declared.", syntaxNode->sourceFileName, syntaxNode->lineNumber);
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

    CPMRelativeCodeChunk* CPMOperator_Alloc::GenerateCode()
    {
        return nullptr;
    }

    CPMIntermediate::CPMIntermediate(CPMCompiler* compiler)
    {
        vector<CPMNamespace*> nslist;
        compiler->getNamespaces(nslist);

        vector<CPMRelativeCodeChunk*> ccs;

        FRIDGE_RAM_ADDR offset = compiler->getGlobalOffset();
        compiler->AsmDebugOutput()->Add("globalOffset = ")->AddHex(offset)->Add("\n");

        for (int nsi = 0; nsi < nslist.size(); ++nsi)
        {
            compiler->AsmDebugOutput()->Add("\n")->Add(nslist[nsi]->name)->Add(":\n\n");
            for (auto fi = nslist[nsi]->functions.begin(); fi != nslist[nsi]->functions.end(); ++fi)
            {
                CPMFunctionSymbol* funsym = &fi->second;
                funsym->globalAddress = offset;
                
                CPMFunctionSemanticBlock* funblock = new CPMFunctionSemanticBlock(funsym);
                CPMRelativeCodeChunk* funcode = funblock->GenerateCode();
                ccs.push_back(funcode);
                delete funblock;
                offset += funcode->size;
            }
        }
        compiler->AsmDebugOutput()->Add("\nStatic data:\n");
        vector<FRIDGE_WORD> staticData;
        for (int nsi = 0; nsi < nslist.size(); ++nsi)
        {
            for (auto si = nslist[nsi]->statics.begin(); si != nslist[nsi]->statics.end(); ++si)
            {
                if (si->second.importSource < 0 && !si->second.isconst)
                {
                    si->second.field.globalAddress = offset;
                    offset += si->second.field.serialize(staticData);
                    compiler->AsmDebugOutput()->Add(nslist[nsi]->name)->Add(".")->Add(si->second.field.name)->
                        Add(" ")->Add(offset - si->second.field.globalAddress)->Add(" bytes at ")->AddHex(si->second.field.globalAddress)->Add("\n");
                }
            }
        }
        CPMRelativeCodeChunk* staticscc = new CPMRelativeCodeChunk(staticData);
        ccs.push_back(staticscc);
        objectCode = new CPMRelativeCodeChunk(ccs);

        ofstream myfile(compiler->getOutputFileName(), ios::out | ios::binary);
        myfile.write((char*)objectCode->code, objectCode->size * sizeof(FRIDGE_WORD));
        myfile.close();
        compiler->CompilerLog()->Add(LOG_MESSAGE, "Compiled program is saved to " + compiler->getOutputFileName());
    }


    CPMIntermediate::~CPMIntermediate()
    {
        delete objectCode;
    }

}