#include "FridgeAssemblyLanguageCompiler.h"
#include <cstring>
#include <iterator>
#include <regex>

#ifndef FALC_STD_PATH
#define FALC_STD_PATH "../sdk/x2al_std/"
#endif

#define FALC_ERR_LOC "[" << currentSourceFile << " : " << currentLineNumber << "] "

FridgeAssemblyLanguageCompiler::FridgeAssemblyLanguageCompiler(string sourceRootFolder, string sourceFileName,
    string outputFile, vector<string> includeFolders, ostream* errstr, bool saveVHDL, bool saveVHDLAggregate, bool saveCpmHeader)
    : FridgeAssemblyLanguageCompiler(std::move(sourceRootFolder), std::move(sourceFileName), std::move(outputFile),
                                     std::move(includeFolders), errstr, saveVHDL, saveVHDLAggregate, saveCpmHeader, nullptr)
{
}

FridgeAssemblyLanguageCompiler::FridgeAssemblyLanguageCompiler(istream& source, string sourceRootFolder,
    string sourceFileName, string outputFile, ostream* errstr)
    : FridgeAssemblyLanguageCompiler(std::move(sourceRootFolder), std::move(sourceFileName), std::move(outputFile),
                                     {}, errstr, false, false, false, &source)
{
}

FridgeAssemblyLanguageCompiler::FridgeAssemblyLanguageCompiler(string sourceRootFolder, string sourceFileName, string outputFile, vector<string> includeFolders, ostream* errstr, bool saveVHDL, bool saveVHDLAggregate, bool saveCpmHeader, istream* source)
{
    this->errstr = errstr;
    this->includeFolders = includeFolders;
    mainSourceName = sourceFileName;
    mainSourcePath = sourceRootFolder + sourceFileName;
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
    KeywordIDs.emplace(R_INCBIN);

    if (source ? preprocessSource(*source, sourceRootFolder, sourceFileName)
               : preprocessFile(sourceRootFolder, sourceFileName))
    {
        initDefaultAliases();
        string header;
        successful = readResources() && dealias() && addressMarkup(saveCpmHeader) && generateObjectCode() &&
                     (!saveCpmHeader || generateCpmHeader(outputFile, header));
        if (successful && !outputFile.empty())
        {
            successful = saveObjectCode(outputFile);
            if (successful && saveVHDLAggregate)
                successful = saveObjectCodeVHDLAggregate(outputFile + ".vhd");
            else if (successful && saveVHDL)
                successful = saveObjectCodeVHDL(outputFile + ".vhd");
            if (successful && relocatable)
                successful = saveRelocations(outputFile + ".rel");
            if (successful && saveCpmHeader)
            {
                const string path = outputFile.substr(0, outputFile.size() - 4) + ".cpm";
                ofstream file(path);
                file << header;
                file.close();
                successful = static_cast<bool>(file);
                if (!successful) logerrout("Cannot write CPM header '" + path + "'.");
            }
        }
    }
    if (successful)
        logout("Compilation successful.");
    else
    {
        free(objectCode);
        objectCode = nullptr;
        logerrout("Compilation failed due to errors.");
    }
}

bool FridgeAssemblyLanguageCompiler::preprocessFile(string sourceRootFolder, string sourceFileName)
{
    ifstream f(sourceRootFolder + sourceFileName);
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
        actualSourceRootFolder = FALC_STD_PATH;
        f.clear();
        f.open(actualSourceRootFolder + sourceFileName);
    }

    if (f.is_open()) return preprocessSource(f, actualSourceRootFolder, sourceFileName);
    *errstr << FALC_ERR_LOC << "[ERROR]   Cannot open file '" << sourceFileName << "'. \n";
    return false;
}

