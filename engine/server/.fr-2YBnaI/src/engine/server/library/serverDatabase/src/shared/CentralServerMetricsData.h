//CentralServerMetricsData.h
//Copyright 2002 Sony Online Entertainment


#ifndef	_CentralServerMetricsData_H
#define	_CentralServerMetricsData_H

//-----------------------------------------------------------------------
#include "MetricsData.h" // Base class for metrics data
#include <map>
#include <string>
#include <time.h>

//-----------------------------------------------------------------------

class CentralServerMetricsData : public MetricsData
{
public:
    CentralServerMetricsData(); // Constructor
    virtual ~CentralServerMetricsData(); // Destructor

    void updateData(); // Method to update metrics data

    // Accessors for metrics data (getters)
    int getNumChatServers() const;
    int getNumConnectionServers() const;
    int getNumDatabaseServers() const;
    int getNumGameServers() const;
    int getNumPlanetServers() const;
    int getPopulation() const;
    int getFreeTrialPopulation() const;
    int getEmptyScenePopulation() const;
    int getTutorialScenePopulation() const;
    int getFalconScenePopulation() const;
    int getUniverseProcess() const;
    int getClusterId() const;
    int getClusterStartupTime() const;

private:
    // Metrics data members
    int m_numChatServers;
    int m_numConnectionServers;
    int m_numDatabaseServers;
    int m_numGameServers;
    int m_numPlanetServers;
    int m_population;
    int m_freeTrialPopulation;
    int m_emptyScenePopulation;
    int m_tutorialScenePopulation;
    int m_falconScenePopulation;
    int m_universeProcess;
    int m_isLocked;
    int m_isSecret;
    int m_isLoading;
    int m_clusterId;
    int m_clusterStartupTime;
    int m_systemTimeMismatch;
    int m_taskManagerDisconnected;
    int m_clusterWideDataQueuedRequests;
    int m_characterMatchRequests;
    int m_characterMatchResultsPerRequest;
    int m_characterMatchTimePerRequest;

    // Time metrics refresh
    time_t m_timePopulationStatisticsRefresh;
    time_t m_timeGcwScoreStatisticsRefresh;
    time_t m_timeLastLoginTimeStatisticsRefresh;

    // Maps to store indices for various statistics
    std::map<std::string, int> m_mapPopulationStatisticsIndex;
    std::map<std::string, int> m_mapGcwScoreStatisticsIndex;
    std::map<std::string, int> m_mapLastLoginTimeStatisticsIndex;
    std::map<std::string, int> m_mapCreateTimeStatisticsIndex;

    // Add any additional private methods or members if needed
};

// ======================================================================

#endif // CENTRALSERVERMETRICSDATA_H
