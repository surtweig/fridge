#include "stdafx.h"
#include "XCM2.h"

coutStreamWrapper::coutStreamWrapper(std::ostream* cout)
{
    this->cout = cout;
}

ITextStreamWrapper* coutStreamWrapper::Append(const char* value)
{
    (*cout) << value;
    return this;
}

ITextStreamWrapper* coutStreamWrapper::Append(const int value)
{
    (*cout) << value;
    return this;
}

ITextStreamWrapper* coutStreamWrapper::Append(const std::string &value)
{
    (*cout) << value;
    return this;
}

std::string coutStreamWrapper::GetText()
{
    std::ostringstream ss;
    ss << cout->rdbuf();
    return ss.str();
}


sstreamWrapper::sstreamWrapper(std::ostringstream* ss)
{
    this->ss = ss;
}

ITextStreamWrapper* sstreamWrapper::Append(const char* value)
{
    (*ss) << value;
    return this;
}

ITextStreamWrapper* sstreamWrapper::Append(const int value)
{
    (*ss) << value;
    return this;
}

ITextStreamWrapper* sstreamWrapper::Append(const std::string &value)
{
    (*ss) << value;
    return this;
}

std::string sstreamWrapper::GetText()
{
    return ss->str();
}