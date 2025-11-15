// TaskConnection.cpp
// Copyright 2000-01, Sony Online Entertainment Inc., all rights reserved.
// Author: Justin Randall

#include "FirstTaskManager.h"
#include "CentralConnection.h"
#include "ConfigTaskManager.h"
#include "DatabaseConnection.h"
#include "Locator.h"
#include "GameConnection.h"
#include "ManagerConnection.h"
#include "MetricsServerConnection.h"
#include "PlanetConnection.h"
#include "TaskConnection.h"
#include "TaskManager.h"
#include "serverNetworkMessages/ExcommunicateGameServerMessage.h"
#include "serverNetworkMessages/TaskConnectionIdMessage.h"
#include "serverNetworkMessages/TaskKillProcess.h"
#include "sharedNetwork/NetworkSetupData.h"
#include "sharedFoundation/CrcConstexpr.hpp"

//-----------------------------------------------------------------------

TaskConnection::TaskConnection(const std::string &address, const unsigned short port)
    : Connection(address, port, NetworkSetupData()),
      handler(nullptr)
{
}

//-----------------------------------------------------------------------

TaskConnection::TaskConnection(UdpConnectionMT *udpConn, TcpClient *tcpClient)
    : Connection(udpConn, tcpClient),
      handler(nullptr)
{
}

//-----------------------------------------------------------------------

TaskConnection::~TaskConnection()
{
    delete handler;
}

//-----------------------------------------------------------------------

void TaskConnection::onConnectionClosed()
{
    static Closed msg;
    closed.emitMessage(msg);
}

//-----------------------------------------------------------------------

void TaskConnection::onConnectionOpened()
{
    DEBUG_REPORT_LOG(true, ("Task Connection opened %s\n", getRemoteAddress().c_str()));
    static Opened msg;
    opened.emitMessage(msg);
}

//-----------------------------------------------------------------------

void TaskConnection::onConnectionOverflowing(const unsigned int bytesPending)
{
    static Overflowing msg;
    msg.bytesPending = bytesPending;
    overflowing.emitMessage(msg);
}

//-----------------------------------------------------------------------

void TaskConnection::onReceive(const Archive::ByteStream &message)
{
    static Receive msg;
    if (handler)
        handler->receive(message);

    Archive::ReadIterator iter(message);
    GameNetworkMessage networkMessage(iter);
    const uint32 messageType = networkMessage.getType();

    switch (messageType)
    {
        case constcrc("TaskConnectionIdMessage"):
        {
            iter = message.begin();
            TaskConnectionIdMessage t(iter);

            FATAL(ConfigTaskManager::getVerifyClusterName() &&
                  TaskManager::getNodeLabel() == "node0" &&
                  t.getServerType() == TaskConnectionIdMessage::TaskManager &&
                  t.getClusterName() != ConfigTaskManager::getClusterName(),
                  ("Remote TaskManager %s (%s) reported different cluster name: %s != %s",
                   t.getCommandLine().c_str(), getRemoteAddress().c_str(),
                   t.getClusterName().c_str(), ConfigTaskManager::getClusterName()));

            Identified identifiedMsg = {this, t.getServerType()};
            identified.emitMessage(identifiedMsg);

            switch (identifiedMsg.id)
            {
                case TaskConnectionIdMessage::Central:
                    REPORT_LOG(true, ("New Central Server connection active\n"));
                    handler = new CentralConnection(this, t.getCommandLine());
                    TaskManager::setCentralConnection(this);
                    break;
                case TaskConnectionIdMessage::Game:
                    REPORT_LOG(true, ("New Game Server connection active\n"));
                    handler = new GameConnection(this);
                    break;
                case TaskConnectionIdMessage::Database:
                    REPORT_LOG(true, ("New Database Server connection active\n"));
                    handler = new DatabaseConnection();
                    break;
                case TaskConnectionIdMessage::Metrics:
                    REPORT_LOG(true, ("New Metrics Server connection active\n"));
                    handler = new MetricsServerConnection(this, t.getCommandLine());
                    break;
                case TaskConnectionIdMessage::Planet:
                    REPORT_LOG(true, ("New Planet Server connection active\n"));
                    handler = new PlanetConnection(this);
                    break;
                default:
                    WARNING_STRICT_FATAL(true, ("Unknown id (%d) received on task connection", identifiedMsg.id));
                    break;
            }
            break;
        }
        case constcrc("ExcommunicateGameServerMessage"):
        {
            iter = message.begin();
            ExcommunicateGameServerMessage ex(iter);
            TaskKillProcess killMsg(ex.getHostName(), ex.getProcessId(), true);
            TaskManager::killProcess(killMsg);

            Locator::sendToAllTaskManagers(killMsg);  // Broadcast to other task managers
            break;
        }
        case constcrc("TaskKillProcess"):
        {
            iter = message.begin();
            TaskKillProcess killProcess(iter);
            TaskManager::killProcess(killProcess);
            break;
        }
    }

    msg.message = &message;
    receiveMessage.emitMessage(msg);
}

//-----------------------------------------------------------------------

void TaskConnection::send(const GameNetworkMessage &message)
{
    Archive::ByteStream byteStream;
    message.pack(byteStream);
    Connection::send(byteStream, true);
}

//-----------------------------------------------------------------------
// Get transceiver functions

MessageDispatch::Transceiver<const Closed &> &TaskConnection::getTransceiverClosed()
{
    return closed;
}

MessageDispatch::Transceiver<const Failed &> &TaskConnection::getTransceiverFailed()
{
    return failed;
}

MessageDispatch::Transceiver<const Identified &> &TaskConnection::getTransceiverIdentified()
{
    return identified;
}

MessageDispatch::Transceiver<const Opened &> &TaskConnection::getTransceiverOpened()
{
    return opened;
}

MessageDispatch::Transceiver<const Overflowing &> &TaskConnection::getTransceiverOverflowing()
{
    return overflowing;
}

MessageDispatch::Transceiver<const Receive &> &TaskConnection::getTransceiverReceive()
{
    return receiveMessage;
}

MessageDispatch::Transceiver<const Reset &> &TaskConnection::getTransceiverReset()
{
    return reset;
}

//-----------------------------------------------------------------------

