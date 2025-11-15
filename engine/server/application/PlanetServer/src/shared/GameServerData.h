// ======================================================================
//
// GameServerData.h
// Upgraded from the original version
//
// ======================================================================

#ifndef INCLUDED_GameServerData_H
#define INCLUDED_GameServerData_H

// Forward declaration
class GameServerConnection;

class GameServerData
{
public:
    // Enumerations
    enum ServerStatus { SS_unready, SS_readyForObjects, SS_loadedUniverseObjects, SS_running };

    // Constructors and assignment operator
    explicit GameServerData(GameServerConnection* connection);
    GameServerData();
    GameServerData(const GameServerData& rhs);
    GameServerData& operator=(const GameServerData& rhs);

    // Accessors
    GameServerConnection* getConnection();
    int                   getObjectCount() const;
    int                   getInterestObjectCount() const;
    int                   getInterestCreatureObjectCount() const;
    ServerStatus          getServerStatus() const;
    uint32                getProcessId() const;
    bool                  isRunning() const;

    // Debugging
    void                  debugOutputData() const;

    // Status management
    void                  universeLoaded();
    void                  ready();
    void                  preloadComplete();

    // Object count adjustments
    void                  adjustObjectCount(int adjustment);
    void                  adjustInterestObjectCount(int adjustment);
    void                  adjustInterestCreatureObjectCount(int adjustment);

private:
    GameServerConnection* m_connection;
    int                   m_objectCount;
    int                   m_interestObjectCount;
    int                   m_interestCreatureObjectCount;
    ServerStatus          m_serverStatus;
};

// Inline implementations

inline GameServerConnection* GameServerData::getConnection()
{
    return m_connection;
}

inline int GameServerData::getObjectCount() const
{
    return m_objectCount;
}

inline void GameServerData::adjustObjectCount(int adjustment)
{
    m_objectCount += adjustment;
}

inline void GameServerData::adjustInterestObjectCount(int adjustment)
{
    m_interestObjectCount += adjustment;
}

inline GameServerData::ServerStatus GameServerData::getServerStatus() const
{
    return m_serverStatus;
}

inline int GameServerData::getInterestObjectCount() const
{
    return m_interestObjectCount;
}

inline bool GameServerData::isRunning() const
{
    return m_serverStatus == SS_running;
}

inline int GameServerData::getInterestCreatureObjectCount() const
{
    return m_interestCreatureObjectCount;
}

inline void GameServerData::adjustInterestCreatureObjectCount(int adjustment)
{
    m_interestCreatureObjectCount += adjustment;
}

#endif  // INCLUDED_GameServerData_H

