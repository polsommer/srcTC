// TaskManager.cpp
// Copyright 2000-01, Sony Online Entertainment Inc., all rights reserved.
// Author: Justin Randall

//-----------------------------------------------------------------------
#include "FirstTaskManager.h"
#include "CentralConnection.h"
#include "ConfigTaskManager.h"
#include "Console.h"
#include "ConsoleConnection.h"
#include "GameConnection.h"
#include "Locator.h"
#include "ManagerConnection.h"
#include "ProcessSpawner.h"
#include "serverNetworkMessages/SetConnectionServerPublic.h"
#include "serverNetworkMessages/TaskKillProcess.h"
#include "serverNetworkMessages/TaskProcessDiedMessage.h"
#include "serverNetworkMessages/TaskSpawnProcess.h"
#include "fileInterface/StdioFile.h"
#include "sharedFoundation/Clock.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedLog/Log.h"
#include "sharedLog/SetupSharedLog.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedNetwork/Address.h"
#include "sharedNetwork/NetworkSetupData.h"
#include "sharedNetwork/Service.h"
#include "sharedNetworkMessages/GenericValueTypeMessage.h"
#include "EnvironmentVariable.h"
#include "TaskConnection.h"
#include "TaskManager.h"
#include "TaskManagerSysInfo.h"
#include <unordered_map>
#include <sstream>
#include <memory>

namespace TaskManagerNameSpace
{
	std::vector<ProcessEntry> ms_deferredSpawns;
	int ms_numGameConnections = 0;
	bool ms_doUpdate = false;
	TaskManager *gs_instance = nullptr;
	int ms_idleFrames = 0;
	bool ms_preloadFinished = false;
	bool ms_done = false;

	std::unordered_map<std::string, TaskManager::SpawnDelaySeconds> s_processRestartDelayInformation;

	void initializeRestartDelayInformation()
	{
		s_processRestartDelayInformation = {
			{"CentralServer", ConfigTaskManager::getRestartDelayCentralServer()},
			{"LogServer", ConfigTaskManager::getRestartDelayLogServer()},
			{"MetricsServer", ConfigTaskManager::getRestartDelayMetricsServer()},
			{"CommoditiesServer", ConfigTaskManager::getRestartDelayCommoditiesServer()},
			{"CommodityServer", ConfigTaskManager::getRestartDelayCommoditiesServer()},
			{"TransferServer", ConfigTaskManager::getRestartDelayTransferServer()}
		};
	}

	TaskManager::SpawnDelaySeconds getRestartDelayInformation(const std::string &name)
	{
		auto it = std::find_if(s_processRestartDelayInformation.begin(), s_processRestartDelayInformation.end(),
			[&name](const auto &pair) { return name.find(pair.first) != std::string::npos; });
		return (it != s_processRestartDelayInformation.end()) ? it->second : 0;
	}

	std::unordered_map<std::string, float> s_processLoadInformation;

	void initializeLoadInformation()
	{
		s_processLoadInformation = {
			{"ConnectionServer", ConfigTaskManager::getLoadConnectionServer()},
			{"PlanetServer", ConfigTaskManager::getLoadPlanetServer()},
			{"SwgGameServer", ConfigTaskManager::getLoadGameServer()}
		};
	}

	float getLoadForProcess(const std::string &name)
	{
		auto it = std::find_if(s_processLoadInformation.begin(), s_processLoadInformation.end(),
			[&name](const auto &pair) { return name.find(pair.first) != std::string::npos; });
		return (it != s_processLoadInformation.end()) ? it->second : 0.0f;
	}

	struct RestartRequest
	{
		std::string commandLine;
		unsigned long timeQueued;
		TaskManager::SpawnDelaySeconds spawnDelay;
	};
	std::vector<RestartRequest> s_restartRequests;

	struct QueuedSpawnRequest
	{
		std::string processName;
		std::string options;
		std::string nodeLabel;
		unsigned long timeQueued;
		TaskManager::SpawnDelaySeconds spawnDelay;
	};

	std::vector<QueuedSpawnRequest> s_queuedSpawnRequests;
	std::vector<QueuedSpawnRequest> s_delayedSpawnRequests;

