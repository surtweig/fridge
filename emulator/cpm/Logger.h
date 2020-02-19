#pragma once
#include <string>
#include <ostream>
#include <sstream>

#define CPM_ASSERT(EXPRESSION) if (!(EXPRESSION)) { compilerLog.Add(LOG_INTERNAL, #EXPRESSION, __FILE__, __LINE__); noErrors = false; return false; }
#define CPM_ASSERT_MESSAGE(EXPRESSION, MESSAGE) if (!(EXPRESSION)) { compilerLog.Add(LOG_INTERNAL, MESSAGE, __FILE__, __LINE__); noErrors = false; return false; }
#define CPM_SEMANTIC_ASSERT(EXPRESSION) if (!(EXPRESSION)) { CompilerLog()->Add(LOG_INTERNAL, #EXPRESSION, __FILE__, __LINE__); }
#define CPM_SEMANTIC_ASSERT_MESSAGE(EXPRESSION, MESSAGE) if (!(EXPRESSION)) { CompilerLog()->Add(LOG_INTERNAL, MESSAGE, __FILE__, __LINE__); }

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
        Logger* Add(const char c);
        Logger* Add(const int n);
        string GetText();
    };

}