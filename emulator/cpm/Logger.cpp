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
        sstream << "'" << c << "' (" << hex << (int)c << ")" << dec;
        return this;
    }

    Logger* Logger::Add(int n)
    {
        sstream << dec << n;
        return this;
    }

    Logger* Logger::AddHex(FRIDGE_DWORD n)
    {
        sstream << "0x"
            << setfill('0') << setw(sizeof(FRIDGE_DWORD))
            << hex << n << dec;
        return this;
    }

    Logger* Logger::AddHex(FRIDGE_WORD n)
    {
        sstream << "0x"
            << setfill('0') << setw(sizeof(FRIDGE_WORD))
            << hex << n << dec;
        return this;
    }


    Logger* Logger::Endl()
    {
        sstream << "\n";
        return this;
    }

    Logger* Logger::Tab()
    {
        sstream << "   ";
        return this;
    }

    string Logger::GetText()
    {
        return sstream.str();
    }

}