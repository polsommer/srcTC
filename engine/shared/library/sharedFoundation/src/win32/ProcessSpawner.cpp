// ======================================================================
//
// ProcessSpawner.cpp
//
// ======================================================================

#include "sharedFoundation/FirstSharedFoundation.h"
#include "sharedFoundation/ProcessSpawner.h"

#include "sharedFoundation/Os.h"
#include "sharedFoundation/Logger.h"

#include <minmax.h>
#include <stdexcept>
#include <memory>

ProcessSpawner::ProcessSpawner() :
	m_asConsole(false),
	hProcess(nullptr),
	hOutputRead(nullptr),
	hInputWrite(nullptr),
	currentLine(currentRead(readBuffer))
{
}

ProcessSpawner::~ProcessSpawner()
{
	closeHandles();
}

void ProcessSpawner::closeHandles()
{
	if (hProcess)
	{
		CloseHandle(hProcess);
		hProcess = nullptr;
	}
	if (hOutputRead)
	{
		CloseHandle(hOutputRead);
		hOutputRead = nullptr;
	}
	if (hInputWrite)
	{
		CloseHandle(hInputWrite);
		hInputWrite = nullptr;
	}
}

bool ProcessSpawner::terminate(unsigned exitCode)
{
	if (!hProcess)
	{
		Logger::logError("ProcessSpawner", "Cannot terminate process: handle is null.");
		return false;
	}
	if (TerminateProcess(hProcess, exitCode) == 0)
	{
		Logger::logError("ProcessSpawner", "Failed to terminate process.");
		return false;
	}
	return true;
}

bool ProcessSpawner::create(const char *commandLine, const char *startupFolder, bool asConsole)
{
	if (hProcess)
	{
		Logger::logError("ProcessSpawner", "Process already created.");
		return false;
	}

	if (!commandLine)
	{
		Logger::logError("ProcessSpawner", "Command line is null.");
		return false;
	}

	if (!startupFolder)
	{
		startupFolder = Os::getProgramStartupDirectory();
	}

	m_asConsole = asConsole;

	STARTUPINFO sinfo{};
	sinfo.cb = sizeof(sinfo);

	HANDLE hOutputWrite = nullptr;
	HANDLE hErrorWrite = nullptr;
	HANDLE hInputRead = nullptr;

	std::unique_ptr<SECURITY_ATTRIBUTES> sa(new SECURITY_ATTRIBUTES{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE });

	if (asConsole)
	{
		if (!createPipes(sa.get(), hOutputRead, hOutputWrite, hErrorWrite, hInputRead, hInputWrite))
		{
			Logger::logError("ProcessSpawner", "Failed to create pipes.");
			return false;
		}

		sinfo.dwFlags |= (STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW);
		sinfo.hStdError = hErrorWrite;
		sinfo.hStdInput = hInputRead;
		sinfo.hStdOutput = hOutputWrite;
		sinfo.wShowWindow = SW_HIDE;
	}

	PROCESS_INFORMATION pinfo{};
	BOOL result = CreateProcess(
		nullptr,
		(char*)commandLine,
		nullptr,
		nullptr,
		TRUE,
		CREATE_NEW_CONSOLE,
		nullptr,
		startupFolder,
		&sinfo,
		&pinfo
	);

	closeInheritableHandles(asConsole, hOutputWrite, hErrorWrite, hInputRead);

	if (result)
	{
		CloseHandle(pinfo.hThread);
		hProcess = pinfo.hProcess;
		return true;
	}
	else
	{
		Logger::logError("ProcessSpawner", "Failed to create process.");
		hProcess = nullptr;
		return false;
	}
}

