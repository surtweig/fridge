#pragma once
#include <string>
#include <ostream>
#include <sstream>
#include <iomanip>
#include "../../include/fridge.h"

#define CPM_ASSERT(EXPRESSION) if (!(EXPRESSION)) { compilerLog.Add(LOG_INTERNAL, #EXPRESSION, __FILE__, __LINE__); noErrors = false; return false; }
#define CPM_ASSERT_MESSAGE(EXPRESSION, MESSAGE) if (!(EXPRESSION)) { compilerLog.Add(LOG_INTERNAL, MESSAGE, __FILE__, __LINE__); noErrors = false; return false; }
#define CPM_SEMANTIC_ASSERT(EXPRESSION) if (!(EXPRESSION)) { CompilerLog()->Add(LOG_INTERNAL, #EXPRESSION, __FILE__, __LINE__); Error(); }
#define CPM_SEMANTIC_ASSERT_MESSAGE(EXPRESSION, MESSAGE) if (!(EXPRESSION)) { CompilerLog()->Add(LOG_INTERNAL, MESSAGE, __FILE__, __LINE__); Error(); }

using namespace std;

namespace CPM
{

    typedef enum LoggerMessageSeverity
    {
        LOG_MESSAGE,
        LOG_WARNING,
        LOG_ERROR,
        LOG_INTERNAL
    } LoggerMessageSeverity;

    class Logger
    {
    private:
        stringstream sstream;
    public:
        int currentLineNumber;
        string currentSourceFile;

        Logger();
        Logger* Add(LoggerMessageSeverity severity, const string& s, const string& sourceFileName = "", const int lineNumber = -1);
        Logger* Add(const string& s);
        Logger* Add(const char* s);
        Logger* Add(char c);
        Logger* Add(int n);
        Logger* AddHex(FRIDGE_DWORD n);
        Logger* AddHex(FRIDGE_WORD n);
        Logger* Endl();
        Logger* Tab();
        string GetText();
    };

}