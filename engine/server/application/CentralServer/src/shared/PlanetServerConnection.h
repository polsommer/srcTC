// ======================================================================
//
// PlanetServerConnection.h
// copyright 2024 swg+
// ======================================================================

#ifndef _PlanetServerConnection_H
#define _PlanetServerConnection_H

// ======================================================================

#include "serverUtility/ServerConnection.h"
#include <string>
#include <vector>
#include <utility>  // for std::pair
#include <cstdint>

// ======================================================================

class PlanetServerConnection : public ServerConnection
{
public:
	PlanetServerConnection(UdpConnectionMT *udpConnection, TcpClient *tcpClient);
	~PlanetServerConnection() override;

	void onConnectionOpened() override;
	void onConnectionClosed() override;
	void onReceive(Archive::ByteStream const &message) override;

	void setGameServerConnectionData(std::string const &address, uint16 port);
	std::string const &getGameServerConnectionAddress() const;
	uint16 getGameServerConnectionPort() const;
	std::string const &getSceneId() const;

private:
	// Disable copy and assignment
	PlanetServerConnection(PlanetServerConnection const &) = delete;
	PlanetServerConnection &operator=(PlanetServerConnection const &) = delete;

	// Helper functions for internal operations
	bool isMessageForwardable(uint32 msgType) const;
	bool handleForwardingMessages(uint32 msgType, Archive::ByteStream const &message, Archive::ReadIterator &ri);
	void handleBeginForward(Archive::ReadIterator &ri);
	void processMessageType(uint32 msgType, Archive::ReadIterator &ri);
	void pushAndClearObjectForwarding();

	// Member variables
	std::string m_gameServerConnectionAddress;
	uint16 m_gameServerConnectionPort;
	std::string sceneId;
	std::vector<int> m_forwardCounts;
	std::vector<std::vector<uint32>> m_forwardDestinationServers;
	std::vector<std::pair<Archive::ByteStream, std::vector<uint32>>> m_forwardMessages;
};

// ======================================================================

#endif // _PlanetServerConnection_H

