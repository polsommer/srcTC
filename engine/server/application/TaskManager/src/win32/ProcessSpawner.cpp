#include "FirstTaskManager.h"
#include "sharedFoundation/FirstSharedFoundation.h"
#include "ProcessSpawner.h"
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <Windows.h>

uint32 ProcessSpawner::prefix;
std::map<uint32, std::unique_ptr<HANDLE, decltype(&CloseHandle)>> procById;

//-----------------------------------------------------------------------

bool tokenize(const std::string& str, std::vector<std::string>& result)
{
    result.clear();
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

//-----------------------------------------------------------------------

uint32 ProcessSpawner::execute(const std::string& processName, const std::vector<std::string>& parameters)
{
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    std::string cmdLine = processName;

    for (const auto& param : parameters)
    {
        cmdLine += " " + param;
    }

    std::string cmd = processName + ".exe";
    si.cb = sizeof(si);

    // Create the process
    if (!CreateProcess(cmd.c_str(), const_cast<char*>(cmdLine.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        DWORD error = GetLastError();
        LPSTR messageBuffer = nullptr;
        size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        std::string errorMessage(messageBuffer, size);
        LocalFree(messageBuffer);

        DEBUG_REPORT_LOG(true, ("ProcessSpawner: Failed to spawn process %s - Error: %s\n", cmd.c_str(), errorMessage.c_str()));
        return 0;
    }

    procById.insert({ pi.dwProcessId, std::unique_ptr<HANDLE, decltype(&CloseHandle)>(pi.hProcess, &CloseHandle) });
    return pi.dwProcessId;
}

//-----------------------------------------------------------------------

uint32 ProcessSpawner::execute(const std::string& cmd)
{
    std::vector<std::string> args;
    size_t firstArgPos = cmd.find_first_of(" ");
    std::string processName;

    if (firstArgPos < cmd.size())
    {
        std::string arguments = cmd.substr(firstArgPos + 1);
        tokenize(arguments, args);
        processName = cmd.substr(0, firstArgPos);
    }
    else
    {
        processName = cmd;
    }

    return execute(processName, args);
}

//-----------------------------------------------------------------------

bool ProcessSpawner::isProcessActive(uint32 pid) const
{
    auto it = procById.find(pid);
    if (it != procById.end())
    {
        DWORD exitCode;
        if (GetExitCodeProcess(it->second.get(), &exitCode))
        {
            return (exitCode == STILL_ACTIVE);
        }
        else
        {
            DEBUG_REPORT_LOG(true, ("ProcessSpawner: Failed to get exit code for process %u\n", pid));
        }
    }

    return false;
}

//-----------------------------------------------------------------------

void ProcessSpawner::kill(uint32 pid)
{
    HANDLE processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (processHandle)
    {
        if (!TerminateProcess(processHandle, 0))
        {
            DWORD error = GetLastError();
            DEBUG_REPORT_LOG(true, ("ProcessSpawner: Failed to terminate process %u - Error: %lu\n", pid, error));
        }
        CloseHandle(processHandle);
    }
    else
    {
        DWORD error = GetLastError();
        DEBUG_REPORT_LOG(true, ("ProcessSpawner: Failed to open process %u for termination - Error: %lu\n", pid, error));
    }
}

//-----------------------------------------------------------------------

void ProcessSpawner::forceCore(uint32 pid)
{
    kill(pid);  // Force kill the process
}

//-----------------------------------------------------------------------

