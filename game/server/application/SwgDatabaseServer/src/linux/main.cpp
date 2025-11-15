#include "FirstSwgDatabaseServer.h"

#include "SwgDatabaseServer/SwgDatabaseServer.h"
#include "serverDatabase/ConfigServerDatabase.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFile/SetupSharedFile.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/SetupSharedFoundation.h"
#include "sharedLog/LogManager.h"
#include "sharedLog/SetupSharedLog.h"
#include "sharedNetwork/NetworkHandler.h"
#include "sharedNetwork/SetupSharedNetwork.h"
#include "sharedNetworkMessages/SetupSharedNetworkMessages.h"
#include "sharedRandom/SetupSharedRandom.h"
#include "sharedThread/SetupSharedThread.h"
#include "sharedNetworkMessages/SetupSharedNetworkMessages.h"
#include "swgSharedNetworkMessages/SetupSwgSharedNetworkMessages.h"
#include "swgServerNetworkMessages/SetupSwgServerNetworkMessages.h"

#include <cstdio> // For std::sprintf
#include <cstdlib> // For std::exit

// Function to dump process ID to a file
void dumpPid(const char * argv)
{
    pid_t p = getpid();
    char fileName[1024];
    std::sprintf(fileName, "%s.%d", argv, static_cast<int>(p));
    FILE * f = std::fopen(fileName, "w+");
    if (f) {
        std::fclose(f);
    } else {
        std::perror("Failed to create PID file");
    }
}

int main(int argc, char ** argv)
{
    // Dump process ID if needed
    // dumpPid(argv[0]);

    // Install shared thread and debug setup
    SetupSharedThread::install();
    SetupSharedDebug::install(1024);

    // Setup shared foundation
    SetupSharedFoundation::Data setupFoundationData(SetupSharedFoundation::Data::D_game);
    setupFoundationData.lpCmdLine = ConvertCommandLine(argc, argv);
    SetupSharedFoundation::install(setupFoundationData);

    // Install shared file, random, and network setups
    SetupSharedFile::install(false);
    SetupSharedRandom::install(time(nullptr));

    SetupSharedNetwork::SetupData networkSetupData;
    SetupSharedNetwork::getDefaultServerSetupData(networkSetupData);
    SetupSharedNetwork::install(networkSetupData);

    // Install network messages setups
    SetupSharedNetworkMessages::install();
    SetupSwgSharedNetworkMessages::install();
    SetupSwgServerNetworkMessages::install();

    // Install and setup shared logging
    SetupSharedLog::install("SwgDatabaseServer");

    // Add search paths for TreeFile
    TreeFile::addSearchAbsolute(0);
    TreeFile::addSearchPath(".", 0);

    // Install server database configuration
    ConfigServerDatabase::install();

    // Install network handler and set program name
    NetworkHandler::install();
    Os::setProgramName("SwgDatabaseServer");

    // Install and run SwgDatabaseServer instance
    SwgDatabaseServer::install();
    SwgDatabaseServer::getInstance().run();

    // Remove network handler and shared logging
    NetworkHandler::remove();
    SetupSharedLog::remove();

    // Remove shared foundation and thread setups
    SetupSharedFoundation::remove();
    SetupSharedThread::remove();

    // Optionally exit for profiling
#ifdef ENABLE_PROFILING
    std::exit(0);
#endif

    return 0;
}

