// ======================================================================
//
// ConfigSharedFoundation.cpp
// copyright 1998 Bootprint Entertainment
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "sharedFoundation/FirstSharedFoundation.h"
#include "sharedFoundation/ConfigSharedFoundation.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/Production.h"
#include "sharedFoundation/Logger.h"  // Assume Logger is a logging utility

#include <optional>

// ======================================================================

namespace ConfigSharedFoundationNamespace
{
	std::optional<bool>        ms_noExceptionHandling;

	std::optional<bool>        ms_fpuExceptionPrecision;
	std::optional<bool>        ms_fpuExceptionUnderflow;
	std::optional<bool>        ms_fpuExceptionOverflow;
	std::optional<bool>        ms_fpuExceptionZeroDivide;
	std::optional<bool>        ms_fpuExceptionDenormal;
	std::optional<bool>        ms_fpuExceptionInvalid;

	std::optional<bool>        ms_demoMode;

	std::optional<real>        ms_frameRateLimit;
	std::optional<real>        ms_minFrameRate;

	std::optional<bool>        ms_useRemoteDebug;
	std::optional<int>         ms_defaultRemoteDebugPort;

	std::optional<bool>        ms_profilerExpandAllBranches;

	std::optional<bool>        ms_memoryManagerReportAllocations;
	std::optional<bool>        ms_memoryManagerReportOnOutOfMemory;

	std::optional<bool>        ms_useMemoryBlockManager;
	std::optional<bool>        ms_memoryBlockManagerDebugDumpOnRemove;

	std::optional<int>         ms_fatalCallStackDepth;
	std::optional<int>         ms_warningCallStackDepth;
	std::optional<bool>        ms_lookUpCallStackNames;

	std::optional<int>         ms_processPriority;

	std::optional<bool>        ms_verboseHardwareLogging;
	std::optional<bool>        ms_verboseWarnings;

	std::optional<bool>        ms_causeAccessViolation;

	std::optional<float>       ms_debugReportLongFrameTime;
}

using namespace ConfigSharedFoundationNamespace;

// ======================================================================

const int c_defaultFatalCallStackDepth   = 32;
const int c_defaultWarningCallStackDepth = PRODUCTION ? -1 : 8;

// ======================================================================

#define LOAD_KEY_BOOL(name, defaultValue) \
    ms_##name = ConfigFile::getKeyBool("SharedFoundation", #name, defaultValue)

#define LOAD_KEY_INT(name, defaultValue) \
    ms_##name = ConfigFile::getKeyInt("SharedFoundation", #name, defaultValue)

#define LOAD_KEY_FLOAT(name, defaultValue) \
    ms_##name = ConfigFile::getKeyFloat("SharedFoundation", #name, defaultValue)

#define LOAD_KEY_STRING(name, defaultValue) \
    ms_##name = ConfigFile::getKeyString("SharedFoundation", #name, defaultValue)

// ======================================================================

