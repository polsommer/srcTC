// TaskConnection.cpp
// Upgraded from original version
// Author: Justin Randall, Upgraded by OpenAI

#include "FirstPlanetServer.h"
#include "serverNetworkMessages/TaskConnectionIdMessage.h"
#include "serverNetworkMessages/TaskUtilization.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedNetwork/NetworkSetupData.h"
#include "TaskConnection.h"

//-----------------------------------------------------------------------

TaskConnection::TaskConnection(const std::string& address, unsigned short port)
    : ServerConnection(address, port, NetworkSetupData()) {}

//-----------------------------------------------------------------------

TaskConnection::TaskConnection(UdpConnectionMT* udpConnection, TcpClient* tcpClient)
    : ServerConnection(udpConnection, tcpClient) {}

//-----------------------------------------------------------------------

TaskConnection::~TaskConnection() = default;

//-----------------------------------------------------------------------

void TaskConnection::onConnectionClosed() {
    static MessageConnectionCallback callback("TaskConnectionClosed");
    emitMessage(callback);
    PlanetServer::getInstance().setTaskManager(nullptr);
}

//-----------------------------------------------------------------------

void TaskConnection::onConnectionOpened() {
    PlanetServer::getInstance().setTaskManager(this);

    // Retrieve cluster name from configuration
    std::string clusterName;
    if (const auto* section = ConfigFile::getSection("TaskManager")) {
        if (const auto* key = section->findKey("clusterName")) {
            clusterName = key->getAsString(key->getCount() - 1, "");
        }
    }

    // Send TaskConnectionIdMessage to the task manager
    TaskConnectionIdMessage id(TaskConnectionIdMessage::Planet, "", clusterName);
    send(id, true);

    static MessageConnectionCallback callback("TaskConnectionOpened");
    emitMessage(callback);
}

//-----------------------------------------------------------------------

void TaskConnection::onReceive(const Archive::ByteStream& message) {
    Archive::ReadIterator reader(message);
    GameNetworkMessage networkMessage(reader);

    if (networkMessage.isType("TaskUtilization")) {
        // Generate and send utilization messages for various metrics
        const float defaultCpuMemoryUtilization = 1.5f;
        const float defaultNetworkUtilization = 50000.0f;

        TaskUtilization sysCpu(static_cast<int>(TaskUtilization::SYSTEM_CPU), defaultCpuMemoryUtilization);
        TaskUtilization sysMem(static_cast<int>(TaskUtilization::SYSTEM_MEMORY), defaultCpuMemoryUtilization);
        TaskUtilization sysNet(static_cast<int>(TaskUtilization::SYSTEM_NETWORK_IO), defaultNetworkUtilization);
        TaskUtilization procCpu(static_cast<int>(TaskUtilization::PROCESS_CPU), defaultCpuMemoryUtilization);
        TaskUtilization procMem(static_cast<int>(TaskUtilization::PROCESS_MEMORY), defaultCpuMemoryUtilization);
        TaskUtilization procNet(static_cast<int>(TaskUtilization::PROCESS_NETWORK_IO), defaultNetworkUtilization);

        // Send each utilization message
        send(sysCpu, true);
        send(sysMem, true);
        send(sysNet, true);
        send(procCpu, true);
        send(procMem, true);
        send(procNet, true);
    }
}