bool FridgeAssemblyLanguageCompiler::preprocessSource(istream& source, const string& sourceRootFolder, const string& sourceFileName)
{
    currentLineNumber = 1;
    currentSourceFile = sourceFileName;
    *errstr << FALC_ERR_LOC << " Source: " << sourceRootFolder << sourceFileName << "\n";
    string line;
    string decorator;
    int decoratorLine = 0;
    while (getline(source, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto start = line.find_first_not_of(" \t");
        if (start != string::npos && line[start] == '@')
        {
            if (!decorator.empty())
            {
                logerrout("A CPM decorator must be followed by its declaration.");
                return false;
            }
            decorator = line.substr(start, line.find("//", start) - start);
            decoratorLine = currentLineNumber++;
            continue;
        }
        ParsedLine parsed;
        parseLine(&line, parsed.words);
        if (parsed.words.empty())
        {
            currentLineNumber++;
            continue;
        }
        parsed.address = 0;
        parsed.lineNumber = currentLineNumber;
        parsed.sourceFile = currentSourceFile;
        parsed.sourceFolder = sourceRootFolder;
        parsed.rawText = line;
        if (!decorator.empty())
        {
            const auto& keyword = parsed.words[0];
            if (keyword != R_STATICRESOURCE && keyword != R_ALIAS && keyword != R_SUBROUTINE)
            {
                logerrout("A CPM decorator requires a static, alias, or subroutine declaration.");
                return false;
            }
            cpmExports.push_back({std::move(decorator), parsed});
            decorator.clear();
        }
        if (parsed.words.size() == 2 && parsed.words[0] == R_INCLUDE)
        {
            const string incfile = parsed.words[1].substr(1, parsed.words[1].size() - 2);
            if (includes.emplace(incfile).second)
            {
                const int lineNumber = currentLineNumber;
                if (!preprocessFile(sourceRootFolder, incfile)) return false;
                currentSourceFile = sourceFileName;
                currentLineNumber = lineNumber;
            }
            else
                logwarnout("File " + incfile + " has already been included.");
        }
        else
            lines.push_back(std::move(parsed));
        currentLineNumber++;
    }
    if (!decorator.empty())
    {
        currentLineNumber = decoratorLine;
        logerrout("A CPM decorator must be followed by its declaration.");
        return false;
    }
    return true;
}

bool FridgeAssemblyLanguageCompiler::saveObjectCode(string outputFile)
{
    ofstream myfile(outputFile, ios::out | ios::binary);
    myfile.write((char*)objectCode, programSize * sizeof(FRIDGE_WORD));
    myfile.close();
    if (!myfile)
    {
        logerrout("Cannot write binary '" + outputFile + "'.");
        return false;
    }
    logout("Compiled program is saved to " + outputFile);
    return true;
}

bool FridgeAssemblyLanguageCompiler::saveObjectCodeVHDL(string outputFile)
{
    ofstream myfile(outputFile, ios::out);
    for (int i = 0; i < programSize * sizeof(FRIDGE_WORD); i++)
    {
        myfile << int_to_hex_vhd((FRIDGE_WORD)objectCode[i]) << ", ";
        if ((i + 1) % 16 == 0)
            myfile << "\n";
    }
    myfile << "\nothers => X\"00\"";
    myfile.close();
    return static_cast<bool>(myfile);
}

// ----------------------------------------------------------------------------
// VHDL aggregate emitter
//
// Writes a complete FridgeRAMBootImage package whose RAMBootImage constant is
// an address-indexed aggregate in the same style as the hand-maintained boot
// images of the FPGA examples:
//
//     16#00# => MVI_A,        -- MVI A, 1
//     16#01# => X"01",
//
// Opcode bytes are written as FridgeIRCodes symbolic names, operand and
// resource bytes as hex literals. Each instruction carries the text of the
// source line it was assembled from, and comment-only lines of the main
// source file are reproduced as VHDL comments above the code they precede.
// ----------------------------------------------------------------------------

static string falcTrimSpace(const string& raw)
{
    size_t b = raw.find_first_not_of(" \t\r\n");
    if (b == string::npos)
        return string();
    size_t e = raw.find_last_not_of(" \t\r\n");
    return raw.substr(b, e - b + 1);
}

// Source text of a line without its trailing // comment, trimmed.
static string falcTrimComment(const string& raw)
{
    size_t c = raw.find("//");
    return falcTrimSpace(c == string::npos ? raw : raw.substr(0, c));
}

static string falcHexByte(FRIDGE_WORD v)
{
    stringstream stream;
    stream << "X\"" << uppercase << setfill('0') << setw(2) << hex << (int)v << "\"";
    return stream.str();
}

static string falcHexAddress(FRIDGE_RAM_ADDR v, int digits)
{
    stringstream stream;
    stream << "16#" << uppercase << setfill('0') << setw(digits) << hex << (int)v << "#";
    return stream.str();
}

bool FridgeAssemblyLanguageCompiler::saveObjectCodeVHDLAggregate(string outputFile)
{
    if (objectCode == nullptr)
    {
        logerrout("No object code to save.");
        return false;
    }

    ofstream myfile(outputFile, ios::out);
    if (!myfile.is_open())
    {
        *errstr << FALC_ERR_LOC << "[ERROR]   Cannot open file '" << outputFile << "'. \n";
        return false;
    }

    // Source text and line number per address. Directives (`entry`, `main`,
    // `offset`, ...) carry no address of their own -- they keep the default 0
    // assigned during preprocessing -- so they are skipped here; otherwise an
    // `entry foo` before the first instruction would claim address 0 and hide
    // the instruction that actually lives there. Among the statements that do
    // share an address, the last one wins: `entry foo` is followed by the
    // statement it labels, and that statement's text is the useful one.
    //
    // `lines[].address` is assigned before the jump prologue is prepended to
    // the object code, so when a prologue is emitted the source addresses
    // trail the code by its 3 bytes and have to be shifted (the emulator's
    // falc frontend corrects the same way). A prologue is emitted exactly
    // when main does not sit at the offset.
    const int preambleShift = (jumpPreamble ? 3 : 0) + (stackPreamble ? 3 : 0);
    map<FRIDGE_RAM_ADDR, string> addrText;
    map<FRIDGE_RAM_ADDR, int> addrLine;
    for (vector<ParsedLine>::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        if (KeywordIDs.find(iline->words[0]) != KeywordIDs.end())
            continue;
        string text = falcTrimComment(iline->rawText);
        if (text.empty())
            continue;
        FRIDGE_RAM_ADDR addr = (FRIDGE_RAM_ADDR)(iline->address + preambleShift);
        addrText[addr] = text;
        // Comment placement is keyed on line numbers, which are only
        // comparable inside one file. Statements pulled in from include files
        // therefore do not claim a comment slot: comments written in the main
        // source are placed against the main source's own statements.
        if (iline->sourceFile == mainSourceName)
            addrLine[addr] = iline->lineNumber;
    }

    // Comment-only lines of the main source file, in line order.
    vector< pair<int, string> > comments;
    {
        ifstream src(mainSourcePath.c_str());
        string line;
        int lineNumber = 1;
        while (getline(src, line))
        {
            if (falcTrimComment(line).empty())
            {
                string text = falcTrimSpace(line);
                if (text.size() >= 2 && text[0] == '/' && text[1] == '/')
                    comments.push_back(make_pair(lineNumber, falcTrimSpace(text.substr(2))));
            }
            lineNumber++;
        }
    }

    // Resource name per start address, for data-section comments, plus the
    // line number of the `static` directive so that comments written above a
    // resource are emitted above it.
    map<FRIDGE_RAM_ADDR, string> resourceNames;
    for (map<string, StaticResourceInfo>::iterator ires = resources.begin(); ires != resources.end(); ++ires)
    {
        resourceNames[ires->second.address] = ires->first;
        for (vector<ParsedLine>::iterator iline = lines.begin(); iline != lines.end(); ++iline)
        {
            if (iline->words[0] == R_STATICRESOURCE && iline->words.size() >= 2 && iline->words[1] == ires->first)
            {
                addrLine[ires->second.address] = iline->lineNumber;
                break;
            }
        }
    }

    int addressDigits = (offset + programSize > 0x100) ? 4 : 2;

    myfile << "library ieee;\n"
           << "use ieee.std_logic_1164.all;\n"
           << "use ieee.numeric_std.all;\n"
           << "use work.FridgeGlobals.all;\n"
           << "use work.FridgeIRCodes.all;\n"
           << "\n"
           << "package FridgeRAMBootImage is\n"
           << "\n"
           << "constant RAMBootImage : XCM2_RAM :=\n"
           << "(\n"
           << "     -- Generated by falc from " << mainSourcePath << ".\n"
           << "     -- Do not edit: change the source program and re-run falc.\n"
           << "     --\n"
           << "     -- addr  bytes          instruction\n";

    int extraSize = 0;
    size_t nextComment = 0;
    for (int i = 0; i < (int)programSize; i++)
    {
        FRIDGE_RAM_ADDR addr = (FRIDGE_RAM_ADDR)(offset + i);
        FRIDGE_WORD byte = objectCode[i];
        bool isData = (i >= (int)resOrigin);
        bool instructionStart = false;
        string name;

        if (!isData)
        {
            if (extraSize == 0)
            {
                instructionStart = true;
                map<FRIDGE_WORD, string>::iterator iname = IRNames.find(byte);
                if (iname != IRNames.end())
                    name = iname->second;
                for (int ircode = byte; ircode >= 0; ircode--)
                {
                    map<FRIDGE_WORD, InstructionSignature>::iterator isig = IRSigs.find((FRIDGE_WORD)ircode);
                    if (isig != IRSigs.end())
                    {
                        extraSize = isig->second.extraSize;
                        break;
                    }
                }
            }
            else
                extraSize--;
        }

        // Reproduce the source comments that precede this unit of code.
        if (instructionStart || (isData && resourceNames.find(addr) != resourceNames.end()))
        {
            int limit = -1;
            map<FRIDGE_RAM_ADDR, int>::iterator aline = addrLine.find(addr);
            if (aline != addrLine.end())
                limit = aline->second;
            while (nextComment < comments.size() && comments[nextComment].first <= limit)
            {
                myfile << "     --" << (comments[nextComment].second.empty() ? "" : " ")
                       << comments[nextComment].second << "\n";
                nextComment++;
            }
        }

        string value = (instructionStart && !name.empty()) ? name : falcHexByte(byte);
        string comment;
        if (instructionStart)
        {
            map<FRIDGE_RAM_ADDR, string>::iterator atext = addrText.find(addr);
            if (atext != addrText.end())
                comment = atext->second;
            else if (!name.empty())
                comment = name;
        }
        else if (isData)
        {
            map<FRIDGE_RAM_ADDR, string>::iterator iname = resourceNames.find(addr);
            if (iname != resourceNames.end())
                comment = "static " + iname->second;
        }

        string entry = "     " + falcHexAddress(addr, addressDigits) + " => " + value + ",";
        if (!comment.empty())
        {
            while (entry.size() < 29)
                entry += ' ';
            entry += "-- " + comment;
        }
        myfile << entry << "\n";
    }

    // Anything the source says after the last unit of code.
    while (nextComment < comments.size())
    {
        myfile << "     --" << (comments[nextComment].second.empty() ? "" : " ")
               << comments[nextComment].second << "\n";
        nextComment++;
    }

    myfile << "     others => X\"00\"\n"
           << ");\n"
           << "end FridgeRAMBootImage;\n";
    myfile.close();
    if (!myfile) return false;
    logout("VHDL aggregate is saved to " + outputFile);
    return true;
}

void FridgeAssemblyLanguageCompiler::parseLine(string* line, vector<string> &words)
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
                    *errstr << "[ERROR]   " << FALC_ERR_LOC << "String literals must be separated from other terminals. \n";

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

void FridgeAssemblyLanguageCompiler::logout(string message)
{
    *errstr << FALC_ERR_LOC << message << "\n";
}

void FridgeAssemblyLanguageCompiler::logerrout(string message)
{
    *errstr << "[ERROR]   " << FALC_ERR_LOC << message << "\n";
}

void FridgeAssemblyLanguageCompiler::logwarnout(string message)
{
    *errstr << "[WARNING] " << FALC_ERR_LOC << message << "\n";
}

void FridgeAssemblyLanguageCompiler::initDefaultAliases()
{
    for (map<string, FRIDGE_WORD>::iterator iir = IRIDs.begin(); iir != IRIDs.end(); ++iir)
    {
        aliases[IRCodeAliasPrefix + iir->first] = to_string(iir->second);
    }
    aliases["FRIDGE_ROM_SEGMENT_SIZE"]           = int_to_hex((FRIDGE_DWORD)FRIDGE_ROM_SEGMENT_SIZE);
    aliases["FRIDGE_MAX_IO_DEVICES"]             = int_to_hex((FRIDGE_WORD)FRIDGE_MAX_IO_DEVICES);
    aliases["FRIDGE_GPU_BUS_SIZE"]               = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_BUS_SIZE);
    aliases["FRIDGE_GPU_FRAME_EGA_WIDTH"]        = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_FRAME_EGA_WIDTH);
    aliases["FRIDGE_GPU_FRAME_EGA_HEIGHT"]       = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_FRAME_EGA_HEIGHT);
    aliases["FRIDGE_GPU_TEXT_FRAME_WIDTH"]       = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_TEXT_FRAME_WIDTH);
    aliases["FRIDGE_GPU_TEXT_FRAME_HEIGHT"]      = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_TEXT_FRAME_HEIGHT);
    aliases["FRIDGE_GPU_MAX_SPRITES"]            = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_MAX_SPRITES);
    aliases["FRIDGE_GPU_MAX_SPRITES_PER_PIXEL"]  = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_MAX_SPRITES_PER_PIXEL);
    aliases["FRIDGE_GPU_FRAME_BUFFER_SIZE"]      = int_to_hex((FRIDGE_DWORD)FRIDGE_GPU_FRAME_BUFFER_SIZE);
    aliases["FRIDGE_BOOT_SECTION_INDEX_ADDRESS"] = int_to_hex((FRIDGE_RAM_ADDR)FRIDGE_BOOT_SECTION_INDEX_ADDRESS);
    aliases["FRIDGE_EXECUTABLE_OFFSET"]          = int_to_hex((FRIDGE_RAM_ADDR)FRIDGE_EXECUTABLE_OFFSET);
    aliases["FRIDGE_IRQ_SYS_TIMER"]              = int_to_hex((FRIDGE_RAM_ADDR)FRIDGE_IRQ_SYS_TIMER);
    aliases["FRIDGE_IRQ_KEYBOARD_PRESS"]         = int_to_hex((FRIDGE_RAM_ADDR)FRIDGE_IRQ_KEYBOARD_PRESS);
    aliases["FRIDGE_IRQ_KEYBOARD_RELEASE"]       = int_to_hex((FRIDGE_RAM_ADDR)FRIDGE_IRQ_KEYBOARD_RELEASE);
    aliases["FRIDGE_KEYBOARD_KEY_STATE_MASK"]    = int_to_hex((FRIDGE_WORD)FRIDGE_KEYBOARD_KEY_STATE_MASK);
    aliases["FRIDGE_KEYBOARD_KEY_CODE_MASK"]     = int_to_hex((FRIDGE_WORD)FRIDGE_KEYBOARD_KEY_CODE_MASK);
    aliases["FRIDGE_DEV_ROM_RESET_ID"]           = int_to_hex((FRIDGE_WORD)FRIDGE_DEV_ROM_RESET_ID);
    aliases["FRIDGE_DEV_ROM_ID"]                 = int_to_hex((FRIDGE_WORD)FRIDGE_DEV_ROM_ID);
    aliases["FRIDGE_DEV_KEYBOARD_ID"]            = int_to_hex((FRIDGE_WORD)FRIDGE_DEV_KEYBOARD_ID);
    aliases["FRIDGE_GPU_VIDEO_MODE_EGA"]         = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_VIDEO_MODE_EGA);
    aliases["FRIDGE_GPU_VIDEO_MODE_TEXT"]        = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_VIDEO_MODE_TEXT);
    aliases["FRIDGE_GPU_VIDEO_FRAME_0"]          = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_VIDEO_FRAME_0);
    aliases["FRIDGE_GPU_VIDEO_FRAME_1"]          = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_VIDEO_FRAME_1);
    aliases["FRIDGE_GPU_VIDEO_SWAP_NONE"]        = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_VIDEO_SWAP_NONE);
    aliases["FRIDGE_GPU_VIDEO_SWAP_AUTO"]        = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_VIDEO_SWAP_AUTO);
    aliases["FRIDGE_GPU_VIDEO_SWAP_MANUAL_00"]   = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_VIDEO_SWAP_MANUAL_00);
    aliases["FRIDGE_GPU_VIDEO_SWAP_MANUAL_01"]   = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_VIDEO_SWAP_MANUAL_01);
    aliases["FRIDGE_GPU_VIDEO_SWAP_MANUAL_10"]   = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_VIDEO_SWAP_MANUAL_10);
    aliases["FRIDGE_GPU_VIDEO_SWAP_MANUAL_11"]   = int_to_hex((FRIDGE_WORD)FRIDGE_GPU_VIDEO_SWAP_MANUAL_11);

