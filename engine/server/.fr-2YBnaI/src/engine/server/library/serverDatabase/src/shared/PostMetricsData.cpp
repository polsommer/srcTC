#include "serverDatabase/FirstServerDatabase.h"
#include "serverDatabase/PostMetricsData.h"
#include "serverDatabase/DatabaseProcess.h"
#include "serverDatabase/CentralServerMetricsData.h" // Include the metrics data class
#include "sharedFoundation/Clock.h"
#include "sharedDatabaseInterface/DbRow.h"
#include "sharedDatabaseInterface/DbTaskQueue.h"
using namespace DBQuery;

// ======================================================================

PostMetricsData::PostMetricsData(const CentralServerMetricsData &metricsData)
    : DB::Query(),
      m_metricsData(metricsData) 
{
}

// ----------------------------------------------------------------------

void PostMetricsData::getSQL(std::string &sql)
{
    sql = "begin " + DatabaseProcess::getInstance().getSchemaQualifier() + "metrics.insert_metrics_data("
          ":num_chat_servers, :num_connection_servers, :num_database_servers, :num_game_servers, "
          ":num_planet_servers, :population, :free_trial_population, :empty_scene_population, "
          ":tutorial_scene_population, :falcon_scene_population, :universe_process, :is_locked, "
          ":is_secret, :is_loading, :cluster_id, :cluster_startup_time, :system_time_mismatch, "
          ":task_manager_disconnected, :cluster_wide_data_queued_requests, :character_match_requests, "
          ":character_match_results_per_request, :character_match_time_per_request); end;";
}

// ----------------------------------------------------------------------

bool PostMetricsData::bindParameters()
{
    if (!bindParameter(m_metricsData.getNumChatServers())) return false;
    if (!bindParameter(m_metricsData.getNumConnectionServers())) return false;
    if (!bindParameter(m_metricsData.getNumDatabaseServers())) return false;
    if (!bindParameter(m_metricsData.getNumGameServers())) return false;
    if (!bindParameter(m_metricsData.getNumPlanetServers())) return false;
    if (!bindParameter(m_metricsData.getPopulation())) return false;
    if (!bindParameter(m_metricsData.getFreeTrialPopulation())) return false;
    if (!bindParameter(m_metricsData.getEmptyScenePopulation())) return false;
    if (!bindParameter(m_metricsData.getTutorialScenePopulation())) return false;
    if (!bindParameter(m_metricsData.getFalconScenePopulation())) return false;
    if (!bindParameter(m_metricsData.getUniverseProcess())) return false;
    if (!bindParameter(m_metricsData.getIsLocked())) return false;
    if (!bindParameter(m_metricsData.getIsSecret())) return false;
    if (!bindParameter(m_metricsData.getIsLoading())) return false;
    if (!bindParameter(m_metricsData.getClusterId())) return false;
    if (!bindParameter(m_metricsData.getClusterStartupTime())) return false;
    if (!bindParameter(m_metricsData.getSystemTimeMismatch())) return false;
    if (!bindParameter(m_metricsData.getTaskManagerDisconnected())) return false;
    if (!bindParameter(m_metricsData.getClusterWideDataQueuedRequests())) return false;
    if (!bindParameter(m_metricsData.getCharacterMatchRequests())) return false;
    if (!bindParameter(m_metricsData.getCharacterMatchResultsPerRequest())) return false;
    if (!bindParameter(m_metricsData.getCharacterMatchTimePerRequest())) return false;

    return true;
}

// ----------------------------------------------------------------------

bool PostMetricsData::bindColumns()
{
    // No columns to bind as this is an insert operation
    return true;
}

// ----------------------------------------------------------------------

DB::Query::QueryMode PostMetricsData::getExecutionMode() const
{
    return MODE_PROCEXEC;
}

// ----------------------------------------------------------------------

bool PostMetricsData::execute()
{
    return DB::Query::execute();
}

// ======================================================================
