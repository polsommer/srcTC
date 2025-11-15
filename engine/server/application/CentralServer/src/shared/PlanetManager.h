// ======================================================================
//
// PlanetManager.h
// Copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_PlanetManager_H
#define INCLUDED_PlanetManager_H

// ======================================================================

// Standard headers
#include <map>
#include <string>
#include <vector>

// Singleton and messaging headers
#include "Singleton/Singleton.h"
#include "sharedMessageDispatch/Receiver.h"

// Forward declarations
class PlanetServerConnection;
class GameServerConnection;
class NetworkId;

// ======================================================================

/**
 * The PlanetManager is a singleton that manages information about
 * all the planets in the game universe.
 *
 * It maintains a map of scene IDs to Planet Server connections and keeps
 * track of the authoritative PlanetObjects for each planet.
 */

class PlanetManager : public MessageDispatch::Receiver
{
public:
    // Constructor
    PlanetManager();

    // Static methods for managing servers
    static void addServer(const std::string& sceneId, PlanetServerConnection* connection);
    static void addGameServerForScene(const std::string& sceneId, GameServerConnection* gameServer);
    static void onGameServerDisconnect(const GameServerConnection* gameServer);
    static PlanetServerConnection* getPlanetServerForScene(const std::string& sceneId);

    // Message reception handler
    void receiveMessage(const MessageDispatch::Emitter& source, const MessageDispatch::MessageBase& message);

private:
    // Inner structure for holding planet server data
    struct PlanetRec
    {
        PlanetServerConnection* m_connection;
        NetworkId m_planetObjectId;

        PlanetRec();
    };

    // Singleton instance accessor
    static PlanetManager& instance();

    // Type definitions for internal storage
    typedef std::map<std::string, PlanetRec> ServerListType;
    typedef std::vector<std::pair<std::string, GameServerConnection*>> PendingGameServersType;

    // Data members
    PendingGameServersType m_pendingGameServers;
    ServerListType m_servers;
};

// ======================================================================

#endif // INCLUDED_PlanetManager_H