#ifdef FRIDGE_POSIT16_SUPPORT
    aliases["FRIDGE_PAM16_NOP"]    = int_to_hex((FRIDGE_WORD)PAM16_NOP);
    aliases["FRIDGE_PAM16_RESET"]  = int_to_hex((FRIDGE_WORD)PAM16_RESET);
    aliases["FRIDGE_PAM16_PUSH"]   = int_to_hex((FRIDGE_WORD)PAM16_PUSH);
    aliases["FRIDGE_PAM16_POP"]    = int_to_hex((FRIDGE_WORD)PAM16_POP);
    aliases["FRIDGE_PAM16_ADD"]    = int_to_hex((FRIDGE_WORD)PAM16_ADD);
    aliases["FRIDGE_PAM16_SUB"]    = int_to_hex((FRIDGE_WORD)PAM16_SUB);
    aliases["FRIDGE_PAM16_MUL"]    = int_to_hex((FRIDGE_WORD)PAM16_MUL);
    aliases["FRIDGE_PAM16_DIV"]    = int_to_hex((FRIDGE_WORD)PAM16_DIV);
    aliases["FRIDGE_PAM16_FMADD"]  = int_to_hex((FRIDGE_WORD)PAM16_FMADD);
    aliases["FRIDGE_PAM16_PACK"]   = int_to_hex((FRIDGE_WORD)PAM16_PACK);
    aliases["FRIDGE_PAM16_UNPACK"] = int_to_hex((FRIDGE_WORD)PAM16_UNPACK);
#endif
}

bool FridgeAssemblyLanguageCompiler::readResources()
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

                FRIDGE_DWORD resSize = 0;
                if (iline->words[2][0] == '"')
                {
                    resSize = iline->words[2].size() - 1;
                    FRIDGE_WORD* pres = (FRIDGE_WORD*)malloc(resSize);
                    memcpy(pres, iline->words[2].c_str() + 1, resSize - 1);
                    pres[resSize - 1] = 0;
                    //for (int i = 0; i < resSize-1; i++)
                    //    pres[i] = iline->words[1][i+1];
                    //pres[resSize-1] = 0; // null terminated string
                    resources[iline->words[1]].pdata = pres;
                    resources[iline->words[1]].size = resSize;
                }
                FRIDGE_DWORD dwr;
                FRIDGE_WORD wr;
                if (parseHexDouble(&(iline->words[2]), dwr))
                {
                    resSize = 2;
                    FRIDGE_WORD* pres = (FRIDGE_WORD*)malloc(resSize);
                    pres[0] = FRIDGE_HIGH_WORD(dwr);
                    pres[1] = FRIDGE_LOW_WORD(dwr);
                    resources[iline->words[1]].pdata = pres;
                    resources[iline->words[1]].size = resSize;
                }
                if (parseChar(&(iline->words[2]), wr) || parseHex(&(iline->words[2]), wr) || parseDec(&(iline->words[2]), wr))
                {
                    resSize = 1;
                    FRIDGE_WORD* pres = (FRIDGE_WORD*)malloc(resSize);
                    pres[0] = wr;
                    resources[iline->words[1]].pdata = pres;
                    resources[iline->words[1]].size = resSize;
                }

                if (resSize == 0)
                {
                    logerrout("Invalid resource value '" + iline->words[2] + "'.");
                    return false;
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

bool FridgeAssemblyLanguageCompiler::dealias()
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
                const auto value = aliases.find(iline->words[2]);
                aliases[iline->words[1]] = value == aliases.end() ? iline->words[2] : value->second;
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

void FridgeAssemblyLanguageCompiler::printParsed()
{
    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;
        *errstr << FALC_ERR_LOC;
        for (vector<string>::iterator iword = iline->words.begin(); iword != iline->words.end(); ++iword)
        {
            *errstr << (*iword) << " ";
        }
        *errstr << "\n";
    }
}