	struct OutstandingSpawnRequestAck
	{
		std::string nodeLabel;
		Archive::ByteStream request;
		int transactionId;
		OutstandingSpawnRequestAck(const std::string &n, const Archive::ByteStream &a, int t)
			: nodeLabel(n), request(a), transactionId(t) {}
	};

	std::vector<OutstandingSpawnRequestAck> s_outstandingSpawnRequestAcks;
} // namespace TaskManagerNameSpace

using namespace TaskManagerNameSpace;
//-----------------------------------------------------------------------

TaskManager::NodeEntry::NodeEntry(const std::string &addr, const std::string &label, int index)
	: m_address(addr), m_nodeLabel(label), m_nodeNumber(index) {}

bool TaskManager::NodeEntry::operator==(const NodeEntry &rhs) const
{
	return m_nodeNumber == rhs.m_nodeNumber;
}

bool TaskManager::NodeEntry::operator==(const std::string &address) const
{
	return m_address == address;
}
//-----------------------------------------------------------------------


TaskManager::TaskManager() :
m_processEntries(),
m_localServers(),
m_remoteServers(),
m_nodeLabel(),
m_startTime(::time(nullptr)),
m_nodeList(),
m_nodeNumber(-1),
m_nodeToConnectToList(),
m_managerService(0),
m_taskService(0),
m_sysInfoSource(new TaskManagerSysInfo),
m_centralConnection(0)
{
	processRcFile();
	processEnvironmentVariables();
	setupNodeList();
}

//-----------------------------------------------------------------------

TaskManager::~TaskManager()
{
	delete m_sysInfoSource;
}

//-----------------------------------------------------------------------


void TaskManager::processEnvironmentVariables()
{
	const int numEnvVars = ConfigTaskManager::getNumberOfEnvironmentVariables();
	for (int i = 0; i < numEnvVars; ++i)
	{
		const char *envVar = ConfigTaskManager::getEnvironmentVariable(i);
		if (envVar)
		{
			const char *plus = strchr(envVar, '+');
			const char *value = strchr(envVar, '=');
			if (!value)
			{
				WARNING(true, ("Missing equal sign in environment variable %s", envVar));
				continue;
			}
			std::string key(envVar, (plus ? plus : value) - envVar);
			++value;

			if (plus)
			{
				if (!EnvironmentVariable::addToEnvironmentVariable(key.c_str(), value))
				{
					WARNING(true, ("Failed to add environment variable %s", envVar));
				}
			}
			else
			{
				if (!EnvironmentVariable::setEnvironmentVariable(key.c_str(), value))
				{
					WARNING(true, ("Failed to set environment variable %s", envVar));
				}
			}
		}
	}
}

//-----------------------------------------------------------------------

void TaskManager::install()
{
	gs_instance = new TaskManager();
}

//-----------------------------------------------------------------------

void TaskManager::remove()
{
	delete gs_instance;
}

//-----------------------------------------------------------------------

void TaskManager::processRcFile()
{
    // Open the file using a unique pointer for automatic resource management
    auto file = std::make_unique<StdioFile>(ConfigTaskManager::getRcFileName(), "r");

    if (file && file->isOpen())
    {
        // Read the file content into a string buffer
        const int fileLength = file->length();
        if (fileLength > 0)
        {
            std::string buffer(fileLength, '\0');
            if (file->read(&buffer[0], fileLength) > 0)
            {
                std::istringstream rcData(buffer);
                std::string record;

                while (std::getline(rcData, record))
                {
                    // Ignore comments and empty lines
                    const auto firstChar = record.find_first_not_of(" \t");
                    if (firstChar == std::string::npos || record[firstChar] == '#')
                    {
                        continue;
                    }

                    ProcessEntry pe;
                    std::istringstream recordStream(record);

                    // Parse the server name
                    recordStream >> pe.processName;
                    if (pe.processName.empty())
                    {
                        REPORT_LOG(true, ("Missing server name in rc entry: [%s]\n", record.c_str()));
                        continue;
                    }

                    // Parse the target host directive
                    recordStream >> pe.targetHost;
                    if (pe.targetHost.empty())
                    {
                        REPORT_LOG(true, ("Missing target host in rc entry: [%s]\n", record.c_str()));
                        continue;
                    }
                    if (pe.targetHost != "any" && pe.targetHost != "local")
                    {
                        Address a(pe.targetHost, 0);
                        pe.targetHost = a.getHostAddress();
                    }

                    // Parse the executable name
                    recordStream >> pe.executable;
                    if (pe.executable.empty())
                    {
                        REPORT_LOG(true, ("Missing executable name in rc entry: [%s]\n", record.c_str()));
                        continue;
                    }

                    // Parse the options (if any)
                    std::getline(recordStream >> std::ws, pe.options);

                    // Insert the parsed entry into process entries map
                    m_processEntries[pe.processName] = std::move(pe);
                }
            }
            else
            {
                REPORT_LOG(true, ("Failed to read file content: %s\n", ConfigTaskManager::getRcFileName()));
            }
        }
        file->close();
    }
    else
    {
        REPORT_LOG(true, ("Could not open rc file: %s\n", ConfigTaskManager::getRcFileName()));
    }
}

