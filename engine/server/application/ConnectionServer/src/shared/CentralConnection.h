// CentralConnection.h
// copyright 2000 Verant Interactive
// Author: Justin Randall

#ifndef _CentralConnection_H
#define _CentralConnection_H

//-----------------------------------------------------------------------

#include "serverUtility/ServerConnection.h"
#include "Archive/ByteStream.h"   // Required for Archive::ByteStream
#include <string>

//-----------------------------------------------------------------------

class CentralConnection : public ServerConnection
{
public:
	CentralConnection(const std::string & address, unsigned short port);
	CentralConnection(UdpConnectionMT * udpConnection, TcpClient * tcpClient);
	~CentralConnection() override;

	void onConnectionClosed() override;
	void onConnectionOpened() override;
	void onReceive(const Archive::ByteStream & message) override;

private:
	// Disable copy constructor and assignment operator
	CentralConnection(const CentralConnection&) = delete;
	CentralConnection& operator=(const CentralConnection&) = delete;

	// Helper methods to process specific message types
	void handleTransferLoginCharacterToSourceServer(Archive::ReadIterator & ri, const Archive::ByteStream & message);
	void handleTransferLoginCharacterToDestinationServer(Archive::ReadIterator & ri, const Archive::ByteStream & message);
	void handleCtsSrcCharWrongPlanet(Archive::ReadIterator & ri, const Archive::ByteStream & message);
	void handleTransferKickConnectedClients(Archive::ReadIterator & ri);
	void handleTransferClosePseudoClientConnection(Archive::ReadIterator & ri);
	void handleLoginDeniedRecentCTS(Archive::ReadIterator & ri);
	void handleLoginDeniedPendingPlayerRenameRequest(Archive::ReadIterator & ri);
};

//-----------------------------------------------------------------------

#endif // _CentralConnection_H