void FridgeAssemblyLanguageCompiler::printCompiled()
{
    printCompiled(errstr);
}

void FridgeAssemblyLanguageCompiler::printCompiled(ostream* f)
{
    if (objectCode)
    {
        int rowc = 0;
        int escounter = 0;
        for (int i = 0; i < programSize; i++)
        {
            //(*errstr)  <<  int_to_hex((FRIDGE_DWORD)objectCode[i])  <<  " ";
            if (i < resOrigin)
            {
                if (escounter == 0)
                {
                    FRIDGE_WORD ircode = (FRIDGE_DWORD)objectCode[i];
                    map<FRIDGE_WORD, string>::iterator iname = IRNames.find(ircode);
                    *f << "\n" << i << ":\t" << iname->second;

                    for (; ircode >= 0; ircode--)
                    {
                        map<FRIDGE_WORD, InstructionSignature>::iterator isig = IRSigs.find(ircode);
                        if (isig != IRSigs.end())
                        {
                            escounter = IRSigs[ircode].extraSize;
                            break;
                        }
                    }

                }
                else
                {
                    *f << " " << (FRIDGE_DWORD)objectCode[i];
                    escounter--;
                }
                rowc++;
                /*if (rowc == 8)
                {
                (*errstr)  <<  "\n";
                rowc = 0;
                }*/
            }
            else
            {
                *f << "\n" << i << ":\t'" << objectCode[i] << "' (" << (int)objectCode[i] << ")";
            }
        }
        *f << "\n";
    }
}

bool FridgeAssemblyLanguageCompiler::parseDec(string* word, FRIDGE_WORD& value)
{
    if (word->size() > 3)
        return false;
    for (int i = 0; i < word->size(); i++)
        if ((*word)[i] < '0' || (*word)[i] > '9')
            return false;
    value = strtol(word->c_str(), nullptr, 10);
    return true;
}

bool FridgeAssemblyLanguageCompiler::parseHex(string* word, FRIDGE_WORD& value)
{
    if (word->size() != 4 || word->compare(0, 2, "0x") != 0 ||
        word->find_first_not_of("0123456789abcdefABCDEF", 2) != string::npos)
        return false;
    value = strtol(word->c_str(), nullptr, 16);
    return true;
}

bool FridgeAssemblyLanguageCompiler::parseHexDouble(string* word, FRIDGE_DWORD& value)
{
    if (word->size() != 6 || word->compare(0, 2, "0x") != 0 ||
        word->find_first_not_of("0123456789abcdefABCDEF", 2) != string::npos)
        return false;
    value = strtol(word->c_str(), nullptr, 16);
    return true;
}

bool FridgeAssemblyLanguageCompiler::parseChar(string* word, FRIDGE_WORD& value)
{
    if (word->size() != 3)
        return false;
    if ((*word)[0] != '\'' && (*word)[2] != '\'')
        return false;
    value = (FRIDGE_WORD)(*word)[1];
    return true;
}

template< typename T > string FridgeAssemblyLanguageCompiler::int_to_hex(T i)
{
    stringstream stream;
    stream << "0x"
           << setfill('0') << setw(sizeof(T) * 2)
           << hex << (int)i;
    return stream.str();
}

string FridgeAssemblyLanguageCompiler::int_to_hex_vhd(FRIDGE_WORD i)
{
    stringstream stream;
    stream << "X\""
           << setfill('0') << setw(sizeof(FRIDGE_WORD) * 2)
           << hex << (int)i << "\"";
    return stream.str();
}

static bool cpmIdentifier(const string& name)
{
    static const regex identifier("[a-zA-Z_][a-zA-Z_0-9]*");
    static const unordered_set<string> reserved{
        "namespace", "using", "imports", "include", "struct", "union", "static", "const",
        "ptr", "array", "if", "else", "for", "return", "break", "continue", "at", "addr",
        "cast", "convert", "void", "bool", "char", "uint8", "int8", "uint16", "int16",
        "string", "null", "true", "false"
    };
    return regex_match(name, identifier) && name.compare(0, 2, "__") != 0 && !reserved.count(name);
}

bool FridgeAssemblyLanguageCompiler::generateCpmHeader(const string& binaryFile, string& header)
{
    const auto slash = binaryFile.find_last_of("/\\");
    const string file = slash == string::npos ? binaryFile : binaryFile.substr(slash + 1);
    if (file.size() <= 4 || file.compare(file.size() - 4, 4, ".bin") != 0)
    {
        logerrout("CPM header generation requires a .bin output filename.");
        return false;
    }
    const string name = file.substr(0, file.size() - 4);
    if (!cpmIdentifier(name) || name == "global" || cpmExports.empty())
    {
        logerrout("CPM header generation requires a valid namespace filename and at least one export.");
        return false;
    }

    static const regex dataExport("@(static|const)[ \\t]+(uint8|uint16)[ \\t]*");
    static const regex subExport(R"(@void\s*\((.*)\)\s*)");
    static const regex parameter(R"(\s*\(\s*(uint8|uint16|string)\s+([a-zA-Z_][a-zA-Z_0-9]*)\s*:\s*(A|B|C|HL)\s*\)\s*)");
    ostringstream result;
    result << "namespace " << name << " imports \"" << file << '"';
    if (!relocatable) result << " at " << int_to_hex(offset);
    result << "\n(\n";
    unordered_set<string> exported;
    for (const auto& item : cpmExports)
    {
        const auto& words = item.declaration.words;
        currentSourceFile = item.declaration.sourceFile;
        currentLineNumber = item.declaration.lineNumber;
        if (words.size() < 2 || !cpmIdentifier(words[1]) || !exported.insert(words[1]).second)
        {
            logerrout("Invalid or duplicate CPM export name.");
            return false;
        }
        const string& symbol = words[1];
        smatch match;
        if (regex_match(item.decorator, match, dataExport))
        {
            const string type = match[2];
            const bool wide = type == "uint16";
            if (match[1] == "static" && words[0] == R_STATICRESOURCE)
            {
                const auto& resource = resources.at(symbol);
                if (words[2].front() == '"' || resource.size != (wide ? 2 : 1))
                {
                    logerrout("CPM static export type does not match resource '" + symbol + "'.");
                    return false;
                }
                result << "    static " << type << ' ' << symbol << " imports " << int_to_hex(resource.address) << ";\n";
                continue;
            }
            if (match[1] == "const" && words[0] == R_ALIAS)
            {
                string value = aliases.at(symbol);
                FRIDGE_WORD byte;
                FRIDGE_DWORD word;
                const bool decimal = !value.empty() && value.size() <= 5 &&
                    value.find_first_not_of("0123456789") == string::npos;
                if (parseHex(&value, byte) || parseChar(&value, byte)) word = byte;
                else if (!parseHexDouble(&value, word))
                {
                    const auto number = decimal ? stoul(value) : 0;
                    if (!decimal || number > FRIDGE_MAX_DWORD)
                    {
                        logerrout("CPM const export requires an unsigned integer alias.");
                        return false;
                    }
                    word = static_cast<FRIDGE_DWORD>(number);
                }
                if (!wide && word > 255)
                {
                    logerrout("CPM const export exceeds uint8 range.");
                    return false;
                }
                result << "    const " << type << ' ' << symbol << ' '
                       << (wide ? int_to_hex(word) : to_string(word)) << ";\n";
                continue;
            }
        }
        if (words[0] != R_SUBROUTINE || !regex_match(item.decorator, match, subExport))
        {
            logerrout("Invalid CPM decorator for '" + symbol + "'.");
            return false;
        }
        const auto subroutine = subroutines.find(symbol);
        if (subroutine == subroutines.end())
        {
            logerrout("Cannot resolve exported subroutine '" + symbol + "'.");
            return false;
        }
        const string parameters = match[1];
        istringstream input(parameters);
        string argument;
        unordered_set<string> names, registers;
        ostringstream body;
        result << "    void " << symbol << " (";
        if (parameters.find_first_not_of(" \t") != string::npos)
        {
            while (getline(input, argument, ','))
            {
                if (!regex_match(argument, match, parameter) || !cpmIdentifier(match[2]) ||
                    !names.insert(match[2]).second || !registers.insert(match[3]).second ||
                    (match[3] == "HL" ? match[1] == "uint8" : match[1] != "uint8"))
                {
                    logerrout("Invalid CPM parameter or register binding for '" + symbol + "'.");
                    return false;
                }
                const string type = match[1], argumentName = match[2], reg = match[3];
                if (names.size() > 1) result << "; ";
                result << type << ' ' << argumentName;
                if (reg == "HL")
                    body << "        __lxi_hl " << (type == "string" ? "(cast uint16, " + argumentName + ")" : argumentName) << ";\n";
                else
                    body << "        __mvi_" << static_cast<char>(tolower(reg.front())) << ' ' << argumentName << ";\n";
            }
            if (parameters.back() == ',')
            {
                logerrout("Trailing comma in CPM decorator for '" + symbol + "'.");
                return false;
            }
        }
        result << ")\n    (\n" << body.str()
               << "        __callsub " << int_to_hex(subroutine->second) << ";\n    );\n";
    }
    result << ");\n";
    header = result.str();
    return true;
}