//-----------------------------------------------------------------------

void TaskManager::setupNodeList()
{
	char buffer[64];
	const char* result = nullptr;
	int nodeIndex = 0;
	bool found = true;

	const std::string & localAddress = NetworkHandler::getHostName();
	const std::string localName = NetworkHandler::getHumanReadableHostName();
	
	m_nodeNumber = -1;

	do
	{
		found = false;
		sprintf(buffer, "node%d", nodeIndex);
		result = ConfigFile::getKeyString("TaskManager", buffer, nullptr);
		if (result)
		{
			NodeEntry n(result, buffer, nodeIndex);
			m_nodeList.push_back(n);;
			found = true;
			if (result == localAddress || result == localName)
			{
				m_nodeNumber = nodeIndex;
				m_nodeLabel = buffer;
			}
			++nodeIndex;
		}

	} while(found);

	if (m_nodeList.empty())
	{
		m_nodeLabel = "node0";
		m_nodeNumber = 0;
	}

	if (m_nodeNumber != -1)
	{
		DEBUG_REPORT_LOG(true, ("This taskmanager is node %s\n", m_nodeLabel.c_str()));
	}
	else
	{
		WARNING(true, ("Could not find node for this host: %s.", localAddress.c_str()));
	}

	std::vector<NodeEntry>::iterator i = m_nodeList.begin();
	for (; i != m_nodeList.end(); ++i)
	{
		if (m_nodeNumber > i->m_nodeNumber)
		{
			DEBUG_REPORT_LOG(true, ("Adding node %d to attempt list\n", i->m_nodeNumber));
			m_nodeToConnectToList.push_back(*i);
		}
	}
}

//-----------------------------------------------------------------------

std::string TaskManager::executeCommand(const std::string & command)
{
	std::string result = "unknown command";

	if(command == "start")
	{
		startCluster();
		result = "start command issued and handled by the TaskManager";
	}
	else if(command == "stop")
	{
		stopCluster();
		result = "stop command issued and handled by the TaskManager";
	}
	else if(command == "public")
	{
		SetConnectionServerPublic p(true);
		sendToCentralServer(p);
		result = "public command issued and handled by the TaskManager";
	}
	else if(command == "private")
	{
		SetConnectionServerPublic p(false);
		sendToCentralServer(p);
		result = "private command issued and handled by the TaskManager";
	}
	else if(command == "taskConnectionCount")
	{
		char countBuf[32];
		IGNORE_RETURN(snprintf(countBuf, sizeof(countBuf)-1, "%d", ManagerConnection::getConnectionCount()));
		countBuf[sizeof(countBuf)-1] = '\0';
		result = countBuf;
	}
	else if(command == "exit")
	{
		stopCluster();
		ms_done = true;
		result = "exiting";
	}
	else if(command == "runState")
	{
		result = "running";
	}
	LOG("TaskManager",("Execute command: %s.", result.c_str()));
	return result;
}

//-----------------------------------------------------------------------

const float TaskManager::getLoadAverage()
{
	return Locator::getMyLoad();
}

//-----------------------------------------------------------------------

TaskManager & TaskManager::instance()
{
	NOT_NULL(gs_instance);
	return *gs_instance;
}

//-----------------------------------------------------------------------

