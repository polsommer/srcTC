// ======================================================================
//
// ConfigServerDatabase.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "serverDatabase/FirstServerDatabase.h"
#include "serverDatabase/ConfigServerDatabase.h"
#include "serverUtility/ConfigServerUtility.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/NetworkId.h"
#include <string>

//-------------------------------------------------------------------

ConfigServerDatabase::Data* ConfigServerDatabase::data;
static ConfigServerDatabase::Data staticData;

//-------------------------------------------------------------------

#define KEY_INT(a,b)    (data->a = ConfigFile::getKeyInt("dbProcess", #a, b))
#define KEY_BOOL(a,b)   (data->a = ConfigFile::getKeyBool("dbProcess", #a, b))
#define KEY_FLOAT(a,b)  (data->a = ConfigFile::getKeyFloat("dbProcess", #a, b))
#define KEY_STRING(a,b) (data->a = ConfigFile::getKeyString("dbProcess", #a, b))

//-------------------------------------------------------------------

void ConfigServerDatabase::install(void)
{
	ExitChain::add(ConfigServerDatabase::remove,"ConfigServerDatabase::remove");

	ConfigServerUtility::install();
	data = &staticData;

    KEY_INT(objvarNameCleanupTime, 60);  // Clean up objvar names every 60 seconds
    KEY_INT(orphanedObjectCleanupTime, 60);  // Clean up orphaned objects every 60 seconds
    KEY_INT(marketAttributesCleanupTime, 60);  // Clean up market attributes every 60 seconds
    KEY_INT(messagesCleanupTime, 60);  // Clean up messages every 60 seconds
    KEY_INT(brokenObjectCleanupTime, 60);  // Clean up broken objects every 60 seconds
    KEY_INT(vendorObjectCleanupTime, 60);  // Clean up vendor objects every 60 seconds
    KEY_BOOL(enableFixBadCells, true);  // Enable fix for bad cells
    KEY_STRING(customSQLFilename, "");  // Custom SQL filename if needed
    KEY_STRING(objectTemplateListUpdateFilename, "");  // Object template list update filename

	KEY_STRING  (DSN,"gameserver");
	KEY_STRING  (alternateDSN,"");
	KEY_STRING  (databaseUID,"gameserver");
	KEY_STRING  (databasePWD,"gameserver");
	KEY_STRING  (databaseProtocol,"DEFAULT");
	KEY_STRING  (centralServerAddress, "localhost");
	KEY_INT     (centralServerPort, 34451);
	KEY_STRING  (commoditiesServerAddress, "localhost");
	KEY_INT     (commoditiesServerPort, 34457);	//todo: confirm that this is a good port MSH
	KEY_INT     (taskManagerPort, 60001);
	KEY_INT     (expectedDBVersion, 271);
	KEY_BOOL    (correctDBVersionRequired,true);
	KEY_INT     (saveFrequencyLimit,0);
	KEY_STRING  (schemaOwner, "");
	KEY_STRING  (goldSchemaOwner, "");
	KEY_FLOAT   (uniqueMessageCacheTimeSec, 30.0f);
	KEY_INT     (loaderThreads,4);
	KEY_INT     (persisterThreads,4);
	KEY_INT     (newCharacterThreads,4);
	KEY_INT     (characterImmediateDeleteMinutes,120);
	KEY_BOOL    (logObjectLoading, false);
	KEY_BOOL    (enableQueryProfile, false);
	KEY_BOOL    (verboseQueryMode, false);
	KEY_BOOL    (logWorkerThreads, true);
	KEY_STRING  (gameServiceBindInterface, "");
	KEY_BOOL    (reportSaveTimes, false);
	KEY_BOOL    (shouldSleep, true);
	KEY_BOOL    (enableLoadLocks, true);
	KEY_INT     (databaseReconnectTime, 1);
	KEY_BOOL    (logChunkLoading,false);
	KEY_BOOL    (useMemoryManagerForOCI,false);
	KEY_INT     (maxCharactersPerLoadRequest,40);
	KEY_INT     (maxChunksPerLoadRequest,400);
	KEY_FLOAT   (maxLoadStartDelay,200.0f);
	KEY_INT     (maxErrorCountBeforeDisconnect,5);
	KEY_INT     (maxErrorCountBeforeBailout,15);
	KEY_INT     (errorSleepTime,5000);
	KEY_INT     (disconnectSleepTime,30000);
	KEY_INT     (saveAtModulus,-1);
	KEY_INT     (saveAtDivisor,10);
	KEY_INT     (backloggedQueueSize,50);
	KEY_INT     (backloggedRecoveryQueueSize,25);
	KEY_INT     (maxTimewarp,data->saveFrequencyLimit * 2 < 600 ? 600 : data->saveFrequencyLimit * 2);
	KEY_BOOL    (enableObjvarPacking, true);
	KEY_INT     (prefetchNumRows,0);
	KEY_INT     (prefetchMemory,0);
	KEY_INT     (defaultFetchBatchSize,1000);
	KEY_INT     (queryReportingRate,60);
	KEY_INT     (enableDatabaseErrorLogging, 0);
	KEY_INT     (defaultMessageBulkBindSize, 1000);
	KEY_BOOL    (enableGoldDatabase, false);
	KEY_STRING  (maxGoldNetworkId, "10000000");
	KEY_FLOAT   (defaultQueueUpdateTimeLimit, 0.25f);
	KEY_BOOL    (enableDataCleanup, false);
	KEY_INT     (defaultLazyDeleteBulkBindSize, 100);
	KEY_INT     (defaultLazyDeleteSleepTime, 1000);
	KEY_INT     (writeDelay, 0);
	KEY_BOOL    (delayUnloadIfObjectStillHasData, true);
	KEY_FLOAT   (experienceConsolidationTime, 0.0f);
	KEY_INT     (maxLoaderFinishedTasks, 100);
	KEY_FLOAT   (reportLongFrameTime, 1.0f);
	KEY_BOOL    (enableVerboseMessageLogging, false);
	KEY_BOOL    (profilerExpandAll, false);
	KEY_INT     (profilerDisplayPercentageMinimum, 0);
	KEY_BOOL    (fatalOnDataError, false);
	KEY_INT     (maxUnackedLoadCount, 1000000000);  // by default, set to an "unlimited" number, and use maxUnackedLoadCountPerServer as the cap
	KEY_INT     (maxUnackedLoadCountPerServer, 10);
	KEY_INT     (auctionLocationLoadBatchSize, 100);
	KEY_INT     (auctionLoadBatchSize, 100);
	KEY_INT     (auctionAttributeLoadBatchSize, 100);
	KEY_INT     (auctionBidLoadBatchSize, 100);
	KEY_INT     (oldestUnackedLoadAlertThresholdSeconds, 10*60); // seconds
}

//-------------------------------------------------------------------

void ConfigServerDatabase::remove(void)
{
	ConfigServerUtility::remove();
}

// ----------------------------------------------------------------------

const NetworkId & ConfigServerDatabase::getMaxGoldNetworkId(void)
{
	static const NetworkId theValue(std::string(data->maxGoldNetworkId));
	return theValue;
}

// ======================================================================
