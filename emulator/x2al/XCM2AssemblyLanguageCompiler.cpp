#include "stdafx.h"
#include "XCM2AssemblyLanguageCompiler.h"
#include <stdlib.h>

#define X2AL_ERR_LOC "[")->Append(currentSourceFile)->Append(" : ")->Append(currentLineNumber)->Append("] "

XCM2AssemblyLanguageCompiler::XCM2AssemblyLanguageCompiler(string sourceRootFolder, string sourceFileName, string outputFile, vector<string> includeFolders, ITextStreamWrapper* errstr, bool saveVHDL)
{
    this->errstr = errstr;
    this->includeFolders = includeFolders;
    currentLineNumber = -1;
    currentSourceFile = "none";
    offset = 0;
    mainEntry = 0;
    programSize = 0;
    resOrigin = -1;
    objectCode = nullptr;
    unsafeFlowAllowed = false;

    KeywordIDs.clear();
    KeywordIDs.emplace(R_INCLUDE);
    KeywordIDs.emplace(R_ALIAS);
    KeywordIDs.emplace(R_SUBROUTINE);
    KeywordIDs.emplace(R_ENTRY);
    KeywordIDs.emplace(R_MAIN);
    KeywordIDs.emplace(R_OFFSET);
    KeywordIDs.emplace(R_STATICRESOURCE);
    KeywordIDs.emplace(R_UNSAFE_FLOW);

    bool noerrors = false;
    if (preprocessFile(sourceRootFolder, sourceFileName))
    {
        initDefaultAliases();
        if (readResources())
            if (dealias())
                if (addressMarkup())
                    if (generateObjectCode())
                    {
                        saveObjectCode(outputFile);
                        if (saveVHDL)
                            saveObjectCodeVHDL(outputFile + ".vhd");
                        noerrors = true;
                        logout("Compilation successful.");
                    }
    }
    if (!noerrors)
    {
        logerrout("Compilation failed due to errors. No code generated.");
    }
}

bool XCM2AssemblyLanguageCompiler::preprocessFile(string sourceRootFolder, string sourceFileName)
{
    string line;
    ifstream f(sourceRootFolder + sourceFileName);
    int lineNumberBackup;
    string actualSourceRootFolder = sourceRootFolder;

    // try to open from include folders
    if (!f.is_open())
        for (int i = 0; i < includeFolders.size(); i++)
        {
            actualSourceRootFolder = includeFolders[i];
            f.clear();
            f.open(actualSourceRootFolder + sourceFileName);
            if (f.is_open())
                break;
        }

    // last try
    if (!f.is_open())
    {
        actualSourceRootFolder = STD_PATH;
        f.clear();
        f.open(actualSourceRootFolder + sourceFileName);
    }

    if (f.is_open())
    {
        currentLineNumber = 1;
        currentSourceFile = sourceFileName;
        errstr->Append(X2AL_ERR_LOC)->Append(" Source: ")->Append(actualSourceRootFolder + sourceFileName)->Append("\n");
        while (getline(f, line))
        {
            ParsedLine parsed;
            parseLine(&line, parsed.words);
            bool wasInclude = false;
            if (parsed.words.size() == 2)
                if (parsed.words[0] == R_INCLUDE)
                {
                    string incfile = parsed.words[1].substr(1, parsed.words[1].size() - 2);
                    unordered_set<string>::iterator iinc = includes.find(incfile);
                    if (iinc == includes.end())
                    {
                        lineNumberBackup = currentLineNumber;
                        preprocessFile(actualSourceRootFolder, incfile);
                        currentSourceFile = sourceFileName;
                        currentLineNumber = lineNumberBackup;
                        wasInclude = true;
                        includes.emplace(incfile);
                    }
                    else
                        logwarnout("File " + incfile + " has already been included.");
                }

            if (!wasInclude && parsed.words.size() > 0)
            {
                parsed.lineNumber = currentLineNumber;
                parsed.sourceFile = currentSourceFile;
                lines.push_back(parsed);
            }
            currentLineNumber++;
        }
        f.close();
        return true;
    }
    else
    {
        errstr->Append(X2AL_ERR_LOC)->Append("[ERROR]   Cannot open file '")->Append(sourceFileName)->Append("'. \n");
        return false;
    }
}

bool XCM2AssemblyLanguageCompiler::saveObjectCode(string outputFile)
{
    ofstream myfile(outputFile, ios::out | ios::binary);
    myfile.write((char*)objectCode, programSize * sizeof(XCM2_WORD));
    myfile.close();
    logout("Compiled program is saved to " + outputFile);
    return true;
}

bool XCM2AssemblyLanguageCompiler::saveObjectCodeVHDL(string outputFile)
{
    ofstream myfile(outputFile, ios::out);
    for (int i = 0; i < programSize * sizeof(XCM2_WORD); i++)
    {
        myfile << int_to_hex_vhd((XCM2_WORD)objectCode[i]) << ", ";
        if ((i + 1) % 16 == 0)
            myfile << "\n";
    }
    myfile << "\nothers => X\"00\"";
    myfile.close();
    return true;
}

void XCM2AssemblyLanguageCompiler::parseLine(string* line, vector<string> &words)
{
    int tStart = 0;
    int tFinish = 0;
    bool strLiteral = false;
    bool resLiteral = false;
    bool isTerminal = false;
    int commentSignCounter = 0;
    for (int i = 0; i < line->length(); i++)
    {
        char c = (*line)[i];

        // check for comment line
        if (c == '/' && !strLiteral)
        {
            commentSignCounter++;
            if (commentSignCounter == 2)
                break;
        }
        else
        {
            commentSignCounter = 0;

            // quote encounter
            if (c == '"')
            {
                if (isTerminal)
                    errstr->Append("[ERROR]   ")->Append(X2AL_ERR_LOC)->Append("String literals must be separated from other terminals. \n");

                // string has ended
                if (strLiteral)
                {
                    strLiteral = false;
                    words.push_back(line->substr(tStart, i - tStart + 1));
                }
                else // string has started
                {
                    strLiteral = true;
                    tStart = i;
                }
            }
            // separator encounter
            else if ((c == ' ' || c == '\t' || c == ',' || c == ';') && !strLiteral)
            {
                // terminal has finished
                if (isTerminal)
                {
                    isTerminal = false;
                    words.push_back(line->substr(tStart, i - tStart));
                }
            }
            else if (!strLiteral)
            {
                // terminal has started
                if (!isTerminal)
                {
                    tStart = i;
                    isTerminal = true;
                }
            }
        }
    }
    if (isTerminal)
    {
        isTerminal = false;
        words.push_back(line->substr(tStart, line->length() - tStart));
    }
}

void XCM2AssemblyLanguageCompiler::logout(string message)
{
    errstr->Append(X2AL_ERR_LOC)->Append(message)->Append("\n");
}