void TaskManager::killProcess(const TaskKillProcess &killProcessMessage)
{
    // Check if the host is local or matches the current node label
    if (!NetworkHandler::isAddressLocal(killProcessMessage.getHostName()) && killProcessMessage.getHostName() != instance().m_nodeLabel)
    {
        return;
    }

    // Iterate over local servers to find and kill the specified process
    for (auto it = instance().m_localServers.begin(); it != instance().m_localServers.end();)
    {
        const auto &[commandLine, pid] = *it;
        if (pid == killProcessMessage.getPid())
        {
            // Execute the appropriate kill command
            killProcessMessage.getForceCore() ? ProcessSpawner::forceCore(pid) : ProcessSpawner::kill(pid);

            // Emit a kill notification message
            MessageDispatch::Transceiver<const ProcessKilled &> transceiver;
            ProcessKilled killedMessage;
            killedMessage.commandLine = commandLine;
            killedMessage.hostName = killProcessMessage.getHostName();
            transceiver.emitMessage(killedMessage);

            // Update local servers and decrement load
            it = instance().m_localServers.erase(it);
            Locator::decrementMyLoad(getLoadForProcess(commandLine));
        }
        else
        {
            ++it;
        }
    }
}


//-----------------------------------------------------------------------
void TaskManager::run()
{
    // Install the NetworkHandler and set up logging
    NetworkHandler::install();
    SetupSharedLog::install("TaskManager");
    Locator::install();
    initializeRestartDelayInformation();
    initializeLoadInformation();

    // Setup for Task Service
    NetworkSetupData taskSetup;
    taskSetup.port = ConfigTaskManager::getGameServicePort();
    taskSetup.maxConnections = 1000;
    taskSetup.bindInterface = ConfigTaskManager::getGameServiceBindInterface();
    instance().m_taskService = new Service(ConnectionAllocator<TaskConnection>(), taskSetup);

    // Setup for Manager Service
    NetworkSetupData managerSetup;
    managerSetup.port = ConfigTaskManager::getTaskManagerServicePort();
    managerSetup.maxConnections = 32;
    managerSetup.bindInterface = ConfigTaskManager::getTaskManagerServiceBindInterface();
    instance().m_managerService = new Service(ConnectionAllocator<ManagerConnection>(), managerSetup);

    // Setup for Console Service
    NetworkSetupData consoleSetup;
    consoleSetup.port = ConfigTaskManager::getConsoleConnectionPort();
    consoleSetup.maxConnections = 32;
    consoleSetup.bindInterface = ConfigTaskManager::getConsoleServiceBindInterface();
    instance().m_consoleService = new Service(ConnectionAllocator<ConsoleConnection>(), consoleSetup);

    // Auto-start the cluster if enabled in configuration
    if (ConfigTaskManager::getAutoStart())
    {
        startCluster();
    }

    // Main loop: continue running updates until the done flag is set
    while (!ms_done)
    {
        TaskManager::update();
    }

    // Ensure cleanup after main loop ends
    NetworkHandler::update();
    NetworkHandler::dispatch();

    // Cleanup: remove installed components in reverse order of initialization
    SetupSharedLog::remove();
    NetworkHandler::remove();
}


//-----------------------------------------------------------------------

unsigned long TaskManager::startServerLocal(const ProcessEntry &pe, const std::string &options)
{
    // Build the command string from executable, options, and additional options
    std::string cmd = pe.executable + " " + pe.options + " " + options;

    // Log details about the spawning process
    DEBUG_REPORT_LOG(true, ("Starting process: %s\n", cmd.c_str()));
    LOG("TaskManager", ("Starting process (%s) on host %s (%s) with current load %f.",
                       cmd.c_str(),
                       NetworkHandler::getHumanReadableHostName().c_str(),
                       NetworkHandler::getHostName().c_str(),
                       Locator::getMyLoad()));

    // Execute the command and get the process ID
    unsigned long pid = ProcessSpawner::execute(cmd);
    if (pid > 0)
    {
        // Update local load information
        Locator::incrementMyLoad(getLoadForProcess(pe.processName));

        // Insert process into local servers map
        instance().m_localServers.insert({cmd, pid});

        // Emit a process started message
        MessageDispatch::Transceiver<const ProcessStarted &> transceiver;
        ProcessStarted startedMessage;
        startedMessage.pid = pid;
        startedMessage.hostName = NetworkHandler::getHostName();
        startedMessage.commandLine = cmd;
        transceiver.emitMessage(startedMessage);
    }

    return pid;
}


