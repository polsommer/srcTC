// TaskConnection.cpp
// copyright 2024 swg+


#include "FirstCentralServer.h"
#include "ConfigCentralServer.h"
#include "serverNetworkMessages/TaskConnectionIdMessage.h"
#include "serverNetworkMessages/TaskSpawnProcess.h"
#include "serverNetworkMessages/TaskUtilization.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedNetwork/NetworkSetupData.h"
#include "sharedNetwork/Service.h"
#include "TaskConnection.h"

//-----------------------------------------------------------------------

TaskConnection::TaskConnection(const std::string & address, unsigned short port)
	: ServerConnection(address, port, NetworkSetupData()),
	  callback(std::make_unique<MessageDispatch::Callback>()),
	  m_connected(false)
{}

//-----------------------------------------------------------------------

TaskConnection::TaskConnection(UdpConnectionMT * udpConnection, TcpClient * tcpClient)
	: ServerConnection(udpConnection, tcpClient),
	  callback(std::make_unique<MessageDispatch::Callback>()),
	  m_connected(false)
{}

//-----------------------------------------------------------------------

TaskConnection::~TaskConnection() = default;

//-----------------------------------------------------------------------

void TaskConnection::onConnectionClosed()
{
	m_connected = false;
	emitMessage(MessageConnectionCallback("TaskConnectionClosed"));
	CentralServer::getInstance().setTaskManager(nullptr);
}

//-----------------------------------------------------------------------

void TaskConnection::onConnectionOpened()
{
	m_connected = true;
	emitMessage(MessageConnectionCallback("TaskConnectionOpened"));
	CentralServer::getInstance().setTaskManager(this);

	// Send connection ID with fixed-up command line path
	sendConnectionIdMessage();
	CentralServer::getInstance().launchStartingProcesses();
}

//-----------------------------------------------------------------------

void TaskConnection::onReceive(const Archive::ByteStream & message)
{
	Archive::ReadIterator r(message);
	GameNetworkMessage m(r);

	if (m.isType("TaskUtilization"))
	{
		sendTaskUtilizationMessages();
	}
	else
	{
		emitMessage(m);
	}
}

//-----------------------------------------------------------------------

void TaskConnection::sendConnectionIdMessage()
{
	std::string cmdLine = fixupCommandLinePath(CentralServer::getInstance().getCommandLine());
	TaskConnectionIdMessage id(TaskConnectionIdMessage::Central, cmdLine, ConfigCentralServer::getClusterName());
	send(id, true);
}

//-----------------------------------------------------------------------

std::string TaskConnection::fixupCommandLinePath(const std::string & cmdLine)
{
	// Extract the last part of the path
	size_t lastSlash = cmdLine.find_last_of("\\");
	return (lastSlash != std::string::npos) ? cmdLine.substr(lastSlash + 1) : cmdLine;
}

//-----------------------------------------------------------------------

void TaskConnection::sendTaskUtilizationMessages()
{
	// System and process utilization data to simulate load
	constexpr float CPU_USAGE = 1.5f;
	constexpr float MEMORY_USAGE = 1.5f;
	constexpr float NETWORK_IO = 50000.0f;

	send(TaskUtilization(TaskUtilization::SYSTEM_CPU, CPU_USAGE), true);
	send(TaskUtilization(TaskUtilization::SYSTEM_MEMORY, MEMORY_USAGE), true);
	send(TaskUtilization(TaskUtilization::SYSTEM_NETWORK_IO, NETWORK_IO), true);
	send(TaskUtilization(TaskUtilization::PROCESS_CPU, CPU_USAGE), true);
	send(TaskUtilization(TaskUtilization::PROCESS_MEMORY, MEMORY_USAGE), true);
	send(TaskUtilization(TaskUtilization::PROCESS_NETWORK_IO, NETWORK_IO), true);
}