bool FridgeAssemblyLanguageCompiler::addressMarkup(bool exportCpm)
{
    bool noerrors = true;
    bool offsetDeclared = false;
    offset = 0;
    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;
        if (iline->words[0] == R_OFFSET)
        {
            offsetDeclared = true;
            if (iline->words.size() != 2 || !parseHexDouble(&iline->words[1], offset))
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

    size_t pc = 0;
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
                map<string, FRIDGE_RAM_ADDR>::iterator ientry = entries.find(entryname);
                if (ientry == entries.end())
                {
                    entries[entryname] = pc + offset;
                    //(*errstr)  <<  FALC_ERR_LOC  <<  "Entry '"  <<  entryname  <<  "' declared at "  <<  pc  <<  "\n";
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
                map<string, FRIDGE_RAM_ADDR>::iterator isub = subroutines.find(subname);
                if (isub == subroutines.end())
                {
                    subroutines[subname] = pc + offset;
                    //(*errstr)  <<  FALC_ERR_LOC  <<  "Subroutine '"  <<  entryname  <<  "' declared at "  <<  pc  <<  "\n";
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

        map<string, FRIDGE_WORD>::iterator irid = IRIDs.find(iline->words[0]);
        if (irid != IRIDs.end())
        {
            iline->address = pc + offset;
            pc += 1 + IRSigs[irid->second].extraSize;
            //(*errstr)  <<  pc  <<  " "  <<  iline->words[0]  <<  "\n";
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

    relocatable = exportCpm && !offsetDeclared && !mainEntryDeclared;
#ifdef FRIDGE_ASCENDING_STACK
    stackPreamble = mainEntryDeclared;
#endif
    jumpPreamble = mainEntryDeclared && mainEntry != offset;
    const int preambleShift = (jumpPreamble ? 3 : 0) + (stackPreamble ? 3 : 0);

    size_t imageSize = pc + preambleShift;
    if (imageSize > FRIDGE_MAX_DWORD - offset)
    {
        logerrout("Code exceeds Fridge address space.");
        return false;
    }

    for (map<string, FRIDGE_RAM_ADDR>::iterator ientry = entries.begin(); ientry != entries.end(); ++ientry)
        ientry->second = (FRIDGE_RAM_ADDR)(ientry->second + preambleShift);
    for (map<string, FRIDGE_RAM_ADDR>::iterator isub = subroutines.begin(); isub != subroutines.end(); ++isub)
        isub->second = (FRIDGE_RAM_ADDR)(isub->second + preambleShift);

    mainEntry = (mainEntryDeclared ? mainEntry : offset) + preambleShift;

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
            map<string, FRIDGE_RAM_ADDR>::iterator ientry = entries.find(*iword);
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
                    iline->relocation = FalcRelocation::Base;
                    if (isCall)
                        logwarnout("Use of CALL instruction with an entry address.");
                }
            }
            else
            {
                // dealias subroutines address
                map<string, FRIDGE_RAM_ADDR>::iterator isub = subroutines.find(*iword);
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
                        iline->relocation = FalcRelocation::Base;
                        if (isJump)
                            logwarnout("Use of JMP instruction with a subroutine address.");
                    }
                }
            }
        }
    }

    if (resources.size() > 0)
    {
        imageSize += 3; // JMP to BootLoader to prevent from executing resources data
        resOrigin = static_cast<FRIDGE_RAM_ADDR>(imageSize);
        for (map<string, StaticResourceInfo>::iterator ires = resources.begin(); ires != resources.end(); ++ires)
        {
            if (imageSize + ires->second.size > FRIDGE_MAX_DWORD - offset)
            {
                logerrout("Static resources exceed Fridge address space.");
                return false;
            }
            // dealias resource address
            for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
            {
                currentLineNumber = iline->lineNumber;
                currentSourceFile = iline->sourceFile;
                for (vector<string>::iterator iword = iline->words.begin(); iword != iline->words.end(); ++iword)
                {
                    if ((*iword) == ires->first)
                    {
                        (*iword) = int_to_hex((FRIDGE_RAM_ADDR)(imageSize + offset));
                        iline->relocation = FalcRelocation::Base;
                    }
                }

            }

            ires->second.address = imageSize + offset;
            imageSize += ires->second.size;
        }
    }

    const size_t unlinkedSize = imageSize;
    if (!noerrors || !placeBinarySections(imageSize)) return false;
    if (resources.empty() && !binarySections.empty()) resOrigin = static_cast<FRIDGE_RAM_ADDR>(unlinkedSize);
    programSize = static_cast<FRIDGE_RAM_ADDR>(imageSize);

    for (auto& line : lines)
    {
        if (KeywordIDs.count(line.words[0])) continue;
        currentSourceFile = line.sourceFile;
        currentLineNumber = line.lineNumber;
        for (auto operand = line.words.begin() + 1; operand != line.words.end(); ++operand)
        {
            if (*operand == HeapOriginAlias)
            {
                *operand = int_to_hex(static_cast<FRIDGE_RAM_ADDR>(programSize + offset));
                line.relocation = FalcRelocation::Heap;
                continue;
            }
            const auto plus = operand->find('+');
            if (plus == string::npos) continue;
            const auto symbol = binarySymbols.find(operand->substr(0, plus));
            if (symbol == binarySymbols.end()) continue;
            string displacement = operand->substr(plus + 1);
            FRIDGE_DWORD relative;
            if (!parseHexDouble(&displacement, relative) || relative >= symbol->second.second)
            {
                logerrout("Invalid offset in linked binary address '" + *operand + "'.");
                return false;
            }
            *operand = int_to_hex(static_cast<FRIDGE_RAM_ADDR>(symbol->second.first + relative));
            line.relocation = FalcRelocation::Base;
        }
    }

    return noerrors;
}