//-----------------------------------------------------------------------
/** Start a new process

	Processes are read from the taskmanager.rc file. A remote process
	or the local task manager requests a process startup. The process name
	must match one in the taskmanager.rc file. The rc contains default
	options for the local system. Options passed in the request are
	appended to the command line and duplicates will override the
	options specified in the rc file.
*/
unsigned long TaskManager::startServer(const std::string &processName, const std::string &options, const std::string &nodeLabel, const SpawnDelaySeconds spawnDelay)
{
    unsigned long pid = 0;
    const std::string localAddress = NetworkHandler::getHostName();

    DEBUG_REPORT_LOG(true, ("Attempting to start server %s %s on %s, with spawn delay %u\n", processName.c_str(), options.c_str(), nodeLabel.c_str(), spawnDelay));
    DEBUG_REPORT_LOG(true, ("Local address is %s and current node is %s\n", localAddress.c_str(), getNodeLabel().c_str()));

    // Find the process entry for the requested process name
    auto it = instance().m_processEntries.find(processName);
    if (it == instance().m_processEntries.end()) {
        REPORT_LOG(true, ("Process name %s not found in task manager configuration\n", processName.c_str()));
        return 0;
    }

    const ProcessEntry &pe = it->second;

    // If a spawn delay is specified, queue the spawn request
    if (spawnDelay > 0) {
        QueuedSpawnRequest queuedRequest = {processName, options, nodeLabel, Clock::timeSeconds(), spawnDelay};
        s_delayedSpawnRequests.push_back(queuedRequest);
        return 0;
    }

    // Check if the process should run on the local machine
    if (pe.targetHost == "local" || pe.targetHost == getNodeLabel() || nodeLabel == getNodeLabel() || nodeLabel == "local") {
        // Check if we can start locally or if we should forward the request to the master node due to load constraints
        if (processName == "SwgGameServer" && Locator::getMyLoad() + getLoadForProcess(processName) >= Locator::getMyMaximumLoad() && ManagerConnection::getConnectionCount() > 1) {
            ManagerConnection *conn = Locator::getServer("node0");
            if (conn) {
                WARNING(true, ("Attempted to spawn SwgGameServer on this host but exceeded load limit. Requesting master node to spawn."));
                TaskSpawnProcess spawnRequest("any", processName, options);
                conn->send(spawnRequest);
            } else {
                FATAL(true, ("Could not find master node to forward spawn request for %s due to load limit exceeded.", processName.c_str()));
            }
        } else {
            pid = startServerLocal(pe, options);
        }
    } else if (pe.targetHost == "any") {
        // Distributed spawning across any available node
        if (nodeLabel == "any" && getNodeLabel() == "node0") {
            float cost = getLoadForProcess(pe.processName);
            ManagerConnection *conn = Locator::getBestServer(pe.processName, options, cost);
            if (conn) {
                std::string label = conn->getNodeLabel() ? *conn->getNodeLabel() : "local";
                TaskSpawnProcess spawnRequest(label, pe.processName, options);
                conn->send(spawnRequest);
                Locator::incrementServerLoad(label, cost);
                LOG("TaskManager", ("Spawn request for %s sent to %s with load cost %f/%f.", processName.c_str(), label.c_str(), Locator::getServerLoad(label), Locator::getServerMaximumLoad(label)));

                // Queue the request for acknowledgment
                Archive::ByteStream bs;
                spawnRequest.pack(bs);
                s_outstandingSpawnRequestAcks.emplace_back(label, bs, spawnRequest.getTransactionId());
            } else if (ManagerConnection::getConnectionCount() < 1) {
                pid = startServerLocal(pe, options);
            } else {
                // Defer the spawn if no suitable host is found
                WARNING(true, ("No available hosts to spawn process %s with cost %f, deferring request.", processName.c_str(), cost));
                s_queuedSpawnRequests.push_back({processName, options, nodeLabel, Clock::timeSeconds(), spawnDelay});
            }
        } else {
            // Forward the spawn request to the master node for handling
            ManagerConnection *conn = Locator::getServer("node0");
            if (conn) {
                LOG("TaskManager", ("Forwarding spawn request for %s to master node due to 'any' configuration.", processName.c_str()));
                TaskSpawnProcess spawnRequest("any", pe.processName, options);
                conn->send(spawnRequest);
            }
        }
    } else {
        // Spawning on a specific node
        ManagerConnection *conn = Locator::getServer(pe.targetHost);
        if (conn) {
            TaskSpawnProcess spawnRequest(pe.targetHost, pe.processName, options);
            conn->send(spawnRequest);
            Locator::incrementServerLoad(pe.targetHost, getLoadForProcess(pe.processName));
            LOG("TaskManager", ("Spawn request sent to %s for %s. Load: %f/%f.", pe.targetHost.c_str(), processName.c_str(), Locator::getServerLoad(pe.targetHost), Locator::getServerMaximumLoad(pe.targetHost)));

            // Queue acknowledgment
            Archive::ByteStream bs;
            spawnRequest.pack(bs);
            s_outstandingSpawnRequestAcks.emplace_back(pe.targetHost, bs, spawnRequest.getTransactionId());
        } else {
            // Defer if target node is not available
            REPORT_LOG(true, ("Could not spawn %s on %s; node is unavailable. Deferring request.", pe.processName.c_str(), nodeLabel.c_str()));
            ms_deferredSpawns.push_back(pe);
        }
    }

    return pid;
}

