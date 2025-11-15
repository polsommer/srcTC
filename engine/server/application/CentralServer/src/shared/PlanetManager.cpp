#include "FirstCentralServer.h"
#include "PlanetManager.h"

#include "ConfigCentralServer.h"
#include "GameServerConnection.h"
#include "PlanetServerConnection.h"
#include "serverNetworkMessages/PlanetObjectIdMessage.h"
#include "serverNetworkMessages/RequestPlanetObjectIdMessage.h"
#include "serverNetworkMessages/SetPlanetServerMessage.h"
#include "serverNetworkMessages/TaskSpawnProcess.h"
#include "sharedNetwork/NetworkHandler.h"

#include "sharedFoundation/CrcConstexpr.hpp"

#include <algorithm> // For std::remove_if and std::find_if

// ======================================================================

PlanetManager::PlanetRec::PlanetRec() :
    m_connection(nullptr),
    m_planetObjectId(NetworkId::cms_invalid)
{
}

// ----------------------------------------------------------------------

PlanetManager::PlanetManager() :
    MessageDispatch::Receiver(),
    m_pendingGameServers(),
    m_servers()
{
    connectToMessage("PlanetObjectIdMessage");
    connectToMessage("PlanetServerConnectionClosed");
}

// ----------------------------------------------------------------------
// Revert to raw pointer as per the header file
void PlanetManager::addServer(const std::string& sceneId, PlanetServerConnection* connection)
{
    auto& servers = instance().m_servers;
    auto& pendingServers = instance().m_pendingGameServers;

    servers[sceneId].m_connection = connection;
    DEBUG_REPORT_LOG(true, ("Added Planet Server for scene %s\n", sceneId.c_str()));

    auto it = std::remove_if(pendingServers.begin(), pendingServers.end(),
        [&sceneId](const auto& pair) {
            return pair.first == sceneId;
        });

    for (auto i = it; i != pendingServers.end(); ++i) {
        addGameServerForScene(sceneId, i->second);
    }
    pendingServers.erase(it, pendingServers.end());
}

// ----------------------------------------------------------------------

void PlanetManager::addGameServerForScene(const std::string& sceneId, GameServerConnection* gameServer)
{
    if (sceneId.empty())
    {
        WARNING_STRICT_FATAL(true, ("Ignoring request to add game server with empty scene id"));
        if (gameServer)
        {
            gameServer->disconnect();
        }
        return;
    }

    auto& servers = instance().m_servers;
    auto it = servers.find(sceneId);

    if (it == servers.end())
    {
        CentralServer::getInstance().startPlanetServer("any", sceneId, 0);
        instance().m_pendingGameServers.emplace_back(sceneId, gameServer);
    }
    else
    {
        const auto* planetServer = it->second.m_connection;
        SetPlanetServerMessage msg(planetServer->getGameServerConnectionAddress(), planetServer->getGameServerConnectionPort());
        gameServer->send(msg, true);
    }
}

// ----------------------------------------------------------------------

void PlanetManager::receiveMessage(const MessageDispatch::Emitter& source, const MessageDispatch::MessageBase& message)
{
    const uint32_t messageType = message.getType();

    switch (messageType) {
        case constcrc("PlanetObjectIdMessage"):
        {
            auto ri = static_cast<const GameNetworkMessage&>(message).getByteStream().begin();
            PlanetObjectIdMessage msg(ri);

            auto& servers = instance().m_servers;
            auto it = servers.find(msg.getSceneId());

            if (it == servers.end())
            {
                WARNING_STRICT_FATAL(true, ("Unexpected PlanetObjectIdMessage for %s.\n", msg.getSceneId().c_str()));
                return;
            }

            it->second.m_planetObjectId = msg.getPlanetObject();
            break;
        }
        case constcrc("PlanetServerConnectionClosed"):
        {
            const auto* c = static_cast<const PlanetServerConnection*>(&source);
            auto& servers = instance().m_servers;

            auto it = std::find_if(servers.begin(), servers.end(),
                [c](const auto& pair) {
                    return pair.second.m_connection == c;  // No need for .get() since m_connection is a raw pointer
                });

            if (it != servers.end()) {
                servers.erase(it);
            }

            if (ConfigCentralServer::getRequestDbSaveOnPlanetServerCrash())
            {
                GameNetworkMessage msg("CentralRequestSave");
                CentralServer::getInstance().sendToDBProcess(msg, true);
            }

            CentralServer::getInstance().startPlanetServer(
                CentralServer::getInstance().getHostForScene(c->getSceneId()), 
                c->getSceneId(), 
                ConfigCentralServer::getPlanetServerRestartDelayTimeSeconds()
            );
            break;
        }
        default:
            WARNING_STRICT_FATAL(true, ("Planet Manager received unexpected message.\n"));
            break;
    }
}

// ----------------------------------------------------------------------

PlanetServerConnection* PlanetManager::getPlanetServerForScene(const std::string& sceneId)
{
    auto& servers = instance().m_servers;
    auto it = servers.find(sceneId);

    return (it != servers.end()) ? it->second.m_connection : nullptr;
}

// ----------------------------------------------------------------------

void PlanetManager::onGameServerDisconnect(const GameServerConnection* gameServer)
{
    auto& pendingServers = instance().m_pendingGameServers;

    auto it = std::remove_if(pendingServers.begin(), pendingServers.end(),
        [gameServer](const auto& pair) {
            return pair.second == gameServer;
        });

    pendingServers.erase(it, pendingServers.end());
}

//-----------------------------------------------------------------------

PlanetManager& PlanetManager::instance()
{
    static PlanetManager instance;
    return instance;
}

// ======================================================================