void XCM2AssemblyLanguageCompiler::logerrout(string message)
{
    errstr->Append("[ERROR]   ")->Append(X2AL_ERR_LOC)->Append(message)->Append("\n");
}

void XCM2AssemblyLanguageCompiler::logwarnout(string message)
{
    errstr->Append("[WARNING] ")->Append(X2AL_ERR_LOC)->Append(message)->Append("\n");
}

void XCM2AssemblyLanguageCompiler::initDefaultAliases()
{
    for (map<string, XCM2_WORD>::iterator iir = IRIDs.begin(); iir != IRIDs.end(); ++iir)
    {
        aliases[IRCodeAliasPrefix + iir->first] = to_string(iir->second);
    }
}

bool XCM2AssemblyLanguageCompiler::readResources()
{
    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;
        if (iline->words[0] == R_STATICRESOURCE)
        {
            if (iline->words.size() == 3)
            {
                map<string, StaticResourceInfo>::iterator res = resources.find(iline->words[1]);
                if (res != resources.end())
                    logwarnout("Resource '" + iline->words[1] + "' has already been declared.");

                XCM2_DWORD resSize = 0;
                if (iline->words[2][0] == '"')
                {
                    resSize = iline->words[2].size() - 1;
                    XCM2_WORD* pres = (XCM2_WORD*)malloc(resSize);
                    memcpy(pres, iline->words[2].c_str() + 1, resSize - 1);
                    pres[resSize - 1] = 0;
                    //for (int i = 0; i < resSize-1; i++)
                    //    pres[i] = iline->words[1][i+1];
                    //pres[resSize-1] = 0; // null terminated string
                    resources[iline->words[1]].pdata = pres;
                    resources[iline->words[1]].size = resSize;
                }
                XCM2_DWORD dwr;
                XCM2_WORD wr;
                if (parseHexDouble(&(iline->words[2]), dwr))
                {
                    resSize = 2;
                    XCM2_DWORD* pres = (XCM2_DWORD*)malloc(resSize);
                    pres[0] = dwr;
                    resources[iline->words[1]].pdata = (XCM2_WORD*)pres;
                    resources[iline->words[1]].size = resSize;
                }
                if (parseChar(&(iline->words[2]), wr) || parseHex(&(iline->words[2]), wr) || parseDec(&(iline->words[2]), wr))
                {
                    resSize = 1;
                    XCM2_WORD* pres = (XCM2_WORD*)malloc(resSize);
                    pres[0] = wr;
                    resources[iline->words[1]].pdata = pres;
                    resources[iline->words[1]].size = resSize;
                }

                aliases[iline->words[1] + ResourceSizePostfix] = int_to_hex(resSize);
            }
            else
            {
                logerrout("Wrong resource declaration syntax.");
                return false;
            }
        }
    }
    return true;
}

bool XCM2AssemblyLanguageCompiler::dealias()
{
    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;
        if (iline->words[0] == R_ALIAS)
        {
            if (iline->words.size() == 3)
            {
                map<string, string>::iterator alias = aliases.find(iline->words[1]);
                if (alias != aliases.end())
                    logwarnout("Alias '" + iline->words[1] + "' has already been declared.");
                aliases[iline->words[1]] = iline->words[2];
            }
            else
            {
                logerrout("Wrong alias declaration syntax.");
                return false;
            }
        }
    }

    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;
        for (vector<string>::iterator iword = iline->words.begin(); iword != iline->words.end(); ++iword)
        {
            map<string, string>::iterator alias = aliases.find(*iword);
            if (alias != aliases.end())
                (*iword) = alias->second;
        }
    }

    return true;
}

void XCM2AssemblyLanguageCompiler::printParsed()
{
    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;
        errstr->Append(X2AL_ERR_LOC);
        for (vector<string>::iterator iword = iline->words.begin(); iword != iline->words.end(); ++iword)
        {
            errstr->Append((*iword))->Append(" ");
        }
        errstr->Append("\n");
    }
}

void XCM2AssemblyLanguageCompiler::printCompiled()
{
    printCompiled(errstr);
}

void XCM2AssemblyLanguageCompiler::printCompiled(ITextStreamWrapper* f)
{
    if (objectCode)
    {
        int rowc = 0;
        int escounter = 0;
        for (int i = 0; i < programSize; i++)
        {
            //(*errstr) )->Append( int_to_hex((XCM2_DWORD)objectCode[i]) )->Append( " ";
            if (i < resOrigin)
            {
                if (escounter == 0)
                {
                    XCM2_WORD ircode = (XCM2_DWORD)objectCode[i];
                    map<XCM2_WORD, string>::iterator iname = IRNames.find(ircode);
                    f->Append("\n")->Append(i)->Append(":\t")->Append(iname->second);

                    for (; ircode >= 0; ircode--)
                    {
                        map<XCM2_WORD, InstructionSignature>::iterator isig = IRSigs.find(ircode);
                        if (isig != IRSigs.end())
                        {
                            escounter = IRSigs[ircode].extraSize;
                            break;
                        }
                    }

                }
                else
                {
                    f->Append(" ")->Append((XCM2_DWORD)objectCode[i]);
                    escounter--;
                }
                rowc++;
                /*if (rowc == 8)
                {
                (*errstr) )->Append( "\n";
                rowc = 0;
                }*/
            }
            else
            {
                f->Append("\n")->Append(i)->Append(":\t'")->Append(objectCode[i])->Append("' (")->Append((int)objectCode[i])->Append(")");
            }
        }
        f->Append("\n");
    }
}

bool XCM2AssemblyLanguageCompiler::parseDec(string* word, XCM2_WORD& value)
{
    if (word->size() > 3)
        return false;
    for (int i = 0; i < word->size(); i++)
        if ((*word)[i] < '0' || (*word)[i] > '9')
            return false;
    value = strtol(word->c_str(), nullptr, 10);
    return true;
}

bool XCM2AssemblyLanguageCompiler::parseHex(string* word, XCM2_WORD& value)
{
    if (word->size() != 4)
        return false;
    if ((*word)[0] != '0' && (*word)[1] != 'x')
        return false;
    value = strtol(word->c_str(), nullptr, 16);
    return true;
}

bool XCM2AssemblyLanguageCompiler::parseHexDouble(string* word, XCM2_DWORD& value)
{
    if (word->size() != 6)
        return false;
    if ((*word)[0] != '0' && (*word)[1] != 'x')
        return false;
    value = strtol(word->c_str(), nullptr, 16);
    return true;
}

bool XCM2AssemblyLanguageCompiler::parseChar(string* word, XCM2_WORD& value)
{
    if (word->size() != 3)
        return false;
    if ((*word)[0] != '\'' && (*word)[2] != '\'')
        return false;
    value = (XCM2_WORD)(*word)[1];
    return true;
}

