#include "FirstCentralServer.h"
#include "PlanetServerConnection.h"

#include "ConsoleConnection.h"
#include "GameServerConnection.h"
#include "serverNetworkMessages/TaskSpawnProcess.h"
#include "serverNetworkMessages/CentralPlanetServerConnect.h"
#include "sharedNetworkMessages/ConsoleChannelMessages.h"
#include "sharedNetworkMessages/GenericValueTypeMessage.h"
#include "sharedFoundation/CrcConstexpr.hpp"

// ======================================================================

PlanetServerConnection::PlanetServerConnection(UdpConnectionMT *u, TcpClient *t)
	: ServerConnection(u, t),
	  m_gameServerConnectionAddress(""),
	  m_gameServerConnectionPort(0),
	  sceneId(),
	  m_forwardCounts(),
	  m_forwardDestinationServers(),
	  m_forwardMessages()
{}

// ----------------------------------------------------------------------

PlanetServerConnection::~PlanetServerConnection()
{
	onConnectionClosed();
}

// ----------------------------------------------------------------------

std::string const & PlanetServerConnection::getSceneId() const
{
	return sceneId;
}

// ----------------------------------------------------------------------

void PlanetServerConnection::onConnectionOpened()
{
	ServerConnection::onConnectionOpened();
	emitMessage(MessageConnectionCallback("PlanetServerConnectionOpened"));
}

// ----------------------------------------------------------------------

void PlanetServerConnection::onConnectionClosed()
{
	emitMessage(MessageConnectionCallback("PlanetServerConnectionClosed"));
	CentralServer::getInstance().removePlanetServer(this);
}

// ----------------------------------------------------------------------

void PlanetServerConnection::onReceive(Archive::ByteStream const &message)
{
	Archive::ReadIterator ri(message);
	GameNetworkMessage const msg(ri);
	ri = message.begin();

	const uint32 msgType = msg.getType();

	if (!m_forwardDestinationServers.empty() && handleForwardingMessages(msgType, message, ri))
		return;

	if (msgType == constcrc("BeginForward"))
	{
		handleBeginForward(ri);
		return;
	}

	ServerConnection::onReceive(message);
	processMessageType(msgType, ri);
}

// ----------------------------------------------------------------------

void PlanetServerConnection::setGameServerConnectionData(std::string const &address, uint16 port)
{
	m_gameServerConnectionAddress = address;
	m_gameServerConnectionPort = port;
}

// ----------------------------------------------------------------------

std::string const & PlanetServerConnection::getGameServerConnectionAddress() const
{
	return m_gameServerConnectionAddress;
}

// ----------------------------------------------------------------------

uint16 PlanetServerConnection::getGameServerConnectionPort() const
{
	return m_gameServerConnectionPort;
}

// ----------------------------------------------------------------------

void PlanetServerConnection::pushAndClearObjectForwarding()
{
	for (const auto &forwardedMessage : m_forwardMessages)
	{
		const Archive::ByteStream &msg = forwardedMessage.first;
		const auto &destinationServers = forwardedMessage.second;

		for (uint32 serverId : destinationServers)
		{
			GameServerConnection *conn = CentralServer::getInstance().getGameServer(serverId);
			if (conn)
				conn->Connection::send(msg, true);
		}
	}

	m_forwardMessages.clear();
}

// ----------------------------------------------------------------------
// Private Helper Functions

bool PlanetServerConnection::isMessageForwardable(uint32 msgType) const
{
	return msgType != constcrc("EndForward") && msgType != constcrc("BeginForward");
}

// Handles messages that are part of the forwarding system
bool PlanetServerConnection::handleForwardingMessages(uint32 msgType, Archive::ByteStream const &message, Archive::ReadIterator &ri)
{
	if (isMessageForwardable(msgType))
	{
		m_forwardMessages.emplace_back(message, m_forwardDestinationServers.back());
		return true;
	}

	if (msgType == constcrc("EndForward"))
	{
		if (--m_forwardCounts.back() == 0)
		{
			m_forwardCounts.pop_back();
			m_forwardDestinationServers.pop_back();

			if (m_forwardDestinationServers.empty())
				pushAndClearObjectForwarding();
		}
		return true;
	}

	if (msgType == constcrc("BeginForward"))
	{
		handleBeginForward(ri);
		return true;
	}

	return false;
}

// Handles the BeginForward message type
void PlanetServerConnection::handleBeginForward(Archive::ReadIterator &ri)
{
	GenericValueTypeMessage<std::vector<uint32>> beginForwardMessage(ri);

	const auto &destinations = beginForwardMessage.getValue();
	if (!m_forwardDestinationServers.empty() && destinations == m_forwardDestinationServers.back())
	{
		++m_forwardCounts.back();
	}
	else
	{
		m_forwardCounts.push_back(1);
		m_forwardDestinationServers.push_back(destinations);
	}
}

// Processes messages based on their type
void PlanetServerConnection::processMessageType(uint32 msgType, Archive::ReadIterator &ri)
{
	switch (msgType)
	{
		case constcrc("CentralPlanetServerConnect"):
		{
			CentralPlanetServerConnect id(ri);
			sceneId = id.getSceneId();
			break;
		}
		case constcrc("TaskSpawnProcess"):
		{
			TaskSpawnProcess spawn(ri);
			CentralServer::getInstance().sendTaskMessage(spawn);
			break;
		}
		case constcrc("ConGenericMessage"):
		{
			ConGenericMessage con(ri);
			ConsoleConnection::onCommandComplete(con.getMsg(), static_cast<int>(con.getMsgId()));
			break;
		}
	}
}

// ======================================================================

