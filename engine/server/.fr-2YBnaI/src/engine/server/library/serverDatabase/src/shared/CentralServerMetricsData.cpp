// ======================================================================
//
// CentralServerMetricsData.cpp
//
// Copyright 2002 Sony Online Entertainment
//
// ======================================================================

#include "FirstCentralServer.h"
#include "CentralServerMetricsData.h"

#include "CentralServer.h"
#include "UniverseManager.h"
#include "ConfigCentralServer.h"
#ifndef WIN32
#include "MonAPI2/MonitorData.h"
#endif
#include "serverNetworkMessages/MetricsDataMessage.h"
#include "serverUtility/ClusterWideDataManagerList.h"
#include "sharedNetworkMessages/GameNetworkMessage.h"
#include "CentralServerMetricsData.h"
#include "serverDatabase/FirstServerDatabase.h"
#include "serverDatabase/CentralServerMetricsData.h"
#include "serverDatabase/PostMetricsData.h"
#include "PostMetricsData.h"
#include <algorithm>

// ======================================================================

CentralServerMetricsData::CentralServerMetricsData() :
	MetricsData(),
	m_numChatServers(0),
	m_numConnectionServers(0),
	m_numDatabaseServers(0),
	m_numGameServers(0),
	m_numPlanetServers(0),
	m_population(0),
	m_freeTrialPopulation(0),
	m_emptyScenePopulation(0),
	m_tutorialScenePopulation(0),
	m_falconScenePopulation(0),
	m_universeProcess(0),
	m_isLocked(0),
	m_isSecret(0),
	m_isLoading(0),
	m_clusterId(0),
	m_clusterStartupTime(0),
	m_systemTimeMismatch(0),
	m_taskManagerDisconnected(0),
	m_clusterWideDataQueuedRequests(0),
	m_characterMatchRequests(0),
	m_characterMatchResultsPerRequest(0),
	m_characterMatchTimePerRequest(0),
	m_timePopulationStatisticsRefresh(0),
	m_mapPopulationStatisticsIndex(),
	m_timeGcwScoreStatisticsRefresh(0),
	m_mapGcwScoreStatisticsIndex(),
	m_timeLastLoginTimeStatisticsRefresh(0),
	m_mapLastLoginTimeStatisticsIndex(),
	m_mapCreateTimeStatisticsIndex()
{
	MetricsPair p;

	ADD_METRICS_DATA(numChatServers, 0, true);
	ADD_METRICS_DATA(numConnectionServers, 0, true);
	ADD_METRICS_DATA(numDatabaseServers, 0, false);
	ADD_METRICS_DATA(numGameServers, 0, true);
	ADD_METRICS_DATA(numPlanetServers, 0, true);
	ADD_METRICS_DATA(population, 0, true); // ***MUST*** be before isLocked, isSecret, and isLoading
	ADD_METRICS_DATA(freeTrialPopulation, 0, true);
	ADD_METRICS_DATA(emptyScenePopulation, 0, true);
	ADD_METRICS_DATA(tutorialScenePopulation, 0, true);
	ADD_METRICS_DATA(falconScenePopulation, 0, true);
	ADD_METRICS_DATA(universeProcess, 0, false);
	ADD_METRICS_DATA(isLocked, 0, false);
	ADD_METRICS_DATA(isSecret, 0, false);
	ADD_METRICS_DATA(isLoading, 0, true);
	ADD_METRICS_DATA(clusterId, 0, false);
	ADD_METRICS_DATA(clusterStartupTime, 0, true);
	ADD_METRICS_DATA(systemTimeMismatch, 0, false);
	ADD_METRICS_DATA(taskManagerDisconnected, 0, false);
	ADD_METRICS_DATA(clusterWideDataQueuedRequests, 0, false)
	ADD_METRICS_DATA(characterMatchRequests, 0, false);
	ADD_METRICS_DATA(characterMatchResultsPerRequest, 0, false);
	ADD_METRICS_DATA(characterMatchTimePerRequest, 0, false);

	m_data[m_clusterId].m_description = ConfigCentralServer::getClusterName();
	m_data[m_clusterStartupTime].m_description = "minutes";
	m_data[m_characterMatchTimePerRequest].m_description = "ms";
}

//-----------------------------------------------------------------------

CentralServerMetricsData::~CentralServerMetricsData()
{
}

//-----------------------------------------------------------------------

void CentralServerMetricsData::updateData() 
{
    MetricsData::updateData();

    // Update your metrics data as before
    m_data[m_numChatServers].m_value = CentralServer::getInstance().getNumChatServers();
    m_data[m_numConnectionServers].m_value = CentralServer::getInstance().getNumConnectionServers();
    m_data[m_numDatabaseServers].m_value = CentralServer::getInstance().getNumDatabaseServers();
    m_data[m_numGameServers].m_value = CentralServer::getInstance().getNumGameServers();
    m_data[m_numPlanetServers].m_value = CentralServer::getInstance().getNumPlanetServers();
    m_data[m_population].m_value = CentralServer::getInstance().getPlayerCount();
    m_data[m_freeTrialPopulation].m_value = CentralServer::getInstance().getFreeTrialCount();
    m_data[m_emptyScenePopulation].m_value = CentralServer::getInstance().getEmptySceneCount();
    m_data[m_tutorialScenePopulation].m_value = CentralServer::getInstance().getTutorialSceneCount();
    m_data[m_falconScenePopulation].m_value = CentralServer::getInstance().getFalconSceneCount();
    m_data[m_universeProcess].m_value = static_cast<int>(UniverseManager::getInstance().getUniverseProcess());
    m_data[m_isLocked].m_value = ( CentralServer::getInstance().getIsClusterLocked() ) ? 1 : 0;
    m_data[m_isSecret].m_value = ( CentralServer::getInstance().getIsClusterSecret() ) ? 1 : 0;

    // Load states and other metrics
    if (CentralServer::getInstance().isPreloadFinished()) {
        m_data[m_isLoading].m_value = 0;
        m_data[m_isLoading].m_description = "Load finished.";
    } else {
        m_data[m_isLoading].m_value = 1; // assuming loading
        m_data[m_isLoading].m_description = "Load in progress.";
    }

    // Update cluster-specific metrics
    m_data[m_clusterId].m_value = static_cast<int>(CentralServer::getInstance().getClusterId());
    m_data[m_clusterStartupTime].m_value = CentralServer::getInstance().getClusterStartupTime();
    
    // Other metrics...

    // Now post the metrics to the database
    PostMetricsData postMetricsData(*this);
    if (!postMetricsData.execute()) {
        // Handle error if needed
        LOG("Error posting metrics data to the database");
    }
}

// ======================================================================