template< typename T > string XCM2AssemblyLanguageCompiler::int_to_hex(T i)
{
    stringstream stream;
    stream << "0x"
        << setfill('0') << setw(sizeof(T) * 2)
        << hex << i;
    return stream.str();
}

string XCM2AssemblyLanguageCompiler::int_to_hex_vhd(XCM2_WORD i)
{
    stringstream stream;
    stream << "X\""
        << setfill('0') << setw(sizeof(XCM2_WORD) * 2)
        << hex << (int)i << "\"";
    return stream.str();
}

bool XCM2AssemblyLanguageCompiler::addressMarkup()
{
    bool noerrors = true;
    offset = 0;
    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;
        if (iline->words[0] == R_OFFSET)
        {
            if (!parseHexDouble(&iline->words[1], offset))
            {
                logerrout("Offset must be in double hex format.");
                noerrors = false;
            }
        }

        if (iline->words[0] == R_UNSAFE_FLOW)
        {
            unsafeFlowAllowed = true;
            logwarnout("Unsafe flow checks are disabled. Be certain of use of flow control instructions.");
        }
    }

    XCM2_RAM_ADDR pc = 0;
    bool mainEntryDeclared = false;

    bool subClosed = true;

    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;

        if (iline->words[0] == R_MAIN || iline->words[0] == R_ENTRY)
        {
            if (iline->words.size() == 2)
            {
                string entryname = iline->words[1];
                map<string, XCM2_RAM_ADDR>::iterator ientry = entries.find(entryname);
                if (ientry == entries.end())
                {
                    entries[entryname] = pc + offset;
                    //(*errstr) )->Append( X2AL_ERR_LOC )->Append( "Entry '" )->Append( entryname )->Append( "' declared at " )->Append( pc )->Append( "\n";
                    if (iline->words[0] == R_MAIN)
                    {
                        if (mainEntryDeclared)
                            logwarnout("Previous main entry is overridden with the entry '" + entryname + "'.");
                        mainEntry = pc + offset;
                        mainEntryDeclared = true;
                    }
                }
                else
                {
                    logerrout("Entry '" + entryname + "' has already been declared.");
                    noerrors = false;
                }
            }
            else
            {
                logerrout("Wrong entry declaration syntax.");
                noerrors = false;
            }
        }

        if (iline->words[0] == R_SUBROUTINE)
        {
            if (!subClosed)
            {
                logerrout("Subroutines cannot be declared within another.");
                noerrors = false;
            }

            subClosed = false;
            if (iline->words.size() == 2)
            {
                string subname = iline->words[1];
                map<string, XCM2_RAM_ADDR>::iterator isub = subroutines.find(subname);
                if (isub == subroutines.end())
                {
                    subroutines[subname] = pc + offset;
                    //(*errstr) )->Append( X2AL_ERR_LOC )->Append( "Subroutine '" )->Append( entryname )->Append( "' declared at " )->Append( pc )->Append( "\n";
                }
                else
                {
                    logerrout("Subroutine '" + subname + "' has already been declared.");
                    noerrors = false;
                }
            }
            else
            {
                logerrout("Wrong subroutine declaration syntax.");
                noerrors = false;
            }
        }

        unordered_set<string>::iterator ret_irid = RetIRIDs.find(iline->words[0]);
        if (ret_irid != RetIRIDs.end())
        {
            if (subClosed)
            {
                if (unsafeFlowAllowed)
                    logwarnout("Use of a return instruction outside of a subroutine.");
                else
                {
                    logerrout("Use of a return instruction outside of a subroutine.");
                    noerrors = false;
                }
            }
        }

        if (iline->words[0] == R_ENDSUB)
        {
            if (iline->words.size() == 1)
            {
                if (subClosed)
                {
                    logerrout("endsub must refer to a specific subroutine.");
                    noerrors = false;
                }
                else
                {
                    subClosed = true;
                    // dealias ENDSUB as RET
                    iline->words[0] = R_ENDSUB_DEALIAS;
                }
            }
            else
            {
                logerrout("Wrong subroutine declaration syntax.");
                noerrors = false;
            }
        }

        unordered_set<string>::iterator id = KeywordIDs.find(iline->words[0]);
        if (id != KeywordIDs.end())
            continue;

        map<string, XCM2_WORD>::iterator irid = IRIDs.find(iline->words[0]);
        if (irid != IRIDs.end())
        {
            pc += 1 + IRSigs[irid->second].extraSize;
            //(*errstr) )->Append( pc )->Append( " " )->Append( iline->words[0] )->Append( "\n";
        }
        else
        {
            logerrout("Unknown operator '" + iline->words[0] + "'.");
            noerrors = false;
        }
    }

    if (!subClosed)
    {
        logerrout("Missing endsub for the last subroutine.");
        noerrors = false;
    }

    programSize = pc;

    if (!mainEntryDeclared)
        logwarnout("Main entry is not declared.");

    if (mainEntry != offset)
    {
        programSize += 3; // JMP mainEntry at the beginning
        for (map<string, XCM2_RAM_ADDR>::iterator ientry = entries.begin(); ientry != entries.end(); ++ientry)
            ientry->second = (XCM2_RAM_ADDR)(ientry->second + 3);
        for (map<string, XCM2_RAM_ADDR>::iterator isub = subroutines.begin(); isub != subroutines.end(); ++isub)
            isub->second = (XCM2_RAM_ADDR)(isub->second + 3);
        mainEntry += 3;
    }

    // dealias entries and subroutines addresses
    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;

        bool isCall = false;
        bool isJump = false;

        if (iline->words.size() > 1)
        {
            unordered_set<string>::iterator icall = CallIRIDs.find(iline->words[0]);
            if (icall != CallIRIDs.end())
                isCall = true;
            else
            {
                unordered_set<string>::iterator ijump = JumpIRIDs.find(iline->words[0]);
                if (ijump != JumpIRIDs.end())
                    isJump = true;
            }
        }

        for (vector<string>::iterator iword = iline->words.begin(); iword != iline->words.end(); ++iword)
        {
            // dealias entries address
            map<string, XCM2_RAM_ADDR>::iterator ientry = entries.find(*iword);
            if (ientry != entries.end())
            {
                if (isCall && !unsafeFlowAllowed)
                {
                    logerrout("Cannot CALL an entry.");
                    noerrors = false;
                }
                else
                {
                    (*iword) = int_to_hex(ientry->second);
                    if (isCall)
                        logwarnout("Use of CALL instruction with an entry address.");
                }
            }
            else
            {
                // dealias subroutines address
                map<string, XCM2_RAM_ADDR>::iterator isub = subroutines.find(*iword);
                if (isub != subroutines.end())
                {
                    if (isJump && !unsafeFlowAllowed)
                    {
                        logerrout("Cannot JUMP to a subroutine.");
                        noerrors = false;
                    }
                    else
                    {
                        (*iword) = int_to_hex(isub->second);
                        if (isJump)
                            logwarnout("Use of JMP instruction with a subroutine address.");
                    }
                }
            }
        }
    }

    if (resources.size() > 0)
    {
        programSize += 3; // JMP to BootLoader to prevent from executing resources data
        resOrigin = programSize;
        for (map<string, StaticResourceInfo>::iterator ires = resources.begin(); ires != resources.end(); ++ires)
        {
            // dealias resource address
            for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
            {
                currentLineNumber = iline->lineNumber;
                currentSourceFile = iline->sourceFile;
                for (vector<string>::iterator iword = iline->words.begin(); iword != iline->words.end(); ++iword)
                {
                    if ((*iword) == ires->first)
                        (*iword) = int_to_hex((XCM2_RAM_ADDR)(programSize + offset));
                }

            }

            programSize += ires->second.size;
        }
    }

    // dealias MEM_ORIGIN
    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        for (vector<string>::iterator iword = iline->words.begin(); iword != iline->words.end(); ++iword)
        {
            if ((*iword) == HashMemOriginAlias)
                (*iword) = int_to_hex((XCM2_RAM_ADDR)(programSize + offset));
        }
    }

    return noerrors;
}