bool FridgeAssemblyLanguageCompiler::placeBinarySections(size_t& imageSize)
{
    struct PendingBinary
    {
        const ParsedLine* line;
        string path;
        vector<FRIDGE_WORD> bytes;
    };
    vector<PendingBinary> pending;
    const size_t programEnd = offset + imageSize;
    static const regex sectionName(R"([a-zA-Z_][a-zA-Z_0-9.]*)");
    for (const auto& line : lines)
    {
        const auto& words = line.words;
        if (words[0] != R_INCBIN) continue;
        currentSourceFile = line.sourceFile;
        currentLineNumber = line.lineNumber;
        const bool automatic = words.size() == 4 && words[2] == "as";
        FRIDGE_DWORD address = 0;
        string addressText = words.size() == 4 ? words[3] : "";
        if (words.size() != 4 || words[1].size() < 3 || words[1].front() != '"' ||
            words[1].back() != '"' || (automatic ? !regex_match(addressText, sectionName) :
                (words[2] != "at" || !parseHexDouble(&addressText, address))))
        {
            logerrout("Expected 'incbin \"file.bin\" at 0xADDR' or 'incbin \"file.bin\" as name'.");
            return false;
        }
        if (!automatic && relocatable)
        {
            logerrout("A relocatable library cannot include a fixed-address binary.");
            return false;
        }
        if (!automatic && address < programEnd)
        {
            logerrout("Binary at " + addressText + " overlaps the assembled program or precedes its origin.");
            return false;
        }
        const string name = words[1].substr(1, words[1].size() - 2);
        const bool absolute = name[0] == '/' || name[0] == '\\' || (name.size() > 1 && name[1] == ':');
        const string path = absolute ? name : line.sourceFolder + name;
        ifstream file(path, ios::binary | ios::ate);
        const auto size = file.tellg();
        if (size <= 0 || size > FRIDGE_MAX_DWORD - address)
        {
            logerrout("Cannot place binary '" + path + "' at " + addressText + " (missing, empty, or too large).");
            return false;
        }
        vector<FRIDGE_WORD> bytes(static_cast<size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
        {
            logerrout("Cannot read binary '" + path + "'.");
            return false;
        }
        if (automatic)
        {
            pending.push_back({&line, path, std::move(bytes)});
            continue;
        }
        const auto next = binarySections.lower_bound(address);
        if ((next != binarySections.end() && address + bytes.size() > next->first) ||
            (next != binarySections.begin() && std::prev(next)->first + std::prev(next)->second.size() > address))
        {
            logerrout("Binary '" + path + "' overlaps another binary section.");
            return false;
        }
        binarySections.emplace_hint(next, address, std::move(bytes));
    }
    for (auto& binary : pending)
    {
        currentSourceFile = binary.line->sourceFile;
        currentLineNumber = binary.line->lineNumber;
        const auto& name = binary.line->words[3];
        size_t address = programEnd;
        for (const auto& section : binarySections)
        {
            if (address + binary.bytes.size() <= section.first) break;
            if (address < section.first + section.second.size())
                address = section.first + section.second.size();
        }
        if (address + binary.bytes.size() > FRIDGE_MAX_DWORD)
        {
            logerrout("No space to place relocatable binary '" + binary.path + "'.");
            return false;
        }
        if (!binarySymbols.emplace(name, make_pair(address, binary.bytes.size())).second)
        {
            logerrout("Duplicate linked binary name '" + name + "'.");
            return false;
        }
        if (!relocateBinary(binary.path, address, binary.bytes)) return false;
        logout("Placed relocatable binary '" + name + "' at " + int_to_hex(static_cast<FRIDGE_RAM_ADDR>(address)) + ".");
        binarySections.emplace(address, std::move(binary.bytes));
    }
    if (!binarySections.empty())
    {
        const auto& last = *binarySections.rbegin();
        imageSize = last.first - offset + last.second.size();
    }
    return true;
}

bool FridgeAssemblyLanguageCompiler::relocateBinary(const string& path, size_t address, vector<FRIDGE_WORD>& bytes)
{
    const string metadata = path + ".rel";
    ifstream file(metadata);
    string version, number, kind;
    FRIDGE_DWORD size;
    if (!(file >> version >> number) || version != "FALC-REL1" ||
        !parseHexDouble(&number, size) || size != bytes.size())
    {
        logerrout("Missing or invalid relocation metadata '" + metadata + "'.");
        return false;
    }
    size_t previousEnd = 0;
    while (file >> kind)
    {
        FRIDGE_DWORD site;
        if (!(file >> number) || !parseHexDouble(&number, site) || site < previousEnd ||
            static_cast<size_t>(site) + 2 > bytes.size() || (kind != "base" && kind != "heap"))
        {
            logerrout("Invalid relocation record in '" + metadata + "'.");
            return false;
        }
        previousEnd = site + 2;
        const size_t imageSite = address - offset + site;
        const auto type = kind == "base" ? FalcRelocation::Base : FalcRelocation::Heap;
        if (relocatable) relocations.emplace(imageSite, type);
        if (type == FalcRelocation::Heap)
        {
            linkedHeapReferences.push_back(imageSite);
            continue;
        }
        const auto relative = FRIDGE_DWORD_HL(bytes[site], bytes[site + 1]);
        if (relative > bytes.size())
        {
            logerrout("Relocation target is outside binary '" + path + "'.");
            return false;
        }
        const auto value = static_cast<FRIDGE_DWORD>(address + relative);
        bytes[site] = FRIDGE_HIGH_WORD(value);
        bytes[site + 1] = FRIDGE_LOW_WORD(value);
    }
    return file.eof() && !file.bad();
}

bool FridgeAssemblyLanguageCompiler::saveRelocations(const string& path)
{
    ofstream file(path);
    file << "FALC-REL1 " << int_to_hex(programSize) << '\n';
    for (const auto& relocation : relocations)
        file << (relocation.second == FalcRelocation::Base ? "base " : "heap ")
             << int_to_hex(static_cast<FRIDGE_DWORD>(relocation.first)) << '\n';
    file.close();
    if (file) return true;
    logerrout("Cannot write relocation metadata '" + path + "'.");
    return false;
}

bool FridgeAssemblyLanguageCompiler::generateObjectCode()
{
    bool noerrors = true;
    FRIDGE_RAM_ADDR pos = 0;

    objectCode = (FRIDGE_WORD*)calloc(programSize == 0 ? 1 : programSize, sizeof(FRIDGE_WORD));
    if (!objectCode)
    {
        logerrout("Cannot allocate object code.");
        return false;
    }

    logout("Program size is " + to_string(programSize) + " bytes.");

    // Emit LXI SP, STACK_ORIGIN preamble (ascending stack only)
    if (stackPreamble)
    {
        FRIDGE_DWORD sp_target = (FRIDGE_DWORD)(programSize + offset);
        objectCode[pos] = LXI_SP;
        objectCode[pos + 1] = FRIDGE_HIGH_WORD(sp_target);
        objectCode[pos + 2] = FRIDGE_LOW_WORD(sp_target);
        pos += 3;
    }

    if (jumpPreamble)
    {
        objectCode[pos] = JMP;
        objectCode[pos + 1] = FRIDGE_HIGH_WORD(mainEntry);
        objectCode[pos + 2] = FRIDGE_LOW_WORD(mainEntry);
        pos += 3;
    }

    for (vector< ParsedLine >::iterator iline = lines.begin(); iline != lines.end(); ++iline)
    {
        currentLineNumber = iline->lineNumber;
        currentSourceFile = iline->sourceFile;

        unordered_set<string>::iterator id = KeywordIDs.find(iline->words[0]);
        if (id != KeywordIDs.end())
            continue;

        map<string, FRIDGE_WORD>::iterator irid = IRIDs.find(iline->words[0]);
        if (irid != IRIDs.end())
        {
            FRIDGE_WORD ircode = irid->second;
            //objectCode[pos] = ircode;
            //pos++;
            InstructionSignature* sig = &(IRSigs[ircode]);
            FRIDGE_WORD args[FRIDGE_INSTRUCTION_MAX_EXTRA_SIZE] = {};

            bool opmatch = false;
            // we need to find matching operands list
            vector< array<OperandType, FRIDGE_INSTRUCTION_MAX_OPERANDS> >::iterator iop;
            for (iop = sig->operands.begin(); iop != sig->operands.end(); ++iop)
            {
                bool argsmatch = true;
                int argpos = 0;

                // iterating through operands in signature
                for (int i = 0; i < iop->size(); i++)
                {
                    FRIDGE_WORD wordarg;
                    FRIDGE_DWORD dwordarg;
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
                                    args[argpos] = FRIDGE_HIGH_WORD(dwordarg);
                                    argpos++;
                                    args[argpos] = FRIDGE_LOW_WORD(dwordarg);
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
                if (relocatable && iline->relocation != FalcRelocation::None && sig->extraSize == 2)
                    relocations.emplace(pos + 1, iline->relocation);
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
            logout("Resource '" + ires->first + "' at " + int_to_hex(pos));
            memcpy(objectCode + pos, ires->second.pdata, ires->second.size);
            pos += ires->second.size;
        }
    }

    for (const auto& section : binarySections)
        memcpy(objectCode + section.first - offset, section.second.data(), section.second.size());
    const auto heapOrigin = static_cast<FRIDGE_RAM_ADDR>(programSize + offset);
    for (auto site : linkedHeapReferences)
    {
        objectCode[site] = FRIDGE_HIGH_WORD(heapOrigin);
        objectCode[site + 1] = FRIDGE_LOW_WORD(heapOrigin);
    }
    return noerrors;
}


unordered_set<string> FridgeAssemblyLanguageCompiler::KeywordIDs = {
};



unordered_set<string> FridgeAssemblyLanguageCompiler::JumpIRIDs =
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

unordered_set<string> FridgeAssemblyLanguageCompiler::CallIRIDs =
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


unordered_set<string> FridgeAssemblyLanguageCompiler::RetIRIDs =
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

map<OperandType, string> FridgeAssemblyLanguageCompiler::RegOperandsIDs = {
    {REG_A,   "A"},
    {REG_B,   "B"},
    {REG_C,   "C"},
    {REG_D,   "D"},
    {REG_E,   "E"},
    {REG_H,   "H"},
    {REG_L,   "L"},
    {REG_M,   "M"},
    {PAIR_AF, "AF"},
    {PAIR_BC, "BC"},
    {PAIR_DE, "DE"},
    {PAIR_HL, "HL"},
    {PAIR_SP, "SP"},
};

map<string, FRIDGE_WORD> FridgeAssemblyLanguageCompiler::IRIDs = {
    {"NOP",   NOP},
    {"MOV",   MOV_AB},
    {"MVI",   MVI_A},
    {"LXI",   LXI_BC},
    {"LDA",   LDA},
    {"STA",   STA},
    {"LHLD",  LHLD},
    {"SHLD",  SHLD},
    {"LDAX",  LDAX_BC},
    {"STAX",  STAX_BC},
    {"XCNG",  XCNG},
    {"ADD",   ADD_A},
    {"ADI",   ADI},
    {"ADC",   ADC_A},
    {"ACI",   ACI},
    {"SUB",   SUB_A},
    {"SUI",   SUI},
    {"SBB",   SBB_A},
    {"SBI",   SBI},
    {"INR",   INR_A},
    {"DCR",   DCR_A},
    {"INX",   INX_BC},
    {"DCX",   DCX_BC},
    {"DAD",   DAD_BC},
    {"DAI",   DAI},
    {"ANA",   ANA_A},
    {"ANI",   ANI},
    {"ORA",   ORA_A},
    {"ORI",   ORI},
    {"XRA",   XRA_A},
    {"XRI",   XRI},
    {"CMP",   CMP_A},
    {"CPI",   CPI},
    {"RLC",   RLC},
    {"RRC",   RRC},
    {"RAL",   RAL},
    {"RAR",   RAR},
    {"CMA",   CMA},
    {"CMC",   CMC},
    {"STC",   STC},
    {"RTC",   RTC},
    {"JMP",   JMP},
    {"JNZ",   JNZ},
    {"JZ",    JZ},
    {"JNC",   JNC},
    {"JC",    JC},
    {"JPO",   JPO},
    {"JPE",   JPE},
    {"JP",    JP},
    {"JM",    JM},
    {"CALL",  CALL},
    {"CNZ",   CNZ},
    {"CZ",    CZ},
    {"CNC",   CNC},
    {"CC",    CC},
    {"CPO",   CPO},
    {"CPE",   CPE},
    {"CP",    CP},
    {"CM",    CM},
    {"RET",   RET},
    {"RNZ",   RNZ},
    {"RZ",    RZ},
    {"RNC",   RNC},
    {"RC",    RC},
    {"RPO",   RPO},
    {"RPE",   RPE},
    {"RP",    RP},
    {"RM",    RM},
    {"PCHL",  PCHL},
    {"PUSH",  PUSH_AF},
    {"POP",   POP_AF},
    {"XTHL",  XTHL},
    {"SPHL",  SPHL},
    {"HLSP",  HLSP},
    {"IN",    IIN},
    {"OUT",   IOUT},
    {"HLT",   HLT},
    {"EI",    EI},
    {"DI",    DI},
    {"VFSA",  VFSA},
    {"VFSI",  VFSI},
    {"VFSAC", VFSAC},
    {"VFLA",  VFLA},
    {"VFLAC", VFLAC},
    {"VSSA",  VSSA},
    {"VSSI",  VSSI},
    {"VSLA",  VSLA},
    {"VS2F",  VS2F},
    {"VPRE",  VPRE},
    {"VMODE", VMODE},
    {"VPAL",  VPAL},
    {"VSS",   VSS},
    {"VSD",   VSD},
#ifdef FRIDGE_POSIT16_SUPPORT
    {"PAM16C", PAM16C},
#endif
};


map<FRIDGE_WORD, InstructionSignature> FridgeAssemblyLanguageCompiler::IRSigs =
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
            {REG_M, IM_WORD},
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
    { DAI     , { { {IM_DOUBLE_WORD, NONE} }, 2 } },
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
    { VFSI    , { { {IM_WORD, IM_WORD} }, 2} },
    { VFSAC   , { { {NONE, NONE} }, 0} },
    { VFLA    , { { {NONE, NONE} }, 0} },
    { VFLAC   , { { {NONE, NONE} }, 0} },
    { VS2F    , { { {NONE, NONE} }, 0} },
    { VSSA    , { { {NONE, NONE} }, 0} },
    { VSSI    , { { {IM_WORD, IM_WORD} }, 2} },
    { VSLA    , { { {NONE, NONE} }, 0} },
    { VPRE    , { { {NONE, NONE} }, 0} },
    { VMODE   , { { {NONE, NONE} }, 0} },
    { VPAL    , { { {NONE, NONE} }, 0} },
    { VSS     , { { {NONE, NONE} }, 0} },
    { VSD     , { { {NONE, NONE} }, 0} },
#ifdef FRIDGE_POSIT16_SUPPORT
    { PAM16C  , { { {NONE, NONE} }, 0} },
#endif
};


map<FRIDGE_WORD, string> FridgeAssemblyLanguageCompiler::IRNames =
    {
        {NOP,     "NOP"},
        {MOV_AB,  "MOV_AB"},
        {MOV_AC,  "MOV_AC"},
        {MOV_AD,  "MOV_AD"},
        {MOV_AE,  "MOV_AE"},
        {MOV_AH,  "MOV_AH"},
        {MOV_AL,  "MOV_AL"},
        {MOV_AM,  "MOV_AM"},
        {MOV_BA,  "MOV_BA"},
        {MOV_BC,  "MOV_BC"},
        {MOV_BD,  "MOV_BD"},
        {MOV_BE,  "MOV_BE"},
        {MOV_BH,  "MOV_BH"},
        {MOV_BL,  "MOV_BL"},
        {MOV_BM,  "MOV_BM"},
        {MOV_CA,  "MOV_CA"},
        {MOV_CB,  "MOV_CB"},
        {MOV_CD,  "MOV_CD"},
        {MOV_CE,  "MOV_CE"},
        {MOV_CH,  "MOV_CH"},
        {MOV_CL,  "MOV_CL"},
        {MOV_CM,  "MOV_CM"},
        {MOV_DA,  "MOV_DA"},
        {MOV_DB,  "MOV_DB"},
        {MOV_DC,  "MOV_DC"},
        {MOV_DE,  "MOV_DE"},
        {MOV_DH,  "MOV_DH"},
        {MOV_DL,  "MOV_DL"},
        {MOV_DM,  "MOV_DM"},
        {MOV_EA,  "MOV_EA"},
        {MOV_EB,  "MOV_EB"},
        {MOV_EC,  "MOV_EC"},
        {MOV_ED,  "MOV_ED"},
        {MOV_EH,  "MOV_EH"},
        {MOV_EL,  "MOV_EL"},
        {MOV_EM,  "MOV_EM"},
        {MOV_HA,  "MOV_HA"},
        {MOV_HB,  "MOV_HB"},
        {MOV_HC,  "MOV_HC"},
        {MOV_HD,  "MOV_HD"},
        {MOV_HE,  "MOV_HE"},
        {MOV_HL,  "MOV_HL"},
        {MOV_HM,  "MOV_HM"},
        {MOV_LA,  "MOV_LA"},
        {MOV_LB,  "MOV_LB"},
        {MOV_LC,  "MOV_LC"},
        {MOV_LD,  "MOV_LD"},
        {MOV_LE,  "MOV_LE"},
        {MOV_LH,  "MOV_LH"},
        {MOV_LM,  "MOV_LM"},
        {MOV_MA,  "MOV_MA"},
        {MOV_MB,  "MOV_MB"},
        {MOV_MC,  "MOV_MC"},
        {MOV_MD,  "MOV_MD"},
        {MOV_ME,  "MOV_ME"},
        {MOV_MH,  "MOV_MH"},
        {MOV_ML,  "MOV_ML"},
        {MVI_A,   "MVI_A"},
        {MVI_B,   "MVI_B"},
        {MVI_C,   "MVI_C"},
        {MVI_D,   "MVI_D"},
        {MVI_E,   "MVI_E"},
        {MVI_H,   "MVI_H"},
        {MVI_L,   "MVI_L"},
        {LXI_BC,  "LXI_BC"},
        {LXI_DE,  "LXI_DE"},
        {LXI_HL,  "LXI_HL"},
        {LXI_SP,  "LXI_SP"},
        {LDA,     "LDA"},
        {STA,     "STA"},
        {LHLD,    "LHLD"},
        {SHLD,    "SHLD"},
        {LDAX_BC, "LDAX_BC"},
        {LDAX_DE, "LDAX_DE"},
        {LDAX_HL, "LDAX_HL"},
        {STAX_BC, "STAX_BC"},
        {STAX_DE, "STAX_DE"},
        {STAX_HL, "STAX_HL"},
        {XCNG,    "XCNG"},
        {ADD_A,   "ADD_A"},
        {ADD_B,   "ADD_B"},
        {ADD_C,   "ADD_C"},
        {ADD_D,   "ADD_D"},
        {ADD_E,   "ADD_E"},
        {ADD_H,   "ADD_H"},
        {ADD_L,   "ADD_L"},
        {ADD_M,   "ADD_M"},
        {ADI,     "ADI"},
        {ADC_A,   "ADC_A"},
        {ADC_B,   "ADC_B"},
        {ADC_C,   "ADC_C"},
        {ADC_D,   "ADC_D"},
        {ADC_E,   "ADC_E"},
        {ADC_H,   "ADC_H"},
        {ADC_L,   "ADC_L"},
        {ADC_M,   "ADC_M"},
        {ACI,     "ACI"},
        {SUB_A,   "SUB_A"},
        {SUB_B,   "SUB_B"},
        {SUB_C,   "SUB_C"},
        {SUB_D,   "SUB_D"},
        {SUB_E,   "SUB_E"},
        {SUB_H,   "SUB_H"},
        {SUB_L,   "SUB_L"},
        {SUB_M,   "SUB_M"},
        {SUI,     "SUI"},
        {SBB_A,   "SBB_A"},
        {SBB_B,   "SBB_B"},
        {SBB_C,   "SBB_C"},
        {SBB_D,   "SBB_D"},
        {SBB_E,   "SBB_E"},
        {SBB_H,   "SBB_H"},
        {SBB_L,   "SBB_L"},
        {SBB_M,   "SBB_M"},
        {SBI,     "SBI"},
        {INR_A,   "INR_A"},
        {INR_B,   "INR_B"},
        {INR_C,   "INR_C"},
        {INR_D,   "INR_D"},
        {INR_E,   "INR_E"},
        {INR_H,   "INR_H"},
        {INR_L,   "INR_L"},
        {INR_M,   "INR_M"},
        {DCR_A,   "DCR_A"},
        {DCR_B,   "DCR_B"},
        {DCR_C,   "DCR_C"},
        {DCR_D,   "DCR_D"},
        {DCR_E,   "DCR_E"},
        {DCR_H,   "DCR_H"},
        {DCR_L,   "DCR_L"},
        {DCR_M,   "DCR_M"},
        {INX_BC,  "INX_BC"},
        {INX_DE,  "INX_DE"},
        {INX_HL,  "INX_HL"},
        {INX_SP,  "INX_SP"},
        {DCX_BC,  "DCX_BC"},
        {DCX_DE,  "DCX_DE"},
        {DCX_HL,  "DCX_HL"},
        {DCX_SP,  "DCX_SP"},
        {DAD_BC,  "DAD_BC"},
        {DAD_DE,  "DAD_DE"},
        {DAD_HL,  "DAD_HL"},
        {DAD_SP,  "DAD_SP"},
        {DAI,     "DAI"},
        {ANA_A,   "ANA_A"},
        {ANA_B,   "ANA_B"},
        {ANA_C,   "ANA_C"},
        {ANA_D,   "ANA_D"},
        {ANA_E,   "ANA_E"},
        {ANA_H,   "ANA_H"},
        {ANA_L,   "ANA_L"},
        {ANA_M,   "ANA_M"},
        {ANI,     "ANI"},
        {ORA_A,   "ORA_A"},
        {ORA_B,   "ORA_B"},
        {ORA_C,   "ORA_C"},
        {ORA_D,   "ORA_D"},
        {ORA_E,   "ORA_E"},
        {ORA_H,   "ORA_H"},
        {ORA_L,   "ORA_L"},
        {ORA_M,   "ORA_M"},
        {ORI,     "ORI"},
        {XRA_A,   "XRA_A"},
        {XRA_B,   "XRA_B"},
        {XRA_C,   "XRA_C"},
        {XRA_D,   "XRA_D"},
        {XRA_E,   "XRA_E"},
        {XRA_H,   "XRA_H"},
        {XRA_L,   "XRA_L"},
        {XRA_M,   "XRA_M"},
        {XRI,     "XRI"},
        {CMP_A,   "CMP_A"},
        {CMP_B,   "CMP_B"},
        {CMP_C,   "CMP_C"},
        {CMP_D,   "CMP_D"},
        {CMP_E,   "CMP_E"},
        {CMP_H,   "CMP_H"},
        {CMP_L,   "CMP_L"},
        {CMP_M,   "CMP_M"},
        {CPI,     "CPI"},
        {RLC,     "RLC"},
        {RRC,     "RRC"},
        {RAL,     "RAL"},
        {RAR,     "RAR"},
        {CMA,     "CMA"},
        {CMC,     "CMC"},
        {STC,     "STC"},
        {RTC,     "RTC"},
        {JMP,     "JMP"},
        {JNZ,     "JNZ"},
        {JZ,      "JZ"},
        {JNC,     "JNC"},
        {JC,      "JC"},
        {JPO,     "JPO"},
        {JPE,     "JPE"},
        {JP,      "JP"},
        {JM,      "JM"},
        {CALL,    "CALL"},
        {CNZ,     "CNZ"},
        {CZ,      "CZ"},
        {CNC,     "CNC"},
        {CC,      "CC"},
        {CPO,     "CPO"},
        {CPE,     "CPE"},
        {CP,      "CP"},
        {CM,      "CM"},
        {RET,     "RET"},
        {RNZ,     "RNZ"},
        {RZ,      "RZ"},
        {RNC,     "RNC"},
        {RC,      "RC"},
        {RPO,     "RPO"},
        {RPE,     "RPE"},
        {RP,      "RP"},
        {RM,      "RM"},
        {PCHL,    "PCHL"},
        {PUSH_AF, "PUSH_AF"},
        {PUSH_BC, "PUSH_BC"},
        {PUSH_DE, "PUSH_DE"},
        {PUSH_HL, "PUSH_HL"},
        {POP_AF,  "POP_AF"},
        {POP_BC,  "POP_BC"},
        {POP_DE,  "POP_DE"},
        {POP_HL,  "POP_HL"},
        {XTHL,    "XTHL"},
        {SPHL,    "SPHL"},
        {HLSP,    "HLSP"},
        {IIN,     "IIN"},
        {IOUT,    "IOUT"},
        {HLT,     "HLT"},
        {EI,      "EI"},
        {DI,      "DI"},
        {VFSA,    "VFSA"},
        {VFSI,    "VFSI"},
        {VFSAC,   "VFSAC"},
        {VFLA,    "VFLA"},
        {VFLAC,   "VFLAC"},
        {VSSA,    "VSSA"},
        {VSSI,    "VSSI"},
        {VSLA,    "VSLA"},
        {VS2F,    "VS2F"},
        {VPRE,    "VPRE"},
        {VMODE,   "VMODE"},
        {VPAL,    "VPAL"},
        {VSS,     "VSS"},
        {VSD,     "VSD"},
#ifdef FRIDGE_POSIT16_SUPPORT
        {PAM16C,  "PAM16C"},
#endif
};

FridgeAssemblyLanguageCompiler::~FridgeAssemblyLanguageCompiler()
{
    if (objectCode != nullptr)
        free(objectCode);
    for (map<string, StaticResourceInfo>::iterator i = resources.begin(); i != resources.end(); ++i)
        free(i->second.pdata);
}
