// ======================================================================
//                          ChatServerConnection.h
//                    Last Updated: 2024-11-04
//                    Copyright © 2024 SWG+ Project
//                    Author: Justin Randall
// ======================================================================

#ifndef _INCLUDED_ChatServerConnection_H
#define _INCLUDED_ChatServerConnection_H

//-----------------------------------------------------------------------

#include "serverUtility/ServerConnection.h"

//-----------------------------------------------------------------------

class ChatServerConnection : public ServerConnection
{
public:
    explicit ChatServerConnection(UdpConnectionMT* udpConnection, TcpClient* tcpClient);
    ~ChatServerConnection() override;

    unsigned short getGameServicePort() const;
    void           onConnectionClosed() override;
    void           onConnectionOpened() override;
    void           onReceive(const Archive::ByteStream& message) override;
    void           setGameServicePort(unsigned short gameServicePort);

private:
    ChatServerConnection& operator=(const ChatServerConnection& rhs) = delete;
    ChatServerConnection(const ChatServerConnection& source) = delete;

    unsigned short gameServicePort;

}; // lint !e1712 default constructor not defined

//-----------------------------------------------------------------------

#endif // _INCLUDED_ChatServerConnection_H
