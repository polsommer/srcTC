// CentralConnection.h
// Copyright 2000-01, Sony Online Entertainment Inc., all rights reserved.
// Author: Justin Randall

#ifndef INCLUDED_CentralConnection_H
#define INCLUDED_CentralConnection_H

//-----------------------------------------------------------------------

#include "sharedMessageDispatch/Transceiver.h"
#include "TaskHandler.h"
#include <string>

class TaskConnection;
struct Closed;
struct ProcessKilled;

//-----------------------------------------------------------------------

class CentralConnection : public TaskHandler
{
public:
    CentralConnection(TaskConnection* newConnection, const std::string& commandLine);
    ~CentralConnection();

    void closed(const Closed& closedEvent);
    void receive(const Archive::ByteStream& message) override;

private:
    // Disable copy constructor and assignment operator
    CentralConnection(const CentralConnection&) = delete;
    CentralConnection& operator=(const CentralConnection&) = delete;

    void onProcessKilled(const ProcessKilled& killedEvent);

private:
    MessageDispatch::Callback callback;
    std::string commandLine;
    bool connected;
    TaskConnection* connection;
    unsigned long lastSpawnTime;
};

//-----------------------------------------------------------------------

#endif // INCLUDED_CentralConnection_H