bool XCM2AssemblyLanguageCompiler::generateObjectCode()
{
    bool noerrors = true;
    XCM2_RAM_ADDR pos = 0;

    objectCode = (XCM2_WORD*)malloc(programSize * sizeof(XCM2_WORD));

    logout("Program size is " + to_string(programSize) + " bytes.");

    if (mainEntry != offset)
    {
        objectCode[0] = JMP;
        objectCode[1] = XCM2_HIGH_WORD(mainEntry);
        objectCode[2] = XCM2_LOW_WORD(mainEntry);
        pos += 3;
    }

    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;

        unordered_set<string>::iterator id = KeywordIDs.find(iline->words[0]);
        if (id != KeywordIDs.end())
            continue;

        map<string, XCM2_WORD>::iterator irid = IRIDs.find(iline->words[0]);
        if (irid != IRIDs.end())
        {
            XCM2_WORD ircode = irid->second;
            //objectCode[pos] = ircode;
            //pos++;
            InstructionSignature* sig = &(IRSigs[ircode]);
            XCM2_WORD args[XCM2_INSTRUCTION_MAX_EXTRA_SIZE] = {};

            bool opmatch = false;
            // we need to find matching operands list
            vector< array<OperandType, XCM2_INSTRUCTION_MAX_OPERANDS> >::iterator iop;
            for (iop = sig->operands.begin(); iop != sig->operands.end(); ++iop)
            {
                bool argsmatch = true;
                int argpos = 0;

                // iterating through operands in signature
                for (int i = 0; i < iop->size(); i++)
                {
                    XCM2_WORD wordarg;
                    XCM2_DWORD dwordarg;
                    // check for list length mismatch
                    if (i + 1 >= iline->words.size() && iop->at(i) != NONE)
                    {
                        argsmatch = false;
                        break;
                    }
                    map<OperandType, string>::iterator iregop = RegOperandsIDs.find(iop->at(i));
                    // check for reg operand
                    if (iregop != RegOperandsIDs.end())
                    {
                        if (iline->words[i + 1] != iregop->second)
                        {
                            argsmatch = false;
                            break;
                        }
                    }
                    // check for immediate operand
                    else
                    {
                        switch (iop->at(i))
                        {
                        case NONE:
                            if (iline->words.size() > i + 1)
                                argsmatch = false;
                            break;

                        case IM_WORD:
                            if (!parseChar(&(iline->words[i + 1]), wordarg))
                                if (!parseHex(&(iline->words[i + 1]), wordarg))
                                    if (!parseDec(&(iline->words[i + 1]), wordarg))
                                    {
                                        argsmatch = false;
                                        break;
                                    }
                            args[argpos] = wordarg;
                            argpos++;
                            break;

                        case IM_DOUBLE_WORD:
                            if (parseHexDouble(&(iline->words[i + 1]), dwordarg))
                            {
                                args[argpos] = XCM2_HIGH_WORD(dwordarg);
                                argpos++;
                                args[argpos] = XCM2_LOW_WORD(dwordarg);
                                argpos++;
                            }
                            else
                                argsmatch = false;
                            break;
                        }
                    }
                    if (!argsmatch)
                        break;
                }

                // list found
                if (argsmatch)
                {
                    opmatch = true;
                    break;
                }
            }
            // writing instruction call
            if (opmatch)
            {
                int arglistIndex = iop - sig->operands.begin();
                objectCode[pos] = ircode + arglistIndex;
                pos++;
                for (int i = 0; i < sig->extraSize; i++)
                {
                    objectCode[pos] = args[i];
                    pos++;
                }
            }
            else
            {
                string arg1str = "<none>";
                string arg2str = "<none>";
                if (iline->words.size() > 1)
                    arg1str = iline->words[1];
                if (iline->words.size() > 2)
                    arg2str = iline->words[2];
                logerrout("Instruction '" + iline->words[0] + "' cannot take these arguments: (" + arg1str + ", " + arg2str + ")");
                noerrors = false;
            }
        }
        else
        {
            logerrout("Unknown instruction '" + iline->words[0] + "'.");
            noerrors = false;
        }
    }

    if (resources.size() > 0)
    {
        // JMP to BootLoader
        objectCode[pos] = JMP;
        objectCode[pos + 1] = 0;
        objectCode[pos + 2] = 0;
        pos += 3;

        for (map<string, StaticResourceInfo>::iterator ires = resources.begin(); ires != resources.end(); ++ires)
        {
            memcpy(objectCode + pos, ires->second.pdata, ires->second.size);
            pos += ires->second.size;
        }
    }

    return noerrors;
}

unordered_set<string> XCM2AssemblyLanguageCompiler::KeywordIDs = {
};

unordered_set<string> XCM2AssemblyLanguageCompiler::JumpIRIDs =
{
    "JMP",
    "JNZ",
    "JZ",
    "JNC",
    "JC",
    "JPO",
    "JPE",
    "JP",
    "JM",
};

unordered_set<string> XCM2AssemblyLanguageCompiler::CallIRIDs =
{
    "CALL",
    "CNZ",
    "CZ",
    "CNC",
    "CC",
    "CPO",
    "CPE",
    "CP",
    "CM",
};

unordered_set<string> XCM2AssemblyLanguageCompiler::RetIRIDs =
{
    "RET",
    "RNZ",
    "RZ",
    "RNC",
    "RC",
    "RPO",
    "RPE",
    "RP",
    "RM",
};

