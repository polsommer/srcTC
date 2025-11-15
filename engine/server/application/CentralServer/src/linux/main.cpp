#include "sharedFoundation/FirstSharedFoundation.h"

#include "ConfigCentralServer.h"
#include "CentralServer.h"

#include "sharedCompression/SetupSharedCompression.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFile/SetupSharedFile.h"
#include "sharedFoundation/Os.h"
#include "sharedNetwork/NetworkHandler.h"
#include "sharedFoundation/SetupSharedFoundation.h"
#include "sharedRandom/SetupSharedRandom.h"
#include "sharedThread/SetupSharedThread.h"

// ======================================================================
//                           SWG+
//                           Last Updated: 2024-11-04
// ======================================================================

int main(int argc, char** argv)
{
    // Install core systems
    SetupSharedThread::install();
    SetupSharedDebug::install(1024);

    //-- Initialize foundation with data
    SetupSharedFoundation::Data setupFoundationData(SetupSharedFoundation::Data::D_game);
    setupFoundationData.lpCmdLine = ConvertCommandLine(argc, argv);
    SetupSharedFoundation::install(setupFoundationData);

    // Install additional modules
    SetupSharedCompression::install();
    SetupSharedFile::install(false);
    SetupSharedRandom::install(static_cast<unsigned int>(time(nullptr))); // Time cast for 32-bit compatibility

    // Network and configuration setup
    NetworkHandler::install();
    Os::setProgramName("CentralServer");
    ConfigCentralServer::install();

    // Command-line processing for server setup
    std::string cmdLine = setupFoundationData.lpCmdLine;
    const size_t firstArgPos = cmdLine.find(' ');
    size_t lastSlashPos = 0;
    size_t nextSlashPos = 0;

    while ((nextSlashPos = cmdLine.find('/', lastSlashPos)) != std::string::npos && nextSlashPos < firstArgPos) {
        lastSlashPos = nextSlashPos + 1;
    }

    cmdLine = cmdLine.substr(lastSlashPos);
    CentralServer::getInstance().setCommandLine(cmdLine);

    //-- Run the main server process
    SetupSharedFoundation::callbackWithExceptionHandling(CentralServer::run);

    // Cleanup on exit
    NetworkHandler::remove();
    SetupSharedFoundation::remove();

#ifdef ENABLE_PROFILING
    std::exit(0);
#endif

    return 0;
}

