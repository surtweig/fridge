#include "stdafx.h"
#include "Logger.h"

namespace CPM
{

    Logger::Logger() : sstream()
    {
        currentLineNumber = -1;
        currentSourceFile = "<none>";
    }

    Logger* Logger::Add(LoggerMessageSeverity severity, const string& s, const string& sourceFileName, const int lineNumber)
    {
        sstream << "\n";
        if (severity == LOG_WARNING)
            sstream << "[WARNING] ";
        else if (severity == LOG_ERROR)
            sstream << "[ERROR] ";
        else if (severity == LOG_INTERNAL)
            sstream << "[INTERNAL] ";
        if (sourceFileName.size() > 0)
            sstream << "[" << sourceFileName << ":" << lineNumber << "] ";
        else
            sstream << "[" << currentSourceFile << ":" << currentLineNumber << "] ";
        sstream << s;
        return this;
    }

    Logger* Logger::Add(const string& s)
    {
        sstream << s;
        return this;
    }

    Logger* Logger::Add(const char* s)
    {
        sstream << s;
        return this;
    }

    Logger* Logger::Add(const char c)
    {
        sstream << "'" << c << "' (" << (int)c << ")";
        return this;
    }

    Logger* Logger::Add(int n)
    {
        sstream << n;
        return this;
    }

    string Logger::GetText()
    {
        return sstream.str();
    }

}