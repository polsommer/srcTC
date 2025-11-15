// ======================================================================
//                          ChatServerConnection.cpp
//                    Last Updated: 2024-11-04
//                    Copyright © 2024 SWG+ Project
//                    Author: Justin Randall
// ======================================================================

#include "FirstCentralServer.h"
#include "ChatServerConnection.h"

//-----------------------------------------------------------------------

ChatServerConnection::ChatServerConnection(UdpConnectionMT* u, TcpClient* t)
    : ServerConnection(u, t),
      gameServicePort(0)
{
}

//-----------------------------------------------------------------------

ChatServerConnection::~ChatServerConnection() = default;

//-----------------------------------------------------------------------

unsigned short ChatServerConnection::getGameServicePort() const
{
    return gameServicePort;
}

//-----------------------------------------------------------------------

void ChatServerConnection::onConnectionOpened()
{
    ServerConnection::onConnectionOpened();
    static const MessageConnectionCallback connectionOpenedMessage("ChatServerConnectionOpened");
    emitMessage(connectionOpenedMessage);
}

//-----------------------------------------------------------------------

void ChatServerConnection::onConnectionClosed()
{
    ServerConnection::onConnectionClosed();
    static const MessageConnectionCallback connectionClosedMessage("ChatServerConnectionClosed");
    emitMessage(connectionClosedMessage);
}

//-----------------------------------------------------------------------

void ChatServerConnection::onReceive(const Archive::ByteStream& message)
{
    ServerConnection::onReceive(message);
}

//-----------------------------------------------------------------------

void ChatServerConnection::setGameServicePort(unsigned short port)
{
    gameServicePort = port;
}

//-----------------------------------------------------------------------