//-----------------------------------------------------------------------

void TaskManager::retryConnection(ManagerConnection const *connection)
{
	std::vector<NodeEntry>::iterator i = std::find(instance().m_nodeList.begin(), instance().m_nodeList.end(), connection->getRemoteAddress());
	if (i != instance().m_nodeList.end())
	{
		instance().m_nodeToConnectToList.push_back(*i);
	}
	else
	{
		WARNING(true, ("could not find node for %s after it closed connection", connection->getRemoteAddress().c_str()));
	}
}

//-----------------------------------------------------------------------

void TaskManager::runSpawnRequestQueue()
{
	if (!s_queuedSpawnRequests.empty())
	{
		// create a temporary vector to iterate
		std::vector<QueuedSpawnRequest> const temp = s_queuedSpawnRequests;

		// clear the request queue vector
		// if a process can't spawn, it will be re-added to
		// the vector the the next run of the request queue
		s_queuedSpawnRequests.clear();

		for (std::vector<QueuedSpawnRequest>::const_iterator i = temp.begin(); i != temp.end(); ++i)
			IGNORE_RETURN(startServer(i->processName, i->options, i->nodeLabel, 0));
	}
}

//-----------------------------------------------------------------------

void TaskManager::update()
{
	NetworkHandler::update();
	NetworkHandler::dispatch();

	Console::update();

	while(Console::hasPendingCommand())
	{
		IGNORE_RETURN(executeCommand(Console::getNextCommand()));
	}

	Clock::setFrameRateLimit(4.0f);

	static uint32 lastTime = 0;
	uint32 currentTime = Clock::timeMs();
	if (!instance().m_nodeToConnectToList.empty() && currentTime > lastTime + 1000)
	{
		std::vector<NodeEntry>::iterator i = instance().m_nodeToConnectToList.begin();
		for(; i != instance().m_nodeToConnectToList.end(); ++i)
		{
			new ManagerConnection(i->m_address, ConfigTaskManager::getTaskManagerServicePort());
		}
		instance().m_nodeToConnectToList.clear();
		lastTime = currentTime;
	}

	// get process status
	std::set<std::pair<std::string, unsigned long> >::iterator i;
	for(i = instance().m_localServers.begin(); i != instance().m_localServers.end();)
	{
		if(! ProcessSpawner::isProcessActive((*i).second))
		{
			LOG("TaskProcessDied", ("PROCESS DIED: %s", i->first.c_str()));
			// advise the master node that a process has died
			TaskProcessDiedMessage died(i->second, i->first);

			// is this the master node?
			if(getNodeLabel() == "node0")
			{
				// tell CentralServer (if it is still around), that the process died
				if(instance().m_centralConnection)
				{
					LOG("TaskProcessDied", ("advising central that the process is dead"));
					instance().m_centralConnection->send(died);
				}
			}
			else
			{
				ManagerConnection * master = Locator::getServer("node0");
				if(master)
				{
					LOG("TaskProcessDied", ("advising master node that the process is dead"));
					master->send(died);
				}
			}
			// destroy process
			Locator::decrementMyLoad(getLoadForProcess(i->first));
			ProcessAborted a;
			a.commandLine = (*i).first;
			a.hostName = NetworkHandler::getHostName();
			MessageDispatch::emitMessage<const ProcessAborted &>(a);

			if (!fopen(".norestart","r"))
			{
				// respawn CentralServer or LogServer
				if (   (a.commandLine.find("CentralServer") != std::string::npos && ConfigTaskManager::getRestartCentral())
				    || a.commandLine.find("LogServer") != std::string::npos
				    || a.commandLine.find("MetricsServer") != std::string::npos
				    || a.commandLine.find("CommoditiesServer") != std::string::npos
				    || a.commandLine.find("CommodityServer") != std::string::npos 
				    || a.commandLine.find("TransferServer") != std::string::npos)
				{
					RestartRequest r;
					r.commandLine = (*i).first;
					r.timeQueued = Clock::timeSeconds();
					r.spawnDelay = getRestartDelayInformation((*i).first);

					s_restartRequests.push_back(r);
				}
				instance().m_localServers.erase(i++);
				if(i == instance().m_localServers.end())
					break;
			}
		}
		else
			++i;
	}

	if (!s_restartRequests.empty())
	{
		for (std::vector<RestartRequest>::iterator i = s_restartRequests.begin(); i != s_restartRequests.end(); ++i)
		{
			if ((i->spawnDelay == 0) || ((i->timeQueued + i->spawnDelay) < Clock::timeSeconds()))
			{
				unsigned long pid = ProcessSpawner::execute((i->commandLine));
				Locator::incrementMyLoad(getLoadForProcess(i->commandLine));
				IGNORE_RETURN(instance().m_localServers.insert(std::make_pair((i->commandLine), pid)));

				IGNORE_RETURN(s_restartRequests.erase(i));

				// just do 1 per frame to spread out the load
				break;
			}
		}
	}

	if (!ms_deferredSpawns.empty())
	{
		// flush deferred spawns
		std::vector<ProcessEntry>::const_iterator procIter;
		std::vector<ProcessEntry> failures;
		const std::vector<ProcessEntry> & def = ms_deferredSpawns;

		for(procIter = def.begin(); procIter != def.end(); ++procIter)
		{
			ManagerConnection* conn = Locator::getServer(procIter->targetHost);
			if (conn)
			{
				TaskSpawnProcess spawn((*procIter).targetHost, (*procIter).processName, (*procIter).options);
				conn->send(spawn);
				REPORT_LOG(true, ("Sent deferred spawn request for %s to %s\n", (*procIter).processName.c_str(), (*procIter).targetHost.c_str()));
			}
			else
			{
				failures.push_back((*procIter));
			}
		}

		ms_deferredSpawns = failures;
	}

	if (!s_delayedSpawnRequests.empty())
	{
		for (std::vector<QueuedSpawnRequest>::iterator i = s_delayedSpawnRequests.begin(); i != s_delayedSpawnRequests.end(); ++i)
		{
			if ((i->timeQueued + i->spawnDelay) < Clock::timeSeconds())
			{
				IGNORE_RETURN(startServer(i->processName, i->options, i->nodeLabel, 0));
				IGNORE_RETURN(s_delayedSpawnRequests.erase(i));

				// just do 1 per frame to spread out the load
				break;
			}
		}
	}

	// slave TaskManager will periodically send the system time to the
	// the TaskManager to detect when/if system time gets out of sync

	// master TaskManager will periodically send to CentralServer the list
	// of slave TaskManager that has disconnected but has not reconnected,
	// so that an alert can be made in SOEMon so ops can see it and restart
	// the disconnected TaskManager
	static time_t timeSystemTimeCheck = ::time(nullptr) + ConfigTaskManager::getSystemTimeCheckIntervalSeconds();
	time_t const timeNow = ::time(nullptr);
	if (timeSystemTimeCheck <= timeNow)
	{
		if (getNodeLabel() != "node0")
		{
			ManagerConnection * master = Locator::getServer("node0");
			if (master)
			{
				GenericValueTypeMessage<std::pair<std::string, long > > msg("SystemTimeCheck", std::make_pair(getNodeLabel(), static_cast<long>(timeNow)));
				master->send(msg);
			}
		}
		else
		{
			std::string disconnectedTaskManagerList;
			std::map<std::string, std::string> const & disconnectedTaskManager = Locator::getClosedConnections();
			if (!disconnectedTaskManager.empty())
			{
				char buffer[128];
				for (std::map<std::string, std::string>::const_iterator iter = disconnectedTaskManager.begin(); iter != disconnectedTaskManager.end(); ++iter)
				{
					snprintf(buffer, sizeof(buffer)-1, "%s (%s)", iter->first.c_str(), iter->second.c_str());
					buffer[sizeof(buffer)-1] = '\0';

					if (!disconnectedTaskManagerList.empty())
						disconnectedTaskManagerList += ", ";

					disconnectedTaskManagerList += buffer;
				}
			}

			GenericValueTypeMessage<std::string> disconnectedTaskManagerMessage("DisconnectedTaskManagerMessage", disconnectedTaskManagerList);
			TaskManager::sendToCentralServer(disconnectedTaskManagerMessage);
		}

		timeSystemTimeCheck = timeNow + ConfigTaskManager::getSystemTimeCheckIntervalSeconds();
	}

	GameConnection::update();
	Clock::update();
	Clock::limitFrameRate();
}

