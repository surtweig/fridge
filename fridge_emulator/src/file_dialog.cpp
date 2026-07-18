#include "file_dialog.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace FileDialog {

static bool Have(const char* exe)
{
    std::string cmd = "command -v ";
    cmd += exe;
    cmd += " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

static std::string ExecCapture(const char* cmd)
{
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe)
        return result;
    std::array<char, 4096> buf{};
    while (size_t n = std::fread(buf.data(), 1, buf.size(), pipe))
        result.append(buf.data(), n);
    pclose(pipe);
    return result;
}

static std::string TrimRight(std::string s)
{
    while (!s.empty())
    {
        char c = s.back();
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
            s.pop_back();
        else
            break;
    }
    return s;
}

static std::string ExeDir()
{
#if defined(__linux__)
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return {};
    buf[n] = '\0';
    std::string path(buf);
#elif defined(__FreeBSD__)
    char buf[4096];
    ssize_t n = readlink("/proc/curproc/file", buf, sizeof(buf) - 1);
    if (n <= 0)
        return {};
    buf[n] = '\0';
    std::string path(buf);
#else
    return {};
#endif
    size_t slash = path.find_last_of('/');
    if (slash == std::string::npos)
        return {};
    return path.substr(0, slash + 1);
}

static std::string ShellQuote(const std::string& s)
{
    std::string out = "'";
    for (char c : s)
    {
        if (c == '\'')
            out += "'\\''";
        else
            out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

std::string OpenFile(const char* title)
{
#if defined(__linux__) || defined(__FreeBSD__)
    std::string dir = ExeDir();
    if (Have("zenity"))
    {
        std::string cmd = "zenity --file-selection";
        if (title && *title)
        {
            cmd += " --title=";
            cmd += ShellQuote(title);
        }
        if (!dir.empty())
        {
            cmd += " --filename=";
            cmd += ShellQuote(dir);
        }
        std::string out = TrimRight(ExecCapture(cmd.c_str()));
        if (!out.empty())
            return out;
        return {};
    }
    if (Have("kdialog"))
    {
        std::string cmd = "kdialog --getopenfilename";
        if (!dir.empty())
        {
            cmd += ' ';
            cmd += ShellQuote(dir);
        }
        if (title && *title)
        {
            cmd += " --title ";
            cmd += ShellQuote(title);
        }
        std::string out = TrimRight(ExecCapture(cmd.c_str()));
        if (!out.empty())
            return out;
        return {};
    }
    return {};
#elif defined(_WIN32)
    return {};
#else
    return {};
#endif
}

}