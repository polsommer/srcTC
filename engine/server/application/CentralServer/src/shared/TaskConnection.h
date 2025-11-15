// TaskConnection.h
// copyright 2024 swg+


#ifndef _TaskConnection_H
#define _TaskConnection_H

//-----------------------------------------------------------------------

#include "serverUtility/ServerConnection.h"
#include <memory>

class TaskCommandChannel;

namespace MessageDispatch
{
	class Callback;
}

//-----------------------------------------------------------------------

class TaskConnection : public ServerConnection
{
public:
	TaskConnection(const std::string & remoteAddress, unsigned short remotePort);
	TaskConnection(UdpConnectionMT * u, TcpClient * t);
	~TaskConnection();

	void onConnectionClosed() override;
	void onConnectionOpened() override;
	void onReceive(const Archive::ByteStream & message) override;
	bool isConnected() const;

private:
	TaskConnection(const TaskConnection&) = delete;
	TaskConnection& operator=(const TaskConnection&) = delete;

	// Helper functions for internal operations
	void sendConnectionIdMessage();
	std::string fixupCommandLinePath(const std::string & cmdLine);
	void sendTaskUtilizationMessages();

	std::unique_ptr<MessageDispatch::Callback> callback;
	bool m_connected;

}; //lint !e1712 default constructor not defined

//-----------------------------------------------------------------------

inline bool TaskConnection::isConnected() const
{
	return m_connected;
}

//-----------------------------------------------------------------------

#endif // _TaskConnection_H