//-----------------------------------------------------------------------

void TaskManager::sendToCentralServer(const GameNetworkMessage & message)
{
	if(instance().m_centralConnection)
	{
		instance().m_centralConnection->send(message);
	}
}

//-----------------------------------------------------------------------

void TaskManager::setCentralConnection(TaskConnection * newConnection)
{
	instance().m_centralConnection = newConnection;
}

//-----------------------------------------------------------------------

void TaskManager::addToGameConnections(int x)
{
	ms_numGameConnections += x;
	ms_doUpdate = true;
}

//-----------------------------------------------------------------------
int TaskManager::getNumGameConnections()
{
	return ms_numGameConnections;
}

//-----------------------------------------------------------------------

void TaskManager::onDatabaseIdle(bool isIdle)
{
	if (!ms_preloadFinished)
		return;

	if (isIdle)
	{
		++ms_idleFrames;
		if (ConfigTaskManager::getPublishMode() && ms_idleFrames>5) // wait for a few idle frames in a row, just to be safe
		{
			DEBUG_REPORT_LOG(true,("Preloading is done and database is idle.  Shutting down cluster\n"));
			stopCluster();
			ms_done = true;
		}
	}
	else
	{
		ms_idleFrames=0;
	}
}

// ----------------------------------------------------------------------

