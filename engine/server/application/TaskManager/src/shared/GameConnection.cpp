// GameConnection.cpp
// Copyright 2000-01, Sony Online Entertainment Inc., all rights reserved. 
// Author: Justin Randall

//-----------------------------------------------------------------------

#include "FirstTaskManager.h"
#include "GameConnection.h"
#include "ConfigTaskManager.h"
#include "Archive/Archive.h"
#include "TaskManager.h"
#include "sharedFoundation/Clock.h"
#include "sharedLog/Log.h"
#include "sharedNetwork/NetworkHandler.h"
#include "sharedNetworkMessages/GenericValueTypeMessage.h"
#include "ProcessSpawner.h"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <algorithm>

//-----------------------------------------------------------------------

namespace GameConnectionNamespace
{
	std::vector<GameConnection *> s_gameConnections;
}

using namespace GameConnectionNamespace;

//-----------------------------------------------------------------------

GameConnection::GameConnection(TaskConnection * c) :
TaskHandler(),
connection(c),
m_lastKeepalive(0),
m_pid(0),
m_commandLine(),
m_timeLastKilled(0),
m_loggedKill(false),
m_loggedKillForceCore(false)
{
	TaskManager::addToGameConnections(1);
	s_gameConnections.push_back(this);
}

//-----------------------------------------------------------------------

GameConnection::~GameConnection()
{
	std::vector<GameConnection *>::iterator f = std::find(s_gameConnections.begin(), s_gameConnections.end(), this);
	if(f != s_gameConnections.end())
		s_gameConnections.erase(f);
	
	connection = 0;
	TaskManager::addToGameConnections(-1);
}

//-----------------------------------------------------------------------

void GameConnection::install()
{
}

//-----------------------------------------------------------------------

void GameConnection::remove()
{
	s_gameConnections.clear();
}

//-----------------------------------------------------------------------
void GameConnection::receive(const Archive::ByteStream &message) {
    Archive::ReadIterator r(message);
    GameNetworkMessage m(r);

    r = message.begin();
    if (m.isType("GameServerTaskManagerKeepAlive")) {
        if (!m_pid) {
            GenericValueTypeMessage<unsigned long> msg(r);
            m_pid = msg.getValue();

#ifndef _WIN32
            if (m_pid && m_commandLine.empty()) {
                // Construct the cmdline file path manually
                char filename[30];
                std::snprintf(filename, sizeof(filename), "/proc/%lu/cmdline", m_pid);

                // Open the cmdline file to read the command line parameters
                FILE *inFile = std::fopen(filename, "rb");
                if (inFile) {
                    char buffer[512];
                    std::size_t bytesRead = std::fread(buffer, 1, sizeof(buffer) - 1, inFile);
                    if (bytesRead > 0 && bytesRead < sizeof(buffer)) {
                        buffer[bytesRead] = '\0';  // Null-terminate the read data

                        // Replace '\0' characters with spaces for a readable command line
                        for (std::size_t i = 0; i < bytesRead; ++i) {
                            if (buffer[i] == '\0') {
                                buffer[i] = ' ';
                            }
                        }

                        m_commandLine = buffer;  // Store the processed command line
                    } else {
                        std::cerr << "Error reading cmdline file for PID: " << m_pid << std::endl;
                    }
                    std::fclose(inFile);
                } else {
                    std::cerr << "Failed to open cmdline file for PID: " << m_pid << std::endl;
                }
            }
#endif
        }
        m_lastKeepalive = Clock::timeSeconds();
    }
}

//-----------------------------------------------------------------------
void GameConnection::update() {
    const unsigned long currentTime = Clock::timeSeconds();
    const unsigned long gameServerTimeout = ConfigTaskManager::getGameServerTimeout();
    const unsigned long doubleTimeout = 2 * gameServerTimeout;  // For readability in comparisons
    const unsigned long forceCoreRetryInterval = 60;  // Interval in seconds for attempting `forceCore`

    // Only proceed if a timeout is configured
    if (gameServerTimeout > 0) {
        for (GameConnection* g : s_gameConnections) {
            // Check if the game server has timed out
            if (g->m_lastKeepalive && g->m_pid && ((g->m_lastKeepalive + gameServerTimeout) < currentTime)) {
                const unsigned long timeSinceLastKeepalive = currentTime - g->m_lastKeepalive;

                // Check if the server has exceeded double the timeout and should be killed
                if (timeSinceLastKeepalive > doubleTimeout) {
                    g->m_timeLastKilled = currentTime;
                    std::string host = NetworkHandler::getHumanReadableHostName();
                    const char* commandLine = g->m_commandLine.c_str();

                    LOG("ServerHang", ("killing (kill) GameServer %s,%d (%s) because it has not provided a keepalive message in %d seconds", 
                        host.c_str(), g->m_pid, commandLine, timeSinceLastKeepalive));
                    WARNING(true, ("killing (kill) GameServer %s,%d (%s) because it has not provided a keepalive message in %d seconds", 
                        host.c_str(), g->m_pid, commandLine, timeSinceLastKeepalive));

                    if (!g->m_loggedKill) {
                        g->m_loggedKill = true;
                        LOG("TaskManager", ("ServerHang: killing (kill) GameServer %s,%d (%s) because it has not provided a keepalive message in %d seconds", 
                            host.c_str(), g->m_pid, commandLine, timeSinceLastKeepalive));
                    }

                    ProcessSpawner::kill(g->m_pid);
                }
                // Attempt forceCore if last attempt was over a minute ago
                else if ((g->m_timeLastKilled == 0) || ((currentTime - g->m_timeLastKilled) >= forceCoreRetryInterval)) {
                    g->m_timeLastKilled = currentTime;
                    std::string host = NetworkHandler::getHumanReadableHostName();
                    const char* commandLine = g->m_commandLine.c_str();

                    LOG("ServerHang", ("killing (forceCore) GameServer %s,%d (%s) because it has not provided a keepalive message in %d seconds", 
                        host.c_str(), g->m_pid, commandLine, timeSinceLastKeepalive));
                    WARNING(true, ("killing (forceCore) GameServer %s,%d (%s) because it has not provided a keepalive message in %d seconds", 
                        host.c_str(), g->m_pid, commandLine, timeSinceLastKeepalive));

                    if (!g->m_loggedKillForceCore) {
                        g->m_loggedKillForceCore = true;
                        LOG("TaskManager", ("ServerHang: killing (forceCore) GameServer %s,%d (%s) because it has not provided a keepalive message in %d seconds", 
                            host.c_str(), g->m_pid, commandLine, timeSinceLastKeepalive));
                    }

                    ProcessSpawner::forceCore(g->m_pid);
                }
            }
        }
    }
}

//-----------------------------------------------------------------------