bool ProcessSpawner::createPipes(SECURITY_ATTRIBUTES* sa, HANDLE& hOutputRead, HANDLE& hOutputWrite, HANDLE& hErrorWrite, HANDLE& hInputRead, HANDLE& hInputWrite)
{
	HANDLE hOutputReadTmp, hInputWriteTmp;
	if (!CreatePipe(&hOutputReadTmp, &hOutputWrite, sa, 0))
		return false;

	if (!DuplicateHandle(GetCurrentProcess(), hOutputWrite, GetCurrentProcess(), &hErrorWrite, 0, TRUE, DUPLICATE_SAME_ACCESS))
		return false;

	if (!CreatePipe(&hInputRead, &hInputWriteTmp, sa, 0))
		return false;

	if (!DuplicateHandle(GetCurrentProcess(), hOutputReadTmp, GetCurrentProcess(), &hOutputRead, 0, FALSE, DUPLICATE_SAME_ACCESS))
		return false;

	if (!DuplicateHandle(GetCurrentProcess(), hInputWriteTmp, GetCurrentProcess(), &hInputWrite, 0, FALSE, DUPLICATE_SAME_ACCESS))
		return false;

	CloseHandle(hOutputReadTmp);
	CloseHandle(hInputWriteTmp);

	return true;
}

void ProcessSpawner::closeInheritableHandles(bool asConsole, HANDLE& hOutputWrite, HANDLE& hErrorWrite, HANDLE& hInputRead)
{
	if (asConsole)
	{
		CloseHandle(hOutputWrite);
		CloseHandle(hErrorWrite);
		CloseHandle(hInputRead);
	}
}

bool ProcessSpawner::isFinished(unsigned waitTime)
{
	if (!hProcess)
	{
		Logger::logInfo("ProcessSpawner", "Process handle is null, considered finished.");
		return true;
	}

	DWORD waitResult = WaitForSingleObject(hProcess, waitTime);
	return waitResult == WAIT_OBJECT_0;
}

bool ProcessSpawner::getExitCode(unsigned &o_code)
{
	if (!hProcess)
	{
		Logger::logError("ProcessSpawner", "Cannot get exit code: handle is null.");
		return false;
	}

	DWORD exitCode;
	BOOL result = GetExitCodeProcess(hProcess, &exitCode);
	if (result)
	{
		o_code = exitCode;
		return true;
	}
	else
	{
		Logger::logError("ProcessSpawner", "Failed to get exit code.");
		return false;
	}
}

bool ProcessSpawner::_returnExistingLine(char *buffer, const int bufferSize)
{
	const char *const bufferStop = buffer + bufferSize;
	char *iter = currentLine;
	while (iter != currentRead)
	{
		if (buffer == bufferStop)
		{
			currentLine = iter;
			return true;
		}

		if (*iter == '\n')
		{
			*buffer++ = 0;
			_stepIter(iter);
			currentLine = iter;
			return true;
		}
		else
		{
			*buffer++ = *iter;
			_stepIter(iter);
		}
	}

	return false;
}

bool ProcessSpawner::getOutputString(char *buffer, int bufferSize)
{
	if (_returnExistingLine(buffer, bufferSize))
	{
		return true;
	}

	if (!hOutputRead)
	{
		Logger::logError("ProcessSpawner", "Output read handle is null.");
		return false;
	}

	DWORD dwAvail = 0;
	if (!::PeekNamedPipe(hOutputRead, NULL, 0, NULL, &dwAvail, NULL))
	{
		Logger::logError("ProcessSpawner", "Failed to peek named pipe.");
		return false;
	}

	if (!dwAvail)
	{
		return false;
	}

	DWORD dwRead;

	if (currentRead >= currentLine)
	{
		const unsigned bufferAvailable = sizeof(readBuffer) - (currentRead - readBuffer);
		unsigned toRead = min(bufferAvailable, dwAvail);

		if (!::ReadFile(hOutputRead, currentRead, toRead, &dwRead, NULL) || !dwRead)
		{
			Logger::logError("ProcessSpawner", "Failed to read from output pipe.");
			return false;
		}
		dwAvail -= dwRead;
		currentRead += dwRead;
		if (currentRead == readBuffer + sizeof(readBuffer))
		{
			currentRead = readBuffer;
		}
	}

	if (dwAvail > 0)
	{
		const unsigned bufferAvailable = currentLine - currentRead - 1;
		if (bufferAvailable)
		{
			unsigned toRead = min(bufferAvailable, dwAvail);

			if (!::ReadFile(hOutputRead, currentRead, toRead, &dwRead, NULL) || !dwRead)
			{
				Logger::logError("ProcessSpawner", "Failed to read from output pipe.");
				return false;
			}
			currentRead += dwRead;

			DEBUG_FATAL(currentRead >= currentLine, (""));
		}
	}

	return _returnExistingLine(buffer, bufferSize);
}

