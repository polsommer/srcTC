#include "FirstSwgGameServer.h"
#include "serverGame/GameServer.h"

// Libraries and setup includes
#include "LocalizationManager.h"
#include "serverGame/SetupServerGame.h"
#include "serverPathfinding/SetupServerPathfinding.h"
#include "serverScript/SetupScript.h"
#include "serverUtility/SetupServerUtility.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFile/SetupSharedFile.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/SetupSharedFoundation.h"
#include "sharedGame/SetupSharedGame.h"
#include "sharedImage/SetupSharedImage.h"
#include "sharedLog/LogManager.h"
#include "sharedLog/SetupSharedLog.h"
#include "sharedMath/SetupSharedMath.h"
#include "sharedNetwork/NetworkHandler.h"
#include "sharedNetwork/SetupSharedNetwork.h"
#include "sharedObject/SetupSharedObject.h"
#include "sharedRandom/SetupSharedRandom.h"
#include "sharedRegex/SetupSharedRegex.h"
#include "sharedTerrain/SetupSharedTerrain.h"
#include "sharedThread/SetupSharedThread.h"
#include "sharedUtility/SetupSharedUtility.h"
#include "sharedNetworkMessages/SetupSharedNetworkMessages.h"
#include "SwgGameServer/SwgGameServer.h"
#include "SwgGameServer/WorldSnapshotParser.h"
#include "swgSharedNetworkMessages/SetupSwgSharedNetworkMessages.h"
#include "swgServerNetworkMessages/SetupSwgServerNetworkMessages.h"

// ======================================================================

int main(int argc, char **argv)
{
    // Initialize threading and debugging setups
    SetupSharedThread::install();
    SetupSharedDebug::install(1024);

    // Foundation setup for 32-bit mode
    SetupSharedFoundation::Data setupFoundationData(SetupSharedFoundation::Data::D_game);
    setupFoundationData.lpCmdLine = ConvertCommandLine(argc, argv);
    setupFoundationData.runInBackground = true;
    SetupSharedFoundation::install(setupFoundationData);

    SetupServerUtility::install();
    SetupSharedRegex::install();
    SetupSharedFile::install(false);
    SetupSharedMath::install();

    // Setup network data
    SetupSharedNetwork::SetupData networkSetupData;
    SetupSharedNetwork::getDefaultServerSetupData(networkSetupData);
    SetupSharedNetwork::install(networkSetupData);

    // Utility setup
    SetupSharedUtility::Data setupUtilityData;
    SetupSharedUtility::setupGameData(setupUtilityData);
    SetupSharedUtility::install(setupUtilityData);

    SetupSharedNetworkMessages::install();
    SetupSwgSharedNetworkMessages::install();
    SetupSwgServerNetworkMessages::install();

    // Random setup
    SetupSharedRandom::install(static_cast<unsigned int>(time(0)));

    // Object setup with data control
    {
        SetupSharedObject::Data data;
        SetupSharedObject::setupDefaultGameData(data);
        SetupSharedObject::addSlotIdManagerData(data, false);
        SetupSharedObject::addMovementTableData(data);
        SetupSharedObject::addCustomizationSupportData(data);
        SetupSharedObject::addPobEjectionTransformData(data);
        data.objectsAlterChildrenAndContents = false;
        SetupSharedObject::install(data);
    }

    // Logging setup with unique identifier
    char tmp[92];
    snprintf(tmp, sizeof(tmp), "SwgGameServer:%d", Os::getProcessId());
    SetupSharedLog::install(tmp);

    // File and image setups
    TreeFile::addSearchAbsolute(0);
    TreeFile::addSearchPath(".", 0);
    SetupSharedImage::Data setupImageData;
    SetupSharedImage::setupDefaultData(setupImageData);
    SetupSharedImage::install(setupImageData);

    // Shared game setup
    SetupSharedGame::Data setupSharedGameData;
    setupSharedGameData.setUseMountValidScaleRangeTable(true);
    setupSharedGameData.setUseClientCombatManagerSupport(true);
    SetupSharedGame::install(setupSharedGameData);

    // Terrain and scripting setups
    SetupSharedTerrain::Data setupSharedTerrainData;
    SetupSharedTerrain::setupGameData(setupSharedTerrainData);
    SetupSharedTerrain::install(setupSharedTerrainData);
    SetupScript::Data setupScriptData;
    SetupScript::setupDefaultGameData(setupScriptData);
    SetupScript::install(setupScriptData);

    SetupServerPathfinding::install();

    // Game server setup
    SetupServerGame::install();
    NetworkHandler::install();
    Os::setProgramName("SwgGameServer");
    SwgGameServer::install();

#ifdef _DEBUG
    // Debug-only feature for world snapshots
    const char* const createWorldSnapshots = ConfigFile::getKeyString("WorldSnapshot", "createWorldSnapshots", 0);
    if (createWorldSnapshots)
    {
        WorldSnapshotParser::createWorldSnapshots(createWorldSnapshots);
    }
    else
#endif
    {
        // Run game with exception handling
        SetupSharedFoundation::callbackWithExceptionHandling(GameServer::run);
    }

    // Cleanup
    NetworkHandler::remove();
    SetupServerGame::remove();
    SetupSharedLog::remove();
    SetupSharedFoundation::remove();
    SetupSharedThread::remove();

#ifdef ENABLE_PROFILING
    exit(0);
#endif

    return 0;
}