map<OperandType, string> XCM2AssemblyLanguageCompiler::RegOperandsIDs = {
    { REG_A,   "A" },
    { REG_B,   "B" },
    { REG_C,   "C" },
    { REG_D,   "D" },
    { REG_E,   "E" },
    { REG_H,   "H" },
    { REG_L,   "L" },
    { REG_M,   "M" },
    { PAIR_AF, "AF" },
    { PAIR_BC, "BC" },
    { PAIR_DE, "DE" },
    { PAIR_HL, "HL" },
    { PAIR_SP, "SP" },
};

map<string, XCM2_WORD> XCM2AssemblyLanguageCompiler::IRIDs = {
    { "NOP",   NOP    },
    { "MOV",   MOV_AB },
    { "MVI",   MVI_A  },
    { "LXI",   LXI_BC },
    { "LDA",   LDA    },
    { "STA",   STA    },
    { "LHLD",  LHLD   },
    { "SHLD",  SHLD   },
    { "LDAX",  LDAX_BC},
    { "STAX",  STAX_BC},
    { "XCNG",  XCNG   },
    { "ADD",   ADD_A  },
    { "ADI",   ADI    },
    { "ADC",   ADC_A  },
    { "ACI",   ACI    },
    { "SUB",   SUB_A  },
    { "SUI",   SUI    },
    { "SBB",   SBB_A  },
    { "SBI",   SBI    },
    { "INR",   INR_A  },
    { "DCR",   DCR_A  },
    { "INX",   INX_BC },
    { "DCX",   DCX_BC },
    { "DAD",   DAD_BC },
    { "ANA",   ANA_A  },
    { "ANI",   ANI    },
    { "ORA",   ORA_A  },
    { "ORI",   ORI    },
    { "XRA",   XRA_A  },
    { "XRI",   XRI    },
    { "CMP",   CMP_A  },
    { "CPI",   CPI    },
    { "RLC",   RLC    },
    { "RRC",   RRC    },
    { "RAL",   RAL    },
    { "RAR",   RAR    },
    { "CMA",   CMA    },
    { "CMC",   CMC    },
    { "STC",   STC    },
    { "RTC",   RTC    },
    { "JMP",   JMP    },
    { "JNZ",   JNZ    },
    { "JZ",    JZ     },
    { "JNC",   JNC    },
    { "JC",    JC     },
    { "JPO",   JPO    },
    { "JPE",   JPE    },
    { "JP",    JP     },
    { "JM",    JM     },
    { "CALL",  CALL   },
    { "CNZ",   CNZ    },
    { "CZ",    CZ     },
    { "CNC",   CNC    },
    { "CC",    CC     },
    { "CPO",   CPO    },
    { "CPE",   CPE    },
    { "CP",    CP     },
    { "CM",    CM     },
    { "RET",   RET    },
    { "RNZ",   RNZ    },
    { "RZ",    RZ     },
    { "RNC",   RNC    },
    { "RC",    RC     },
    { "RPO",   RPO    },
    { "RPE",   RPE    },
    { "RP",    RP     },
    { "RM",    RM     },
    { "PCHL",  PCHL   },
    { "PUSH",  PUSH_AF},
    { "POP",   POP_AF },
    { "XTHL",  XTHL   },
    { "SPHL",  SPHL   },
    { "HLSP",  HLSP   },
    { "IN",    IIN    },
    { "OUT",   IOUT   },
    { "HLT",   HLT    },
    { "EI",    EI     },
    { "DI",    DI     },
    { "VFSA",  VFSA   },
    { "VFSAC", VFSAC  },
    { "VFLA",  VFLA   },
    { "VFLAC", VFLAC  },
    { "VFCLR", VFCLR  },
    { "VSSA",  VSSA   },
    { "VSSAC", VSSAC  },
    { "VSLA",  VSLA   },
    { "VSLAC", VSLAC  },
    { "VPRE",  VPRE   },
    { "VMODE", VMODE  },
    { "VPAL",  VPAL   },
    { "VSS",   VSS    },
    { "VSDQ",  VSDQ   },
    { "VSDT",  VSDT   },
    { "VSDA",  VSDA   },
    { "VSBA",  VSBA   },
    { "VSBO",  VSBO   },
    { "VSBX",  VSBX   }
};

