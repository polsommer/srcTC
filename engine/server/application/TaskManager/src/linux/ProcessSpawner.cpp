#include "sharedFoundation/FirstSharedFoundation.h"
#include "ProcessSpawner.h"
#include <cstdio>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>

uint32 ProcessSpawner::prefix;

//-----------------------------------------------------------------------

bool tokenize(const std::string& str, std::vector<std::string>& result)
{
    std::istringstream stream(str);
    std::string token;
    bool insideQuotes = false;
    std::ostringstream currentToken;

    for (char c : str)
    {
        if (c == '\"')
        {
            insideQuotes = !insideQuotes;
        }
        else if (c == ' ' && !insideQuotes)
        {
            if (!currentToken.str().empty())
            {
                result.push_back(currentToken.str());
                currentToken.str("");  // Clear the current token
            }
        }
        else
        {
            currentToken << c;
        }
    }
    if (!currentToken.str().empty())
    {
        result.push_back(currentToken.str());
    }

    return !result.empty();
}

// ----------------------------------------------------------------------

uint32 execute(const std::string& commandLine, const std::vector<char*>& parameters)
{
    DEBUG_REPORT_LOG(true, ("Now Attempting To Spawn %s\n", commandLine.c_str()));
    pid_t p = fork();

    if (p == 0)
    {
        // Child process
        DEBUG_REPORT_LOG(true, ("Now Spawning %s\n", commandLine.c_str()));
        if (execv(commandLine.c_str(), const_cast<char* const*>(parameters.data())) == -1)
        {
            perror("execv failed");
            _exit(EXIT_FAILURE);
        }
    }
    else if (p < 0)
    {
        perror("fork failed");
        return static_cast<uint32>(-1);
    }
    return static_cast<uint32>(p);
}

// ----------------------------------------------------------------------

void makeParameters(const std::vector<std::string>& parameters, std::vector<char*>& p)
{
    p.clear();
    for (const auto& param : parameters)
    {
        p.push_back(strdup(param.c_str()));  // strdup is safer than manual new[]
    }
    p.push_back(nullptr);  // null-terminate the arguments list
}

// ----------------------------------------------------------------------

void freeParameters(std::vector<char*>& p)
{
    for (auto& param : p)
    {
        if (param)
        {
            free(param);  // Use free() since we used strdup()
            param = nullptr;
        }
    }
}

// ----------------------------------------------------------------------

uint32 ProcessSpawner::execute(const std::string& commandLine, const std::vector<std::string>& parameters)
{
    std::vector<char*> p;
    makeParameters(parameters, p);

    std::string fullPath = "./" + commandLine;
    uint32 pid = ::execute(fullPath, p);

    freeParameters(p);
    return pid;
}

// ----------------------------------------------------------------------

uint32 ProcessSpawner::execute(const std::string& commandLine)
{
    std::vector<std::string> parameters;
    tokenize(commandLine, parameters);

    std::vector<char*> p;
    makeParameters(parameters, p);

    uint32 result = ::execute(p[0], p);

    freeParameters(p);
    return result;
}

// ----------------------------------------------------------------------

bool ProcessSpawner::isProcessActive(uint32 pid)
{
    pid_t result = waitpid(static_cast<pid_t>(pid), nullptr, WNOHANG | WUNTRACED);
    return (result == 0);  // Process is still active if waitpid returns 0
}

// ----------------------------------------------------------------------

void ProcessSpawner::kill(uint32 pid)
{
    if (::kill(static_cast<int>(pid), SIGKILL) == 0)
    {
        int status;
        waitpid(static_cast<int>(pid), &status, 0);
    }
    else
    {
        perror("kill failed");
    }
}

// ----------------------------------------------------------------------

void ProcessSpawner::forceCore(uint32 pid)
{
    if (::kill(static_cast<int>(pid), SIGABRT) != 0)
    {
        perror("forceCore failed");
    }
}

// ----------------------------------------------------------------------
