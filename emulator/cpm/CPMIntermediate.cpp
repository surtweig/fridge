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
        {
            if (merge[i] == nullptr)
                continue;
            size += merge[i]->size;
        }

        code = (FRIDGE_WORD*)malloc(size);
        assert(code);

        size_t offset = 0;

        //memcpy(mergedCode, code, size);
        //offset += size;
        
        for (int i = 0; i < merge.size(); ++i)
        {
            if (merge[i] == nullptr)
                continue;
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
        blockParent = dynamic_cast<CPMSemanticBlock*>(parent);
    }

    CPMExecutableSemanticNode::~CPMExecutableSemanticNode()
    {
        for (int i = 0; i < children.size(); ++i)
            delete children[i];
    }

    CPMSemanticBlock::CPMSemanticBlock(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode) :
        CPMExecutableSemanticNode(parent, syntaxNode, parent->OwnerFunction())
    {
        stackOffset = 0;
        procreate();
    }

    CPMSemanticBlock::CPMSemanticBlock(CPMSyntaxTreeNode* syntaxNode, CPMFunctionSymbol* ownerFunction) :
        CPMExecutableSemanticNode(NULL, syntaxNode, ownerFunction)
    {
        stackOffset = 0;
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

                        bool isPtr = false;
                        CPMDataType dataType = OwnerFunction()->compiler->resolveDataTypeName(
                            opname->text,
                            isPtr,
                            OwnerFunction()->compiler->getSourceFile(line->sourceFileName),
                            OwnerFunction()->owner);

                        if (dataType > 0)
                        {
                            children.push_back(new CPMOperator_Alloc(dataType, isPtr, this, line));
                        }
                        else if (dataType == CPM_DATATYPE_AMBIGUOUS)
                        {
                            CompilerLog()->Add(LOG_ERROR, "Type reference '" + opname->text + "' is ambiguous in this context. See the message above.", opname->sourceFileName, opname->lineNumber);
                            Error();
                            return;
                        }
                        else if (dataType == CPM_DATATYPE_VOID)
                        {
                            CompilerLog()->Add(LOG_ERROR, "Void type is not allowed for local symbols.", opname->sourceFileName, opname->lineNumber);
                            Error();
                            return;
                        }
                        else
                        {
                            if (opname->text == OP_ASSIGN)
                            {
                                children.push_back(new CPMOperator_Assign(this, line));
                            }
                        }
                    }
                    else
                    {
                        CompilerLog()->Add(LOG_ERROR, "Operator line must start with an operator identifier.", line->sourceFileName, line->lineNumber);
                        Error();
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

            AsmLog()->Tab()->Add("ReserveLocals ")->Add(pushOffset)->Endl();

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

            AsmLog()->Tab()->Add("FlushLocals ")->Add(popOffset)->Endl();

            CPMRelativeCodeChunk* cc = new CPMRelativeCodeChunk(ccs);
            for (int i = 0; i < ccs.size(); ++i)
                delete ccs[i];

            return cc;
        }
        else
        {
            CompilerLog()->Add(LOG_ERROR, "Block local data size exceeds MaxDataSize.", SyntaxNode()->sourceFileName, SyntaxNode()->lineNumber);
            Error();
            return nullptr;
        }
    }

    CPMDataSymbol* CPMSemanticBlock::resolveLocalSymbolName(const string& name)
    {
        auto i = locals.find(name);
        CPMDataSymbol* symbol = nullptr;
        if (i == locals.end())
        {
            if (blockParent != nullptr)
                symbol = blockParent->resolveLocalSymbolName(name);
        }
        else
            symbol = i->second;
        return symbol;
    }

    CPMFunctionSemanticBlock::CPMFunctionSemanticBlock(CPMFunctionSymbol* ownerFunction)
        : CPMSemanticBlock(ownerFunction->bodyNode, ownerFunction)
    {
        for (int i = 0; i < ownerFunction->arguments.size(); ++i)
            locals[ownerFunction->arguments[i].name] = &ownerFunction->arguments[i];

        AsmLog()->Endl()->Add(ownerFunction->signature.name)->Add(" at ")->AddHex(ownerFunction->globalAddress)->Add(" ( ");
        for (int argi = 0; argi < ownerFunction->signature.arguments.size(); ++argi)
            AsmLog()->Add(ownerFunction->signature.arguments[argi].name)->Add(" ");
        AsmLog()->Add(")\n");
    }

    // Semantic tree constructor
    CPMSemanticOperator::CPMSemanticOperator(CPMFunctionSymbol* proto, CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode)
        : CPMExecutableSemanticNode(parent, syntaxNode, parent->OwnerFunction())
    {
        signature = proto->signature;
        ns = proto->owner;
    }

    CPMOperator_Alloc::CPMOperator_Alloc(CPMDataType dataType, bool isPtr, CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode)
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
                    Error();
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
                        symbol->isPtr = isPtr;
                        symbol->offset = blockParent->stackOffset;
                        if (arraySizeNode)
                            symbol->count = OwnerFunction()->compiler->parseArraySizeDecl(arraySizeNode, OwnerFunction()->owner);
                        else
                            symbol->count = 1;
                        if (valueNode)
                        {
                            OwnerFunction()->compiler->parseLiteralValue(symbol, valueNode);
                        }
                        blockParent->stackOffset += OwnerFunction()->compiler->sizeOfData(symbol);
                    }
                    else
                    {
                        CompilerLog()->Add(LOG_ERROR, "Local symbol '" + nameNode->text + "' is already declared.", syntaxNode->sourceFileName, syntaxNode->lineNumber);
                        Error();
                        return;
                    }
                }
                else
                {
                    CompilerLog()->Add(LOG_ERROR, "Local symbol name cannot contain namespace references.", syntaxNode->sourceFileName, syntaxNode->lineNumber);
                    Error();
                    return;
                }
            }
            else
            {
                CompilerLog()->Add(LOG_ERROR, "Local symbol name must be an identificator.", syntaxNode->sourceFileName, syntaxNode->lineNumber);
                Error();
                return;
            }
        }
        else
        {
            CompilerLog()->Add(LOG_ERROR, "Invalid local symbol declaration syntax.", syntaxNode->sourceFileName, syntaxNode->lineNumber);
            Error();
            return;
        }
    }

    CPMRelativeCodeChunk* CPMOperator_Alloc::GenerateCode()
    {
        return nullptr;
    }

    CPMOperator_Assign::CPMOperator_Assign(CPMExecutableSemanticNode* parent, CPMSyntaxTreeNode* syntaxNode)
        : CPMExecutableSemanticNode(parent, syntaxNode, parent->OwnerFunction())
    {
        source = nullptr;
        destination = nullptr;
        srcExpr = nullptr;
        destExpr = nullptr;
        literalSource = false;
        staticDest = false;
        procreate();
    }

    CPMRelativeCodeChunk* CPMOperator_Assign::GenerateCode()
    {
        CPM_SEMANTIC_ASSERT(destination || destExpr);
        CPM_SEMANTIC_ASSERT(source || srcExpr);
        vector<CPMRelativeCodeChunk*> ccs;

        for (int i = 0; i < children.size(); ++i)
            ccs.push_back(children[i]->GenerateCode());

        CPMRelativeCodeChunk* ccAssign = nullptr;
        if ((destination->type >= CPM_DATATYPE_BOOL &&
            destination->type <= CPM_DATATYPE_STRING) || destination->isPtr)
        {
            int destSize = OwnerFunction()->compiler->sizeOfData(destination);
            CPM_SEMANTIC_ASSERT_MESSAGE(destSize == 1 || destSize == 2, "Simple type invalid data size");
            AsmLog()->Tab()->Add("Assign simple type");
            if (!staticDest)
            {
                AsmLog()->Add(" local ")->Add(OwnerFunction()->compiler->GetTypeName(destination->type))->Add(" ")->Add(destination->name);
                if (literalSource)
                {
                    AsmLog()->Add(" to literal ");
                    if (destSize == 1)
                    {
                        FRIDGE_WORD literalVal = (FRIDGE_WORD)source->data[0];
                        AsmLog()->Add(literalVal);
                        FRIDGE_DWORD offset = ~(FRIDGE_DWORD)(blockParent->StackOffset() - destination->offset) + 1;
                        ccAssign = new CPMRelativeCodeChunk(
                            {
                                HLSP,
                                DAI, FRIDGE_HIGH_WORD(offset), FRIDGE_LOW_WORD(offset),
                                MVI_M, literalVal
                            }
                        );
                    }
                    else if (destSize == 2)
                    {
                        FRIDGE_DWORD literalVal = (FRIDGE_DWORD)source->data[0];
                        AsmLog()->Add(literalVal);
                        FRIDGE_DWORD offset = ~(FRIDGE_DWORD)(blockParent->StackOffset() - destination->offset) + 1;
                        ccAssign = new CPMRelativeCodeChunk(
                            {
                                HLSP,
                                DAI, FRIDGE_HIGH_WORD(offset), FRIDGE_LOW_WORD(offset),
                                MVI_M, FRIDGE_HIGH_WORD(literalVal),
                                INX_HL,
                                MVI_M, FRIDGE_LOW_WORD(literalVal)
                            }
                        );
                    }
                    
                }
            }
        }
        ccs.push_back(ccAssign);
        AsmLog()->Endl();

        CPMRelativeCodeChunk* cc = new CPMRelativeCodeChunk(ccs);
        for (int i = 0; i < ccs.size(); ++i)
            delete ccs[i];

        return cc;
    }

    void CPMOperator_Assign::procreate()
    {
        CPM_SEMANTIC_ASSERT(blockParent);
        staticDest = false;
        literalSource = false;
        CPMSyntaxTreeNode* node = SyntaxNode();
        if (node->children.size() == 3)
        {
            CPMSyntaxTreeNode* destNode = node->children[1];
            CPMSyntaxTreeNode* sourceNode = node->children[2];

            if (destNode->type == CPM_ID)
            {
                destination = blockParent->resolveLocalSymbolName(destNode->text);
                if (destination == nullptr)
                {
                    CPMStaticSymbol* ss = OwnerFunction()->compiler->resolveStaticSymbolName(
                        destNode->text,
                        OwnerFunction()->compiler->getSourceFile(node->sourceFileName), destNode,
                        OwnerFunction()->owner);
                    if (ss != nullptr)
                    {
                        if (!ss->isconst)
                        {
                            destination = &ss->field;
                            staticDest = true;
                        }
                        else
                        {
                            CompilerLog()->Add(LOG_ERROR, "Symbol '"+destNode->text+"' is constant.", destNode->sourceFileName, destNode->lineNumber);
                            Error();
                            return;
                        }
                    }
                    else
                    {
                        CompilerLog()->Add(LOG_ERROR, "Symbol '" + destNode->text + "' is not declared or ambiguous in this context.", destNode->sourceFileName, destNode->lineNumber);
                        Error();
                        return;
                    }
                }
                if (destination == nullptr)
                {
                    CompilerLog()->Add(LOG_ERROR, "Undefined symbol '" + destNode->text + "'.", destNode->sourceFileName, destNode->lineNumber);
                    Error();
                    return;
                }
                else if (destination->count > 1)
                {
                    CompilerLog()->Add(LOG_ERROR, "Implicit array assignment is not implemented.", destNode->sourceFileName, destNode->lineNumber);
                    Error();
                    return;
                }
            }
            else
            {
                CompilerLog()->Add(LOG_ERROR, "Assign destination must be an identifier.", destNode->sourceFileName, destNode->lineNumber);
                Error();
                return;
            }

            if (sourceNode->type == CPM_ID)
            {
                source = blockParent->resolveLocalSymbolName(sourceNode->text);
                if (source == nullptr)
                {
                    CPMStaticSymbol* ss = OwnerFunction()->compiler->resolveStaticSymbolName(
                        sourceNode->text,
                        OwnerFunction()->compiler->getSourceFile(node->sourceFileName), sourceNode,
                        OwnerFunction()->owner);
                    if (ss != nullptr)
                    {
                        source = &ss->field;
                    }
                    else
                    {
                        CompilerLog()->Add(LOG_ERROR, "Symbol '" + sourceNode->text + "' is not declared or ambiguous in this context.", sourceNode->sourceFileName, sourceNode->lineNumber);
                        Error();
                        return;
                    }
                }
                if (source == nullptr)
                {
                    CompilerLog()->Add(LOG_ERROR, "Undefined symbol '" + sourceNode->text + "'.", sourceNode->sourceFileName, sourceNode->lineNumber);
                    Error();
                    return;
                }
            }
            else if (sourceNode->type == CPM_NUM)
            {
                source = new CPMDataSymbol();
                source->count = 1;
                source->data.resize(1);
                blockParent->addLiteral(source);
                if (OwnerFunction()->compiler->parseLiteralNumber(source, sourceNode, 0, true))
                {
                    literalSource = true;
                }
                else
                {
                    CompilerLog()->Add(LOG_ERROR, "Cannot parse number literal '" + sourceNode->text + "'.", sourceNode->sourceFileName, sourceNode->lineNumber);
                    Error();
                    return;
                }
            }
            else if (sourceNode->type == CPM_EXPR)
            {
                source = new CPMDataSymbol();
                blockParent->addLiteral(source);
                if (OwnerFunction()->compiler->parseLiteralNumber(source, sourceNode, 0, true))
                {
                    literalSource = true;
                }
                else
                {

                }
            }
            else
            {
                CompilerLog()->Add(LOG_ERROR, "Assign source must be an identifier, number literal or an expression.", sourceNode->sourceFileName, sourceNode->lineNumber);
                Error();
                return;
            }
            
        }
        else
        {
            CompilerLog()->Add(LOG_ERROR, "Invalid '=' operator syntax.", node->sourceFileName, node->lineNumber);
            Error();
        }
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
            compiler->AsmDebugOutput()->Add("\n----- ")->Add(nslist[nsi]->name)->Add(":\n");
            for (auto fi = nslist[nsi]->functions.begin(); fi != nslist[nsi]->functions.end(); ++fi)
            {
                CPMFunctionSymbol* funsym = &fi->second;
                funsym->globalAddress = offset;
                
                CPMFunctionSemanticBlock* funblock = new CPMFunctionSemanticBlock(funsym);
                if (!compiler->NoErrors())
                    break;
                CPMRelativeCodeChunk* funcode = funblock->GenerateCode();
                ccs.push_back(funcode);
                delete funblock;
                offset += funcode->size;
            }
            if (!compiler->NoErrors())
                break;
        }
        compiler->AsmDebugOutput()->Add("\n----- Static data:\n");
        vector<FRIDGE_WORD> staticData;
        for (int nsi = 0; nsi < nslist.size(); ++nsi)
        {
            for (auto si = nslist[nsi]->statics.begin(); si != nslist[nsi]->statics.end(); ++si)
            {
                if (si->second.importSource < 0 && !si->second.isconst)
                {
                    si->second.field.globalAddress = offset;
                    offset += si->second.field.serialize(staticData);
                    compiler->AsmDebugOutput()->Tab()->Add(nslist[nsi]->name)->Add(".")->Add(si->second.field.name)->
                        Add(" ")->Add(offset - si->second.field.globalAddress)->Add(" bytes at ")->AddHex(si->second.field.globalAddress)->Add("\n");
                }
            }
        }
        CPMRelativeCodeChunk* staticscc = new CPMRelativeCodeChunk(staticData);
        ccs.push_back(staticscc);
        objectCode = new CPMRelativeCodeChunk(ccs);

        if (compiler->NoErrors())
        {
            ofstream myfile(compiler->getOutputFileName(), ios::out | ios::binary);
            myfile.write((char*)objectCode->code, objectCode->size * sizeof(FRIDGE_WORD));
            myfile.close();
            compiler->CompilerLog()->Add(LOG_MESSAGE, "Compiled program is saved to " + compiler->getOutputFileName());
        }
    }


    CPMIntermediate::~CPMIntermediate()
    {
        delete objectCode;
    }

}