void TaskManager::startCluster()
{
	IGNORE_RETURN(TaskManager::startServer("TransferServer", "", "local", 0));
	IGNORE_RETURN(TaskManager::startServer("MetricsServer", "", "local", 0));
	IGNORE_RETURN(TaskManager::startServer("LogServer", "", "local", 0));
	std::string options = "-s CentralServer loginServerAddress=";
	options += ConfigTaskManager::getLoginServerAddress();
	options += " clusterName=" + std::string(ConfigTaskManager::getClusterName());
	IGNORE_RETURN(TaskManager::startServer("CentralServer", options, "local", 0));
}

// ----------------------------------------------------------------------

void TaskManager::stopCluster()
{
	std::set<std::pair<std::string, unsigned long> >::iterator i;
	for(i = instance().m_localServers.begin(); i != instance().m_localServers.end(); ++i)
	{
		ProcessSpawner::kill((*i).second);
	}
	instance().m_localServers.clear();
}

// ----------------------------------------------------------------------

void TaskManager::onPreloadFinished()
{
	ms_preloadFinished = true;
}

// ----------------------------------------------------------------------

void TaskManager::resendUnacknowledgedSpawnRequests(Connection * connection, const std::string & nodeLabel)
{
	std::vector<OutstandingSpawnRequestAck>::iterator i;
	for(i = s_outstandingSpawnRequestAcks.begin(); i != s_outstandingSpawnRequestAcks.end(); ++i)
	{
		if(i->nodeLabel == nodeLabel)
		{
			connection->send(i->request, true);
		}
	}
}

// ----------------------------------------------------------------------

void TaskManager::removePendingSpawnProcessAck(int transactionId)
{
	std::vector<OutstandingSpawnRequestAck>::iterator i;
	for(i = s_outstandingSpawnRequestAcks.begin(); i != s_outstandingSpawnRequestAcks.end();)
	{
		if(i->transactionId == transactionId)
		{
			i = s_outstandingSpawnRequestAcks.erase(i);
		}
		else
		{
			++i;
		}
	}
}
// ======================================================================
