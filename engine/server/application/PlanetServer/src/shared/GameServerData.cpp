// ======================================================================
//
// GameServerData.cpp
// Upgraded from the original version
//
// ======================================================================

#include "FirstPlanetServer.h"
#include "GameServerData.h"

#include "ConfigPlanetServer.h"
#include "GameServerConnection.h"
#include "PlanetServer.h"
#include "sharedLog/Log.h"

// ======================================================================

GameServerData::GameServerData(GameServerConnection* connection)
    : m_connection(connection),
      m_objectCount(0),
      m_interestObjectCount(0),
      m_interestCreatureObjectCount(0),
      m_serverStatus(SS_unready) {}

// ----------------------------------------------------------------------

GameServerData::GameServerData(const GameServerData& rhs)
    : m_connection(nullptr),  // Connection pointer is intentionally not copied
      m_objectCount(rhs.m_objectCount),
      m_interestObjectCount(rhs.m_interestObjectCount),
      m_interestCreatureObjectCount(rhs.m_interestCreatureObjectCount),
      m_serverStatus(rhs.m_serverStatus) {}

// ----------------------------------------------------------------------

GameServerData::GameServerData()
    : m_connection(nullptr),
      m_objectCount(0),
      m_interestObjectCount(0),
      m_interestCreatureObjectCount(0),
      m_serverStatus(SS_unready) {}

// ----------------------------------------------------------------------

GameServerData& GameServerData::operator=(const GameServerData& rhs) {
    if (this != &rhs) {
        // Connection pointer is not copied; it remains managed by this instance
        m_connection = nullptr;
        m_objectCount = rhs.m_objectCount;
        m_interestObjectCount = rhs.m_interestObjectCount;
        m_interestCreatureObjectCount = rhs.m_interestCreatureObjectCount;
        m_serverStatus = rhs.m_serverStatus;
    }
    return *this;
}

// ----------------------------------------------------------------------

void GameServerData::universeLoaded() {
    WARNING_DEBUG_FATAL(m_serverStatus != SS_readyForObjects,
                        ("Received UniverseLoaded for server %d in an invalid state: expected \"readyForObjects\" state.", getProcessId()));
    m_serverStatus = SS_loadedUniverseObjects;
}

// ----------------------------------------------------------------------

void GameServerData::ready() {
    WARNING_DEBUG_FATAL(m_serverStatus != SS_unready,
                        ("Received GameServerReady for server %d in an invalid state: expected \"unready\" state.", getProcessId()));
    m_serverStatus = SS_readyForObjects;
}

// ----------------------------------------------------------------------

void GameServerData::preloadComplete() {
    WARNING_DEBUG_FATAL(PlanetServer::getInstance().getEnablePreload() && m_serverStatus != SS_loadedUniverseObjects,
                        ("Received preloadComplete for server %d in an invalid state: expected \"loadedUniverseObjects\" state.", getProcessId()));
    m_serverStatus = SS_running;
}

// ----------------------------------------------------------------------

uint32 GameServerData::getProcessId() const {
    NOT_NULL(m_connection);
    return m_connection ? m_connection->getProcessId() : 0;
}

// ----------------------------------------------------------------------

void GameServerData::debugOutputData() const {
    LOG("LoadBalancing", ("Server %d: %i objects, %i interest objects", getProcessId(), getObjectCount(), getInterestObjectCount()));
}