void ConfigSharedFoundation::install(const Defaults &defaults)
{
    try
    {
        LOAD_KEY_BOOL(noExceptionHandling,             false);

        LOAD_KEY_BOOL(fpuExceptionPrecision,           false);
        LOAD_KEY_BOOL(fpuExceptionUnderflow,           false);
        LOAD_KEY_BOOL(fpuExceptionOverflow,            false);
        LOAD_KEY_BOOL(fpuExceptionZeroDivide,          false);
        LOAD_KEY_BOOL(fpuExceptionDenormal,            false);
        LOAD_KEY_BOOL(fpuExceptionInvalid,             false);

        LOAD_KEY_BOOL(demoMode,                        defaults.demoMode);

    #if defined (WIN32) && PRODUCTION == 1
        // In production builds, force our frame rate limit to be the application-defined limit
        ms_frameRateLimit = defaults.frameRateLimit;
    #else
        LOAD_KEY_FLOAT(frameRateLimit,                 defaults.frameRateLimit);
    #endif

        LOAD_KEY_FLOAT(minFrameRate,                   1.0f);

        LOAD_KEY_BOOL(useRemoteDebug,                  false);
        LOAD_KEY_INT(defaultRemoteDebugPort,           4445);

        LOAD_KEY_BOOL(profilerExpandAllBranches,       false);
        LOAD_KEY_BOOL(memoryManagerReportAllocations,  false);
        LOAD_KEY_BOOL(memoryManagerReportOnOutOfMemory,false);
        LOAD_KEY_BOOL(useMemoryBlockManager,           false);
        LOAD_KEY_BOOL(memoryBlockManagerDebugDumpOnRemove, false);

        LOAD_KEY_INT(fatalCallStackDepth,              c_defaultFatalCallStackDepth);
        LOAD_KEY_INT(warningCallStackDepth,            PRODUCTION ? -1 : c_defaultWarningCallStackDepth);
        LOAD_KEY_BOOL(lookUpCallStackNames,            true);

        LOAD_KEY_INT(processPriority,                  0);

        LOAD_KEY_BOOL(verboseHardwareLogging,          false);
        LOAD_KEY_BOOL(verboseWarnings,                 defaults.verboseWarnings);

        LOAD_KEY_BOOL(causeAccessViolation,            false);

        LOAD_KEY_FLOAT(debugReportLongFrameTime,       0.25f);
        
        Logger::logInfo("ConfigSharedFoundation", "Configuration loaded successfully.");
    }
    catch (const std::exception& e)
    {
        Logger::logError("ConfigSharedFoundation", std::string("Error loading configuration: ") + e.what());
        throw;
    }
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getNoExceptionHandling()
{
	return ms_noExceptionHandling.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getFpuExceptionPrecision()
{
	return ms_fpuExceptionPrecision.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getFpuExceptionUnderflow()
{
	return ms_fpuExceptionUnderflow.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getFpuExceptionOverflow()
{
	return ms_fpuExceptionOverflow.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getFpuExceptionZeroDivide()
{
	return ms_fpuExceptionZeroDivide.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getFpuExceptionDenormal()
{
	return ms_fpuExceptionDenormal.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getFpuExceptionInvalid()
{
	return ms_fpuExceptionInvalid.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getDemoMode()
{
	return ms_demoMode.value_or(false);
}

// ----------------------------------------------------------------------

real ConfigSharedFoundation::getFrameRateLimit()
{
	return ms_frameRateLimit.value_or(1.0f);
}

// ----------------------------------------------------------------------

real ConfigSharedFoundation::getMinFrameRate()
{
	return ms_minFrameRate.value_or(1.0f);
}

// ----------------------------------------------------------------------

int ConfigSharedFoundation::getDefaultRemoteDebugPort()
{
	return ms_defaultRemoteDebugPort.value_or(4445);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getUseRemoteDebug()
{
	return ms_useRemoteDebug.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getProfilerExpandAllBranches()
{
	return ms_profilerExpandAllBranches.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getMemoryManagerReportAllocations()
{
	return ms_memoryManagerReportAllocations.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getMemoryManagerReportOnOutOfMemory()
{
	return ms_memoryManagerReportOnOutOfMemory.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getUseMemoryBlockManager()
{
	return ms_useMemoryBlockManager.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getMemoryBlockManagerDebugDumpOnRemove()
{
	return ms_memoryBlockManagerDebugDumpOnRemove.value_or(false);
}

// ----------------------------------------------------------------------

int ConfigSharedFoundation::getFatalCallStackDepth()
{
	return ms_fatalCallStackDepth.value_or(c_defaultFatalCallStackDepth);
}

// ----------------------------------------------------------------------

int ConfigSharedFoundation::getWarningCallStackDepth()
{
	return ms_warningCallStackDepth.value_or(c_defaultWarningCallStackDepth);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getLookUpCallStackNames()
{
	return ms_lookUpCallStackNames.value_or(true);
}

// ----------------------------------------------------------------------

int ConfigSharedFoundation::getProcessPriority()
{
	return ms_processPriority.value_or(0);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getVerboseHardwareLogging()
{
	return ms_verboseHardwareLogging.value_or(false);
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getVerboseWarnings()
{
	return ms_verboseWarnings.value_or(false);
}

// ----------------------------------------------------------------------

void ConfigSharedFoundation::setVerboseWarnings(bool const verboseWarnings)
{
	ms_verboseWarnings = verboseWarnings;
}

// ----------------------------------------------------------------------

bool ConfigSharedFoundation::getCauseAccessViolation()
{
	return ms_causeAccessViolation.value_or(false);
}

// ----------------------------------------------------------------------

float ConfigSharedFoundation::getDebugReportLongFrameTime()
{
	return ms_debugReportLongFrameTime.value_or(0.25f);
}

// ======================================================================