map<XCM2_WORD, InstructionSignature> XCM2AssemblyLanguageCompiler::IRSigs =
{
    { NOP, { { { NONE, NONE } }, 0 } },
    { MOV_AB  , {
        {
            { REG_A, REG_B },
            { REG_A, REG_C },
            { REG_A, REG_D },
            { REG_A, REG_E },
            { REG_A, REG_H },
            { REG_A, REG_L },
            { REG_A, REG_M },

            { REG_B, REG_A },
            { REG_B, REG_C },
            { REG_B, REG_D },
            { REG_B, REG_E },
            { REG_B, REG_H },
            { REG_B, REG_L },
            { REG_B, REG_M },

            { REG_C, REG_A },
            { REG_C, REG_B },
            { REG_C, REG_D },
            { REG_C, REG_E },
            { REG_C, REG_H },
            { REG_C, REG_L },
            { REG_C, REG_M },

            { REG_D, REG_A },
            { REG_D, REG_B },
            { REG_D, REG_C },
            { REG_D, REG_E },
            { REG_D, REG_H },
            { REG_D, REG_L },
            { REG_D, REG_M },

            { REG_E, REG_A },
            { REG_E, REG_B },
            { REG_E, REG_C },
            { REG_E, REG_D },
            { REG_E, REG_H },
            { REG_E, REG_L },
            { REG_E, REG_M },

            { REG_H, REG_A },
            { REG_H, REG_B },
            { REG_H, REG_C },
            { REG_H, REG_D },
            { REG_H, REG_E },
            { REG_H, REG_L },
            { REG_H, REG_M },

            { REG_L, REG_A },
            { REG_L, REG_B },
            { REG_L, REG_C },
            { REG_L, REG_D },
            { REG_L, REG_E },
            { REG_L, REG_H },
            { REG_L, REG_M },

            { REG_M, REG_A },
            { REG_M, REG_B },
            { REG_M, REG_C },
            { REG_M, REG_D },
            { REG_M, REG_E },
            { REG_M, REG_H },
            { REG_M, REG_L },
        },
        0
    } },

    { MVI_A   , {
        {
            {REG_A, IM_WORD},
            {REG_B, IM_WORD},
            {REG_C, IM_WORD},
            {REG_D, IM_WORD},
            {REG_E, IM_WORD},
            {REG_H, IM_WORD},
            {REG_L, IM_WORD},
        },
        1
    } },

    { LXI_BC  , { {
            {PAIR_BC, IM_DOUBLE_WORD},
            {PAIR_DE, IM_DOUBLE_WORD},
            {PAIR_HL, IM_DOUBLE_WORD},
            {PAIR_SP, IM_DOUBLE_WORD},
        },
        2
    } },
    { LDA     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { STA     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { LHLD    , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { SHLD    , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { LDAX_BC , { { {PAIR_BC, NONE}, {PAIR_DE, NONE}, {PAIR_HL, NONE} }, 0 } },
    { STAX_BC , { { {PAIR_BC, NONE}, {PAIR_DE, NONE}, {PAIR_HL, NONE} }, 0 } },
    { XCNG    , { { {NONE, NONE} }, 0 } },
    { ADD_A   , { {
            {REG_A, NONE},
            {REG_B, NONE},
            {REG_C, NONE},
            {REG_D, NONE},
            {REG_E, NONE},
            {REG_H, NONE},
            {REG_L, NONE},
            {REG_M, NONE},
        },
        0
    } },
    { ADI     , { { {IM_WORD, NONE} }, 1 } },
    { ADC_A   , { {
            {REG_A, NONE},
            {REG_B, NONE},
            {REG_C, NONE},
            {REG_D, NONE},
            {REG_E, NONE},
            {REG_H, NONE},
            {REG_L, NONE},
            {REG_M, NONE},
        },
        0
    } },
    { ACI     , { { {IM_WORD, NONE} }, 1 } },
    { SUB_A   , { {
            {REG_A, NONE},
            {REG_B, NONE},
            {REG_C, NONE},
            {REG_D, NONE},
            {REG_E, NONE},
            {REG_H, NONE},
            {REG_L, NONE},
            {REG_M, NONE},
        },
        0
    } },
    { SUI     , { { {IM_WORD, NONE} }, 1 } },
    { SBB_A   , { {
            {REG_A, NONE},
            {REG_B, NONE},
            {REG_C, NONE},
            {REG_D, NONE},
            {REG_E, NONE},
            {REG_H, NONE},
            {REG_L, NONE},
            {REG_M, NONE},
        },
        0
    } },
    { SBI     , { { {IM_WORD, NONE} }, 1 } },
    { INR_A   , { {
            {REG_A, NONE},
            {REG_B, NONE},
            {REG_C, NONE},
            {REG_D, NONE},
            {REG_E, NONE},
            {REG_H, NONE},
            {REG_L, NONE},
            {REG_M, NONE},
        },
        0
    } },
    { DCR_A   , { {
            {REG_A, NONE},
            {REG_B, NONE},
            {REG_C, NONE},
            {REG_D, NONE},
            {REG_E, NONE},
            {REG_H, NONE},
            {REG_L, NONE},
            {REG_M, NONE},
        },
        0
    } },
    { INX_BC  , { {
            {PAIR_BC, NONE},
            {PAIR_DE, NONE},
            {PAIR_HL, NONE},
            {PAIR_SP, NONE},
        },
        0
    } },
    { DCX_BC  , { {
            {PAIR_BC, NONE},
            {PAIR_DE, NONE},
            {PAIR_HL, NONE},
            {PAIR_SP, NONE},
        },
        0
    } },
    { DAD_BC  , { {
            {PAIR_BC, NONE},
            {PAIR_DE, NONE},
            {PAIR_HL, NONE},
            {PAIR_SP, NONE},
        },
        0
    } },
    { ANA_A   , { {
            {REG_A, NONE},
            {REG_B, NONE},
            {REG_C, NONE},
            {REG_D, NONE},
            {REG_E, NONE},
            {REG_H, NONE},
            {REG_L, NONE},
            {REG_M, NONE},
        },
        0
    } },
    { ANI     , { { {IM_WORD, NONE} }, 1 } },
    { ORA_A   , { {
            {REG_A, NONE},
            {REG_B, NONE},
            {REG_C, NONE},
            {REG_D, NONE},
            {REG_E, NONE},
            {REG_H, NONE},
            {REG_L, NONE},
            {REG_M, NONE},
        },
        0
    } },
    { ORI     , { { {IM_WORD, NONE} }, 1 } },
    { XRA_A   , { {
            {REG_A, NONE},
            {REG_B, NONE},
            {REG_C, NONE},
            {REG_D, NONE},
            {REG_E, NONE},
            {REG_H, NONE},
            {REG_L, NONE},
            {REG_M, NONE},
        },
        0
    } },
    { XRI     , { { {IM_WORD, NONE} }, 1 } },
    { CMP_A   , { {
            {REG_A, NONE},
            {REG_B, NONE},
            {REG_C, NONE},
            {REG_D, NONE},
            {REG_E, NONE},
            {REG_H, NONE},
            {REG_L, NONE},
            {REG_M, NONE},
        },
        0
    } },
    { CPI     , { { {IM_WORD, NONE} }, 1 } },
    { RLC     , { { {NONE, NONE} }, 0 } },
    { RRC     , { { {NONE, NONE} }, 0 } },
    { RAL     , { { {NONE, NONE} }, 0 } },
    { RAR     , { { {NONE, NONE} }, 0 } },
    { CMA     , { { {NONE, NONE} }, 0 } },
    { CMC     , { { {NONE, NONE} }, 0 } },
    { STC     , { { {NONE, NONE} }, 0 } },
    { RTC     , { { {NONE, NONE} }, 0 } },
    { JMP     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { JNZ     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { JZ      , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { JNC     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { JC      , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { JPO     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { JPE     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { JP      , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { JM      , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { CALL    , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { CNZ     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { CZ      , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { CNC     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { CC      , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { CPO     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { CPE     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { CP      , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { CM      , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
    { RET     , { { {NONE, NONE} }, 0} },
    { RNZ     , { { {NONE, NONE} }, 0} },
    { RZ      , { { {NONE, NONE} }, 0} },
    { RNC     , { { {NONE, NONE} }, 0} },
    { RC      , { { {NONE, NONE} }, 0} },
    { RPO     , { { {NONE, NONE} }, 0} },
    { RPE     , { { {NONE, NONE} }, 0} },
    { RP      , { { {NONE, NONE} }, 0} },
    { RM      , { { {NONE, NONE} }, 0} },
    { PCHL    , { { {NONE, NONE} }, 0} },
    { PUSH_AF, {
        {
            { PAIR_AF, NONE },
            { PAIR_BC, NONE },
            { PAIR_DE, NONE },
            { PAIR_HL, NONE },
        },
        0
    } },
    { POP_AF  , {
        {
            {PAIR_AF, NONE},
            {PAIR_BC, NONE},
            {PAIR_DE, NONE},
            {PAIR_HL, NONE},
        },
        0
    } },
    { XTHL    , { { {NONE, NONE} }, 0} },
    { SPHL    , { { {NONE, NONE} }, 0} },
    { HLSP    , { { {NONE, NONE} }, 0} },
    { IIN     , { { {IM_WORD, NONE} }, 1} },
    { IOUT    , { { {IM_WORD, NONE} }, 1} },
    { HLT     , { { {NONE, NONE} }, 0} },
    { EI      , { { {NONE, NONE} }, 0} },
    { DI      , { { {NONE, NONE} }, 0} },
    { VFSA    , { { {NONE, NONE} }, 0} },
    { VFSAC   , { { {NONE, NONE} }, 0} },
    { VFLA    , { { {NONE, NONE} }, 0} },
    { VFLAC   , { { {NONE, NONE} }, 0} },
    { VFCLR   , { { {NONE, NONE} }, 0} },
    { VSSA    , { { {NONE, NONE} }, 0} },
    { VSSAC   , { { {NONE, NONE} }, 0} },
    { VSLA    , { { {NONE, NONE} }, 0} },
    { VSLAC   , { { {NONE, NONE} }, 0} },
    { VPRE    , { { {NONE, NONE} }, 0} },
    { VMODE   , { { {IM_WORD, NONE} }, 1} },
    { VPAL    , { { {NONE, NONE} }, 0} },
    { VSS     , { { {NONE, NONE} }, 0} },
    { VSDQ    , { { {NONE, NONE} }, 0} },
    { VSDT    , { { {NONE, NONE} }, 0} },
    { VSDA    , { { {NONE, NONE} }, 0} },
    { VSBA    , { { {NONE, NONE} }, 0} },
    { VSBO    , { { {NONE, NONE} }, 0} },
    { VSBX    , { { {NONE, NONE} }, 0} },
};

map<XCM2_WORD, string> XCM2AssemblyLanguageCompiler::IRNames =
{
   { NOP,         "NOP"        },
   { MOV_AB,      "MOV_AB"     },
   { MOV_AC,      "MOV_AC"     },
   { MOV_AD,      "MOV_AD"     },
   { MOV_AE,      "MOV_AE"     },
   { MOV_AH,      "MOV_AH"     },
   { MOV_AL,      "MOV_AL"     },
   { MOV_AM,      "MOV_AM"     },
   { MOV_BA,      "MOV_BA"     },
   { MOV_BC,      "MOV_BC"     },
   { MOV_BD,      "MOV_BD"     },
   { MOV_BE,      "MOV_BE"     },
   { MOV_BH,      "MOV_BH"     },
   { MOV_BL,      "MOV_BL"     },
   { MOV_BM,      "MOV_BM"     },
   { MOV_CA,      "MOV_CA"     },
   { MOV_CB,      "MOV_CB"     },
   { MOV_CD,      "MOV_CD"     },
   { MOV_CE,      "MOV_CE"     },
   { MOV_CH,      "MOV_CH"     },
   { MOV_CL,      "MOV_CL"     },
   { MOV_CM,      "MOV_CM"     },
   { MOV_DA,      "MOV_DA"     },
   { MOV_DB,      "MOV_DB"     },
   { MOV_DC,      "MOV_DC"     },
   { MOV_DE,      "MOV_DE"     },
   { MOV_DH,      "MOV_DH"     },
   { MOV_DL,      "MOV_DL"     },
   { MOV_DM,      "MOV_DM"     },
   { MOV_EA,      "MOV_EA"     },
   { MOV_EB,      "MOV_EB"     },
   { MOV_EC,      "MOV_EC"     },
   { MOV_ED,      "MOV_ED"     },
   { MOV_EH,      "MOV_EH"     },
   { MOV_EL,      "MOV_EL"     },
   { MOV_EM,      "MOV_EM"     },
   { MOV_HA,      "MOV_HA"     },
   { MOV_HB,      "MOV_HB"     },
   { MOV_HC,	  "MOV_HC"     },
   { MOV_HD,	  "MOV_HD"     },
   { MOV_HE,	  "MOV_HE"     },
   { MOV_HL,      "MOV_HL"     },
   { MOV_HM,      "MOV_HM"     },
   { MOV_LA,      "MOV_LA"     },
   { MOV_LB,	  "MOV_LB"     },
   { MOV_LC,	  "MOV_LC"     },
   { MOV_LD,	  "MOV_LD"     },
   { MOV_LE,	  "MOV_LE"     },
   { MOV_LH,      "MOV_LH"     },
   { MOV_LM,      "MOV_LM"     },
   { MOV_MA,      "MOV_MA"     },
   { MOV_MB,      "MOV_MB"     },
   { MOV_MC,      "MOV_MC"     },
   { MOV_MD,      "MOV_MD"     },
   { MOV_ME,      "MOV_ME"     },
   { MOV_MH,      "MOV_MH"     },
   { MOV_ML,      "MOV_ML"     },
   { MVI_A,       "MVI_A"      },
   { MVI_B,       "MVI_B"      },
   { MVI_C,       "MVI_C"      },
   { MVI_D,       "MVI_D"      },
   { MVI_E,       "MVI_E"      },
   { MVI_H,       "MVI_H"      },
   { MVI_L,       "MVI_L"      },
   { LXI_BC,      "LXI_BC"     },
   { LXI_DE,      "LXI_DE"     },
   { LXI_HL,      "LXI_HL"     },
   { LXI_SP,      "LXI_SP"     },
   { LDA,         "LDA"        },
   { STA,         "STA"        },
   { LHLD,        "LHLD"       },
   { SHLD,        "SHLD"       },
   { LDAX_BC,     "LDAX_BC"    },
   { LDAX_DE,     "LDAX_DE"    },
   { LDAX_HL,     "LDAX_HL"    },
   { STAX_BC,     "STAX_BC"    },
   { STAX_DE,     "STAX_DE"    },
   { STAX_HL,     "STAX_HL"    },
   { XCNG,        "XCNG"       },
   { ADD_A,       "ADD_A"      },
   { ADD_B,       "ADD_B"      },
   { ADD_C,       "ADD_C"      },
   { ADD_D,       "ADD_D"      },
   { ADD_E,       "ADD_E"      },
   { ADD_H,       "ADD_H"      },
   { ADD_L,       "ADD_L"      },
   { ADD_M,       "ADD_M"      },
   { ADI,         "ADI"        },
   { ADC_A,       "ADC_A"      },
   { ADC_B,       "ADC_B"      },
   { ADC_C,       "ADC_C"      },
   { ADC_D,       "ADC_D"      },
   { ADC_E,       "ADC_E"      },
   { ADC_H,       "ADC_H"      },
   { ADC_L,       "ADC_L"      },
   { ADC_M,       "ADC_M"      },
   { ACI,         "ACI"        },
   { SUB_A,       "SUB_A"      },
   { SUB_B,       "SUB_B"      },
   { SUB_C,       "SUB_C"      },
   { SUB_D,       "SUB_D"      },
   { SUB_E,       "SUB_E"      },
   { SUB_H,       "SUB_H"      },
   { SUB_L,       "SUB_L"      },
   { SUB_M,       "SUB_M"      },
   { SUI,         "SUI"        },
   { SBB_A,       "SBB_A"      },
   { SBB_B,       "SBB_B"      },
   { SBB_C,       "SBB_C"      },
   { SBB_D,       "SBB_D"      },
   { SBB_E,       "SBB_E"      },
   { SBB_H,       "SBB_H"      },
   { SBB_L,       "SBB_L"      },
   { SBB_M,       "SBB_M"      },
   { SBI,         "SBI"        },
   { INR_A,       "INR_A"      },
   { INR_B,       "INR_B"      },
   { INR_C,       "INR_C"      },
   { INR_D,       "INR_D"      },
   { INR_E,       "INR_E"      },
   { INR_H,       "INR_H"      },
   { INR_L,       "INR_L"      },
   { INR_M,       "INR_M"      },
   { DCR_A,       "DCR_A"      },
   { DCR_B,       "DCR_B"      },
   { DCR_C,       "DCR_C"      },
   { DCR_D,       "DCR_D"      },
   { DCR_E,       "DCR_E"      },
   { DCR_H,       "DCR_H"      },
   { DCR_L,       "DCR_L"      },
   { DCR_M,       "DCR_M"      },
   { INX_BC,      "INX_BC"     },
   { INX_DE,      "INX_DE"     },
   { INX_HL,      "INX_HL"     },
   { INX_SP,      "INX_SP"     },
   { DCX_BC,      "DCX_BC"     },
   { DCX_DE,      "DCX_DE"     },
   { DCX_HL,      "DCX_HL"     },
   { DCX_SP,      "DCX_SP"     },
   { DAD_BC,      "DAD_BC"     },
   { DAD_DE,      "DAD_DE"     },
   { DAD_HL,      "DAD_HL"     },
   { DAD_SP,      "DAD_SP"     },
   //{ DAA,         "DAA"        },
   { ANA_A,       "ANA_A"      },
   { ANA_B,       "ANA_B"      },
   { ANA_C,       "ANA_C"      },
   { ANA_D,       "ANA_D"      },
   { ANA_E,       "ANA_E"      },
   { ANA_H,       "ANA_H"      },
   { ANA_L,       "ANA_L"      },
   { ANA_M,       "ANA_M"      },
   { ANI,         "ANI"        },
   { ORA_A,       "ORA_A"      },
   { ORA_B,       "ORA_B"      },
   { ORA_C,       "ORA_C"      },
   { ORA_D,       "ORA_D"      },
   { ORA_E,       "ORA_E"      },
   { ORA_H,       "ORA_H"      },
   { ORA_L,       "ORA_L"      },
   { ORA_M,       "ORA_M"      },
   { ORI,         "ORI"        },
   { XRA_A,       "XRA_A"      },
   { XRA_B,       "XRA_B"      },
   { XRA_C,       "XRA_C"      },
   { XRA_D,       "XRA_D"      },
   { XRA_E,       "XRA_E"      },
   { XRA_H,       "XRA_H"      },
   { XRA_L,       "XRA_L"      },
   { XRA_M,       "XRA_M"      },
   { XRI,         "XRI"        },
   { CMP_A,       "CMP_A"      },
   { CMP_B,       "CMP_B"      },
   { CMP_C,       "CMP_C"      },
   { CMP_D,       "CMP_D"      },
   { CMP_E,       "CMP_E"      },
   { CMP_H,       "CMP_H"      },
   { CMP_L,       "CMP_L"      },
   { CMP_M,       "CMP_M"      },
   { CPI,         "CPI"        },
   { RLC,         "RLC"        },
   { RRC,         "RRC"        },
   { RAL,         "RAL"        },
   { RAR,         "RAR"        },
   { CMA,         "CMA"        },
   { CMC,         "CMC"        },
   { STC,         "STC"        },
   { RTC,         "RTC"        },
   { JMP,         "JMP"        },
   { JNZ,         "JNZ"        },
   { JZ,          "JZ"         },
   { JNC,         "JNC"        },
   { JC,          "JC"         },
   { JPO,         "JPO"        },
   { JPE,         "JPE"        },
   { JP,          "JP"         },
   { JM,          "JM"         },
   { CALL,        "CALL"       },
   { CNZ,         "CNZ"        },
   { CZ,          "CZ"         },
   { CNC,         "CNC"        },
   { CC,          "CC"         },
   { CPO,         "CPO"        },
   { CPE,         "CPE"        },
   { CP,          "CP"         },
   { CM,          "CM"         },
   { RET,         "RET"        },
   { RNZ,         "RNZ"        },
   { RZ,          "RZ"         },
   { RNC,         "RNC"        },
   { RC,          "RC"         },
   { RPO,         "RPO"        },
   { RPE,         "RPE"        },
   { RP,          "RP"         },
   { RM,          "RM"         },
   { PCHL,        "PCHL"       },
   { PUSH_AF,     "PUSH_AF"    },
   { PUSH_BC,     "PUSH_BC"    },
   { PUSH_DE,     "PUSH_DE"    },
   { PUSH_HL,     "PUSH_HL"    },
   { POP_AF,      "POP_AF"     },
   { POP_BC,      "POP_BC"     },
   { POP_DE,      "POP_DE"     },
   { POP_HL,      "POP_HL"     },
   { XTHL,        "XTHL"       },
   { SPHL,        "SPHL"       },
   { IIN,         "IIN"        },
   { IOUT,        "IOUT"       },
   { HLT,         "HLT"        },
   { EI,          "EI"         },
   { DI,          "DI"         },
   { VFSA,        "VFSA"       },
   { VFSAC,       "VFSAC"      },
   { VFLA,        "VFLA"       },
   { VFLAC,       "VFLAC"      },
   { VFCLR,       "VFCLR"      },
   { VSSA,        "VSSA"       },
   { VSSAC,       "VSSAC"      },
   { VSLA,        "VSLA"       },
   { VSLAC,       "VSLAC"      },
   { VPRE,        "VPRE"       },
   { VMODE,       "VMODE"      },
   { VPAL,        "VPAL"       },
   { VSS,         "VSS"        },
   { VSDQ,        "VSDQ"       },
   { VSDT,        "VSDT"       },
   { VSDA,        "VSDA"       },
   { VSBA,        "VSBA"       },
   { VSBO,        "VSBO"       },
   { VSBX,        "VSBX"       },
};

XCM2AssemblyLanguageCompiler::~XCM2AssemblyLanguageCompiler()
{
    if (objectCode != nullptr)
        free(objectCode);
}
